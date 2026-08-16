#ifndef SORCERER_H
#define SORCERER_H

#include <string>

#include "Character.h"

class Sorcerer : public Character  // Derived Class
{
public:
    Sorcerer();
    Sorcerer(std::string, int, int, int, int, int, int, int, int, int, int, int);
    ~Sorcerer();

    // Mutators
    void setManaPotion(int);
    void setMana(int);
    void setMaxMana(int);

    // Accessors
    int getManaPotion() const;
    int getMana() const;
    int getMaxMana() const;

    // Actions
    void attack(Character &);
    TurnResult battleMenu(Character &);
    TurnResult battleMenuBot(Character &);
    void displayInfo() const;
    void drinkManaPotion();

    // Also restores mana, so an empty pool is never a dead end.
    void rest();

    const char *className() const { return "Sorcerer"; }
    void fillRecord(CharacterRecord &) const;

private:
    bool spendMana(int cost);

    int mana;
    int manaPotion;
    int maxMana;
};

#endif
