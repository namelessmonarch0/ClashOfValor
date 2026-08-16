#ifndef CHARACTER_H
#define CHARACTER_H

#include <string>

#include "CharacterRecord.h"

class SaveStore;

// What a combatant did with their turn. The battle loop needs to distinguish
// "played a move" from "left the fight" -- previously every Flee branch just
// printed "Fleeing..." and the loop kept going, so fleeing did nothing.
enum class TurnResult
{
    Continue,
    Fled
};

// Shared combat costs and rewards, in one place so the classes stay comparable.
namespace rules
{
const int ATTACK_STAMINA = 10;
const int CRIT_STAMINA = 20;
const int SPELL_MANA = 10;
const int REST_HEALTH = 10;
const int REST_STAMINA = 25;
const int POTION_HEALTH = 50;
const int POTION_MANA = 50;
const int XP_PER_LEVEL = 100;
const int XP_PER_VICTORY = 60;
const int COINS_PER_VICTORY = 25;
}  // namespace rules

class Character  // Base Class
{
public:
    // Overloaded Constructor
    Character();
    Character(std::string, int, int, int, int, int, int, int, int);
    virtual ~Character();

    // Mutators
    void setName(std::string);
    void setHealth(int);
    void setStamina(int);
    void setLevel(int);
    void setCoins(int);
    void setHealthPotion(int);
    void setXP(int);
    void setMaxHealth(int);
    void setDamage(int);

    // Accessors
    std::string getName() const;
    int getHealthPotion() const;
    int getHealth() const;
    int getStamina() const;
    int getLevel() const;
    int getCoins() const;
    int getXP() const;
    int getMaxHealth() const;
    int getDamage() const;

    // Actions
    // Virtual so the Sorcerer can meditate mana back as well as health. Without
    // that, a Sorcerer out of both mana and mana potions could never act again
    // and the battle would never end.
    virtual void rest();
    void levelUp();
    void drinkHealthPotion();
    bool isAlive() const { return health > 0; }

    // Single point of entry for incoming damage. Warrior overrides it to soak
    // hits with its shield first; everything else takes it on the chin. Attacks
    // used to poke at opponent.setHealth() directly, duplicating the formula
    // four times and leaving no seam for the shield.
    virtual void takeDamage(int amount);

    virtual TurnResult battleMenu(Character &) = 0;
    virtual TurnResult battleMenuBot(Character &) = 0;
    virtual void attack(Character &) = 0;
    virtual void displayInfo() const = 0;

    virtual const char *className() const = 0;

    // Persistence: subclasses add their own columns on top of the base fields.
    virtual void fillRecord(CharacterRecord &) const = 0;
    bool saveTo(SaveStore &store);

    // Grants xp and coins for beating `defeated`, then applies any levels
    // earned. Nothing called levelUp() before this existed.
    void awardVictorySpoils(const Character &defeated);

protected:
    void fillBaseRecord(CharacterRecord &) const;

    // Returns false (and spends nothing) when the cost cannot be paid, so
    // attacks can be refused instead of driving the pool negative.
    bool spendStamina(int cost);

    std::string name;
    int health;
    int maxHealth;
    int stamina;
    int level;
    int coins;
    int xp;
    int healthPotion;
    int damage;
};

#endif
