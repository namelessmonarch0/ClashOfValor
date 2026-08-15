#include "SaveStore.h"

#include <sqlite3.h>

#include <iostream>
#include <string>

#include "Assassin.h"
#include "Character.h"
#include "Sorcerer.h"
#include "Warrior.h"

using namespace std;

namespace
{

const char *SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS characters ("
    "  name          TEXT PRIMARY KEY,"
    "  class         TEXT    NOT NULL CHECK(class IN ('Warrior','Sorcerer','Assassin')),"
    "  health        INTEGER NOT NULL,"
    "  max_health    INTEGER NOT NULL,"
    "  stamina       INTEGER NOT NULL,"
    "  level         INTEGER NOT NULL,"
    "  coins         INTEGER NOT NULL,"
    "  xp            INTEGER NOT NULL,"
    "  health_potion INTEGER NOT NULL,"
    "  damage        INTEGER NOT NULL,"
    "  shield        INTEGER,"
    "  max_shield    INTEGER,"
    "  mana          INTEGER,"
    "  max_mana      INTEGER,"
    "  mana_potion   INTEGER,"
    "  crit          REAL,"
    "  updated_at    TEXT    NOT NULL DEFAULT (datetime('now'))"
    ");";

const char *UPSERT_SQL =
    "INSERT INTO characters "
    "(name, class, health, max_health, stamina, level, coins, xp, health_potion, damage,"
    " shield, max_shield, mana, max_mana, mana_potion, crit, updated_at) "
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, datetime('now')) "
    "ON CONFLICT(name) DO UPDATE SET "
    "  class=excluded.class, health=excluded.health, max_health=excluded.max_health,"
    "  stamina=excluded.stamina, level=excluded.level, coins=excluded.coins, xp=excluded.xp,"
    "  health_potion=excluded.health_potion, damage=excluded.damage, shield=excluded.shield,"
    "  max_shield=excluded.max_shield, mana=excluded.mana, max_mana=excluded.max_mana,"
    "  mana_potion=excluded.mana_potion, crit=excluded.crit, updated_at=datetime('now');";

const char *SELECT_SQL =
    "SELECT name, class, health, max_health, stamina, level, coins, xp, health_potion, damage,"
    "       shield, max_shield, mana, max_mana, mana_potion, crit "
    "FROM characters WHERE name = ?1;";

// Binds an int column, or NULL when the class does not use it. This is what
// keeps "absent" distinct from "zero" in the database.
int bindOptionalInt(sqlite3_stmt *stmt, int index, bool present, int value)
{
    return present ? sqlite3_bind_int(stmt, index, value) : sqlite3_bind_null(stmt, index);
}

int bindOptionalDouble(sqlite3_stmt *stmt, int index, bool present, double value)
{
    return present ? sqlite3_bind_double(stmt, index, value) : sqlite3_bind_null(stmt, index);
}

}  // namespace

SaveStore::SaveStore() : db(0) {}

SaveStore::~SaveStore() { close(); }

bool SaveStore::fail(const char *context)
{
    err = string(context) + ": " + (db ? sqlite3_errmsg(db) : "no database open");
    return false;
}

bool SaveStore::exec(const char *sql)
{
    char *msg = 0;
    if (sqlite3_exec(db, sql, 0, 0, &msg) != SQLITE_OK)
    {
        err = string("exec failed: ") + (msg ? msg : "unknown error");
        sqlite3_free(msg);
        return false;
    }
    return true;
}

bool SaveStore::open(const string &path)
{
    close();
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK)
    {
        fail("could not open save database");
        // sqlite3_open allocates a handle even on failure; release it.
        sqlite3_close(db);
        db = 0;
        return false;
    }
    if (!exec(SCHEMA_SQL))
    {
        close();
        return false;
    }
    return true;
}

void SaveStore::close()
{
    if (db)
    {
        sqlite3_close(db);
        db = 0;
    }
}

