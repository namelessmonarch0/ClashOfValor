#ifndef WARRIOR_H
#define WARRIOR_H

#include <string>

#include "Character.h"

class Warrior : public Character  // Derived Class
{
public:
    Warrior();
    Warrior(std::string, int, int, int, int, int, int, int, int, int);
    ~Warrior();

    // Mutators
    void setShield(int);
    void setMaxShield(int);

    // Accessors
    int getShield() const;
    int getMaxShield() const;

    // Actions
    void attack(Character &);
    TurnResult battleMenu(Character &);
    TurnResult battleMenuBot(Character &);
    void displayInfo() const;

    // The shield is the whole point of the class -- it absorbs damage before
    // health does. The old `healthMultiplier` field was stored, saved and
    // printed but never once consulted in a damage calculation, so the "extra
    // health" the class-select screen promised did not exist.
    void takeDamage(int amount);
    void raiseShield();

    const char *className() const { return "Warrior"; }
    void fillRecord(CharacterRecord &) const;

private:
    int shield;
    int maxShield;
};

#endif
