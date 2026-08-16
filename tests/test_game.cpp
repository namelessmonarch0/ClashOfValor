// Regression tests for Clash of Valor.
//
// Every check here corresponds to a bug that was once live in this codebase, so
// a failure means something specific has regressed rather than "a test broke".
// The classes are driven directly -- nothing depends on feeding menus through
// stdin.
//
// Run with: make test

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "Assassin.h"
#include "Character.h"
#include "SaveStore.h"
#include "Sorcerer.h"
#include "Warrior.h"

namespace
{

int checksRun = 0;
int failures = 0;

void check(const char *what, bool ok)
{
    checksRun++;
    printf("  %-56s %s\n", what, ok ? "ok" : "*** FAIL ***");
    if (!ok)
    {
        failures++;
    }
}

void section(const char *title) { printf("\n%s\n", title); }

// --- Assassin: crit used to fall through into "drink potion", and the 1.5x
// --- multiplier was truncated to 1x by an int accessor over a double field.
void testCrit()
{
    section("Assassin crit");

    Warrior normalTarget("T", 100, 100, 100, 1, 0, 0, 0, 0, 10);
    Warrior critTarget("T", 100, 100, 100, 1, 0, 0, 0, 0, 10);

    Assassin a("A", 100, 100, 100, 1, 0, 0, 0, 1.5, 10);
    a.attack(normalTarget);
    Assassin b("B", 100, 100, 100, 1, 0, 0, 0, 1.5, 10);
    b.critAttack(critTarget);

    const int normal = 100 - normalTarget.getHealth();
    const int crit = 100 - critTarget.getHealth();

    check("normal attack deals base damage", normal == 10);
    check("crit deals 1.5x, not 1x", crit == 15);
    check("getCrit() keeps the fraction", a.getCrit() > 1.4 && a.getCrit() < 1.6);

    // The missing `break` meant a crit also drank a potion.
    Assassin c("C", 50, 100, 100, 1, 0, 0, 1, 1.5, 10);
    Warrior t("T", 100, 100, 100, 1, 0, 0, 0, 0, 10);
    c.critAttack(t);
    check("crit does not also consume a potion", c.getHealthPotion() == 1);
}

// --- Potions were checked but never decremented, so they were infinite.
void testPotions()
{
    section("Potion consumption");

    Warrior w("W", 50, 100, 100, 1, 0, 0, 1, 0, 10);
    w.drinkHealthPotion();
    check("count drops 1 -> 0", w.getHealthPotion() == 0);
    check("health actually restored", w.getHealth() > 50);

    const int hp = w.getHealth();
    w.drinkHealthPotion();
    check("drinking at 0 heals nothing", w.getHealth() == hp);
    check("count never goes negative", w.getHealthPotion() == 0);

    Sorcerer s("S", 100, 100, 100, 1, 0, 0, 0, 10, 60, 1, 10);
    s.drinkManaPotion();
    check("mana potion consumed", s.getManaPotion() == 0);
    check("mana restored", s.getMana() > 10);
}

// --- The Warrior's advertised shield was stored and printed but never
// --- consulted in any damage calculation.
void testShield()
{
    section("Warrior shield");

    Warrior w("W", 100, 100, 100, 1, 0, 0, 0, 25, 10);
    w.takeDamage(10);
    check("shield absorbs, health untouched", w.getHealth() == 100 && w.getShield() == 15);

    w.takeDamage(30);
    check("overflow past shield reaches health", w.getShield() == 0 && w.getHealth() == 85);

    w.takeDamage(5);
    check("with shield gone, damage is direct", w.getHealth() == 80);
}

// --- Health printed as negative, e.g. "-15/100".
void testHealthClamp()
{
    section("Health bounds");

    Warrior w("W", 10, 100, 100, 1, 0, 0, 0, 0, 10);
    w.takeDamage(500);
    check("health floors at 0", w.getHealth() == 0);
    check("dead characters report not alive", !w.isAlive());

    Warrior h("H", 95, 100, 100, 1, 0, 0, 5, 0, 10);
    h.drinkHealthPotion();
    check("health never exceeds max", h.getHealth() == 100);
}

// --- Stamina and mana went negative and gated nothing; rest() never restored
// --- stamina despite its own flavour text.
void testResources()
{
    section("Stamina and mana gating");

    Assassin a("A", 100, 100, 5, 1, 0, 0, 0, 1.5, 10);
    Warrior t("T", 100, 100, 100, 1, 0, 0, 0, 0, 10);
    a.attack(t);
    check("attack refused when stamina short", t.getHealth() == 100);
    check("nothing spent on a refused attack", a.getStamina() == 5);

    a.rest();
    check("rest restores stamina", a.getStamina() > 5);

    Sorcerer s("S", 100, 100, 100, 1, 0, 0, 0, 0, 60, 0, 10);
    Warrior t2("T", 100, 100, 100, 1, 0, 0, 0, 0, 10);
    const int staminaBefore = s.getStamina();
    s.attack(t2);
    check("spell refused with no mana", t2.getHealth() == 100);
    check("failed cast spends no stamina", s.getStamina() == staminaBefore);

    s.rest();
    check("rest restores mana too (no dead end)", s.getMana() > 0);
}

// --- levelUp() had no callers anywhere; xp and coins were never awarded.
void testProgression()
{
    section("Progression");

    Warrior w("W", 100, 100, 100, 1, 0, 0, 0, 25, 10);
    Warrior loser("L", 1, 100, 100, 2, 0, 0, 0, 0, 10);

    const int damageBefore = w.getDamage();
    w.awardVictorySpoils(loser);

    check("coins awarded", w.getCoins() > 0);
    check("level increased", w.getLevel() > 1);
    check("damage scales with level", w.getDamage() > damageBefore);
    check("leftover xp carried, not discarded", w.getXP() >= 0 && w.getXP() < 100);

    // A single large award should cross multiple thresholds.
    Warrior rich("R", 100, 100, 100, 1, 0, 0, 0, 25, 10);
    Warrior boss("B", 1, 100, 100, 5, 0, 0, 0, 0, 10);
    rich.awardVictorySpoils(boss);
    check("multiple levels in one award", rich.getLevel() >= 3);
}

// --- Save/load was entirely non-functional: it read stats from cin instead of
// --- the file, matched the class against a line holding the character's name,
// --- and wrote fields in a different order for each class.
void testPersistence()
{
    section("SQLite persistence");

    const std::string dbPath = "build/test_saves.db";
    remove(dbPath.c_str());

    SaveStore store;
    check("database opens", store.open(dbPath));

    // A name with quotes and spaces would break any string-concatenated SQL.
    const std::string trickyName = "Grimm the \"Bold\" O'Hara";

    Warrior w(trickyName, 70, 120, 55, 3, 40, 20, 2, 15, 16);
    Sorcerer s("Zara", 60, 110, 45, 2, 30, 10, 1, 25, 60, 2, 14);
    Assassin a("Shadow", 50, 100, 35, 4, 90, 55, 3, 1.5, 18);

    check("warrior saves", w.saveTo(store));
    check("sorcerer saves", s.saveTo(store));
    check("assassin saves", a.saveTo(store));

    std::vector<std::pair<std::string, std::string> > listed;
    store.listCharacters(listed);
    check("all three listed", listed.size() == 3);

    Character *lw = store.load(trickyName);
    check("name with quotes round-trips", lw != 0);
    Warrior *rw = dynamic_cast<Warrior *>(lw);
    check("warrior reloads as Warrior", rw != 0);
    check("warrior stats survive",
          rw && rw->getLevel() == 3 && rw->getCoins() == 40 && rw->getXP() == 20 && rw->getShield() == 15);
    delete lw;

    Character *ls = store.load("Zara");
    Sorcerer *rs = dynamic_cast<Sorcerer *>(ls);
    check("sorcerer reloads as Sorcerer", rs != 0);
    check("mana fields survive", rs && rs->getMana() == 25 && rs->getMaxMana() == 60 && rs->getManaPotion() == 2);
    delete ls;

    Character *la = store.load("Shadow");
    Assassin *ra = dynamic_cast<Assassin *>(la);
    check("assassin reloads as Assassin", ra != 0);
    check("crit 1.5 survives as a double", ra && ra->getCrit() > 1.4 && ra->getCrit() < 1.6);
    check("xp and coins survive", ra && ra->getXP() == 55 && ra->getCoins() == 90);
    delete la;

    // Saving the same name twice must update in place.
    w.setCoins(999);
    w.saveTo(store);
    store.listCharacters(listed);
    check("re-save upserts, no duplicate row", listed.size() == 3);

    Character *again = store.load(trickyName);
    check("upsert wrote the new value", again && again->getCoins() == 999);
    delete again;

    check("absent character returns null", store.load("Nobody") == 0);
    check("delete removes the row", store.remove("Zara"));
    check("deleted character is gone", store.load("Zara") == 0);

    remove(dbPath.c_str());
}

}  // namespace

int main()
{
    printf("Clash of Valor regression tests\n");
    printf("(combat narration below is expected -- the classes print as they act)\n");

    testCrit();
    testPotions();
    testShield();
    testHealthClamp();
    testResources();
    testProgression();
    testPersistence();

    printf("\n%s: %d checks, %d failure%s\n", failures ? "FAILED" : "PASSED", checksRun, failures,
           failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
