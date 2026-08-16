#ifndef ASSASSIN_H
#define ASSASSIN_H

#include <string>

#include "Character.h"

class Assassin : public Character  // Derived Class
{
public:
    Assassin();
    Assassin(std::string, int, int, int, int, int, int, int, double, int);
    ~Assassin();

    // crit is a multiplier, so it is a double end to end. It used to be stored
    // as a double but read back through an `int getCrit()`, which truncated the
    // default 1.5 to 1 -- the crit attack cost double stamina for exactly
    // normal damage.
    void setCrit(double);
    double getCrit() const;

    // Actions
    void attack(Character &);
    void critAttack(Character &);
    TurnResult battleMenu(Character &);
    TurnResult battleMenuBot(Character &);
    void displayInfo() const;

    const char *className() const { return "Assassin"; }
    void fillRecord(CharacterRecord &) const;

private:
    double crit;
};

#endif