bool SaveStore::upsert(const CharacterRecord &record)
{
    if (!isOpen())
    {
        return fail("upsert");
    }

    sqlite3_stmt *stmt = 0;
    if (sqlite3_prepare_v2(db, UPSERT_SQL, -1, &stmt, 0) != SQLITE_OK)
    {
        return fail("prepare upsert");
    }

    // Bound parameters, never string concatenation: character names with quotes
    // or spaces round-trip safely and there is nothing to inject into.
    sqlite3_bind_text(stmt, 1, record.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, record.cls.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, record.health);
    sqlite3_bind_int(stmt, 4, record.maxHealth);
    sqlite3_bind_int(stmt, 5, record.stamina);
    sqlite3_bind_int(stmt, 6, record.level);
    sqlite3_bind_int(stmt, 7, record.coins);
    sqlite3_bind_int(stmt, 8, record.xp);
    sqlite3_bind_int(stmt, 9, record.healthPotion);
    sqlite3_bind_int(stmt, 10, record.damage);
    bindOptionalInt(stmt, 11, record.hasShield, record.shield);
    bindOptionalInt(stmt, 12, record.hasShield, record.maxShield);
    bindOptionalInt(stmt, 13, record.hasMana, record.mana);
    bindOptionalInt(stmt, 14, record.hasMana, record.maxMana);
    bindOptionalInt(stmt, 15, record.hasMana, record.manaPotion);
    bindOptionalDouble(stmt, 16, record.hasCrit, record.crit);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        return fail("save character");
    }
    return true;
}

bool SaveStore::listCharacters(vector<pair<string, string> > &out)
{
    out.clear();
    if (!isOpen())
    {
        return fail("list characters");
    }

    sqlite3_stmt *stmt = 0;
    if (sqlite3_prepare_v2(db, "SELECT name, class FROM characters ORDER BY name;", -1, &stmt, 0) != SQLITE_OK)
    {
        return fail("prepare list");
    }

    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        const char *n = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        const char *c = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        out.push_back(make_pair(string(n ? n : ""), string(c ? c : "")));
    }
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        return fail("list characters");
    }
    return true;
}

bool SaveStore::fetch(const string &name, CharacterRecord &out)
{
    if (!isOpen())
    {
        return fail("fetch");
    }

    sqlite3_stmt *stmt = 0;
    if (sqlite3_prepare_v2(db, SELECT_SQL, -1, &stmt, 0) != SQLITE_OK)
    {
        return fail("prepare fetch");
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        err.clear();  // simply not found
        return false;
    }

    const char *n = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    const char *c = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    out.name = n ? n : "";
    out.cls = c ? c : "";
    out.health = sqlite3_column_int(stmt, 2);
    out.maxHealth = sqlite3_column_int(stmt, 3);
    out.stamina = sqlite3_column_int(stmt, 4);
    out.level = sqlite3_column_int(stmt, 5);
    out.coins = sqlite3_column_int(stmt, 6);
    out.xp = sqlite3_column_int(stmt, 7);
    out.healthPotion = sqlite3_column_int(stmt, 8);
    out.damage = sqlite3_column_int(stmt, 9);

    // A NULL column means the class does not have that stat at all, which is
    // exactly the distinction the old positional text format could not make.
    out.hasShield = sqlite3_column_type(stmt, 10) != SQLITE_NULL;
    out.shield = sqlite3_column_int(stmt, 10);
    out.maxShield = sqlite3_column_int(stmt, 11);

    out.hasMana = sqlite3_column_type(stmt, 12) != SQLITE_NULL;
    out.mana = sqlite3_column_int(stmt, 12);
    out.maxMana = sqlite3_column_int(stmt, 13);
    out.manaPotion = sqlite3_column_int(stmt, 14);

    out.hasCrit = sqlite3_column_type(stmt, 15) != SQLITE_NULL;
    out.crit = sqlite3_column_double(stmt, 15);

    sqlite3_finalize(stmt);
    return true;
}

Character *SaveStore::load(const string &name)
{
    CharacterRecord r;
    if (!fetch(name, r))
    {
        return 0;
    }

    // The class comes from its own column, so it can never be confused with the
    // character's name the way the old format's first line was.
    if (r.cls == "Warrior")
    {
        return new Warrior(r.name, r.health, r.maxHealth, r.stamina, r.level, r.coins, r.xp, r.healthPotion, r.shield,
                           r.damage);
    }
    if (r.cls == "Sorcerer")
    {
        return new Sorcerer(r.name, r.health, r.maxHealth, r.stamina, r.level, r.coins, r.xp, r.healthPotion, r.mana,
                            r.maxMana, r.manaPotion, r.damage);
    }
    if (r.cls == "Assassin")
    {
        return new Assassin(r.name, r.health, r.maxHealth, r.stamina, r.level, r.coins, r.xp, r.healthPotion, r.crit,
                            r.damage);
    }

    err = "unknown character class in save: " + r.cls;
    return 0;
}

bool SaveStore::remove(const string &name)
{
    if (!isOpen())
    {
        return fail("remove");
    }

    sqlite3_stmt *stmt = 0;
    if (sqlite3_prepare_v2(db, "DELETE FROM characters WHERE name = ?1;", -1, &stmt, 0) != SQLITE_OK)
    {
        return fail("prepare delete");
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        return fail("delete character");
    }
    return true;
}
