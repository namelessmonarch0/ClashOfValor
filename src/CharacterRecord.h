#ifndef CHARACTER_RECORD_H
#define CHARACTER_RECORD_H

#include <string>

// A flat, storage-shaped view of a character: one instance maps to one row of
// the `characters` table.
//
// The class-specific blocks are optional because a Warrior has no mana and a
// Sorcerer has no crit multiplier; those columns are NULL in the database. The
// `has*` flags are what distinguish "absent" from "present and zero", which the
// old plaintext save format could not express -- it wrote every field
// positionally and silently disagreed with itself across classes.
struct CharacterRecord
{
    std::string name;
    std::string cls;  // "Warrior" | "Sorcerer" | "Assassin"

    int health = 0;
    int maxHealth = 0;
    int stamina = 0;
    int level = 1;
    int coins = 0;
    int xp = 0;
    int healthPotion = 0;
    int damage = 0;

    // Warrior
    bool hasShield = false;
    int shield = 0;
    int maxShield = 0;

    // Sorcerer
    bool hasMana = false;
    int mana = 0;
    int maxMana = 0;
    int manaPotion = 0;

    // Assassin
    bool hasCrit = false;
    double crit = 0.0;
};

#endif
