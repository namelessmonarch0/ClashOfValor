#ifndef SAVE_STORE_H
#define SAVE_STORE_H

#include <string>
#include <utility>
#include <vector>

#include "CharacterRecord.h"

class Character;

// Opaque here so sqlite3.h stays confined to SaveStore.cpp.
struct sqlite3;

// SQLite-backed character storage.
//
// Replaces the previous one-file-per-character plaintext scheme, which did not
// work at all: it read stats from cin instead of the file it had just opened,
// identified a character's class by comparing the first line against "Warrior"
// when save() had written the character's *name* there, and wrote its fields in
// a different order for each of the three classes.
//
// A single saves.db holds every character, which is also what lets the load menu
// list what exists instead of asking for a filename typed from memory.
class SaveStore
{
public:
    SaveStore();
    ~SaveStore();

    // Opens (creating if needed) the database and ensures the schema exists.
    bool open(const std::string &path = "saves.db");
    void close();
    bool isOpen() const { return db != 0; }

    // Insert-or-replace keyed on name.
    bool upsert(const CharacterRecord &record);

    // (name, class) pairs, alphabetical, for the load menu.
    bool listCharacters(std::vector<std::pair<std::string, std::string> > &out);

    // Reads one row. Returns false if absent or on error; check lastError() to
    // tell those apart.
    bool fetch(const std::string &name, CharacterRecord &out);

    // Reads a row and builds the matching subclass. Caller owns the result;
    // returns 0 when the character does not exist.
    Character *load(const std::string &name);

    bool remove(const std::string &name);

    const std::string &lastError() const { return err; }

private:
    SaveStore(const SaveStore &);
    SaveStore &operator=(const SaveStore &);

    bool exec(const char *sql);
    bool fail(const char *context);

    sqlite3 *db;
    std::string err;
};

#endif
