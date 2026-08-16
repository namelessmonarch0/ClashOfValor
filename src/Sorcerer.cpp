#include "Sorcerer.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "Animate.h"
#include "Input.h"
#include "Menu.h"

using namespace std;

namespace
{
const int REST_MANA = 20;
const int SPELL_DAMAGE_BONUS = 4;  // spells hit harder than a plain weapon
}  // namespace

Sorcerer::Sorcerer() : Character()
{
    damage = 10;
    mana = 50;
    maxMana = 50;
    manaPotion = 3;
}

Sorcerer::Sorcerer(string name, int health, int maxHealth, int stamina, int level, int coins, int xp, int healthPotion,
                   int mana, int maxMana, int manaPotion, int damage)
    : Character(name, health, maxHealth, stamina, level, coins, xp, healthPotion, damage)
{
    this->maxMana = max(1, maxMana);
    this->mana = max(0, min(mana, this->maxMana));
    this->manaPotion = max(0, manaPotion);
}

Sorcerer::~Sorcerer() {}

void Sorcerer::setMana(int mana) { this->mana = max(0, min(mana, maxMana)); }

void Sorcerer::setMaxMana(int maxMana)
{
    this->maxMana = max(1, maxMana);
    this->mana = min(mana, this->maxMana);
}

void Sorcerer::setManaPotion(int manaPotion) { this->manaPotion = max(0, manaPotion); }

int Sorcerer::getMana() const { return mana; }

int Sorcerer::getManaPotion() const { return manaPotion; }

int Sorcerer::getMaxMana() const { return maxMana; }

bool Sorcerer::spendMana(int cost)
{
    if (mana < cost)
    {
        cout << getName() << " has too little mana for that -- " << mana << " left (needs " << cost
             << "). Rest or drink a mana potion." << endl;
        return false;
    }
    mana -= cost;
    return true;
}

void Sorcerer::attack(Character &opponent)
{
    // Both costs are checked before either is spent, so a failed cast never
    // silently drains stamina. Attacks used to subtract from stamina and mana
    // unconditionally, driving both negative and never blocking anything.
    if (mana < rules::SPELL_MANA)
    {
        cout << getName() << " has too little mana to cast -- " << mana << " left (needs " << rules::SPELL_MANA
             << "). Rest or drink a mana potion." << endl;
        return;
    }
    if (!spendStamina(rules::ATTACK_STAMINA))
    {
        return;
    }
    spendMana(rules::SPELL_MANA);

    cout << getName() << " conjures a fiery orb of energy and hurls it, exploding on impact!" << endl;
    opponent.takeDamage(getDamage() + SPELL_DAMAGE_BONUS);
}

void Sorcerer::drinkManaPotion()
{
    if (manaPotion <= 0)
    {
        cout << getName() << " reaches for a mana potion, but the pouch is empty." << endl;
        return;
    }
    if (mana == maxMana)
    {
        cout << getName() << " is already brimming with arcane energy." << endl;
        return;
    }

    manaPotion--;  // consumed -- the old version checked the count but never spent it

    const int before = mana;
    setMana(mana + rules::POTION_MANA);
    const int gained = mana - before;
    cout << getName() << " drains a shimmering mana potion, arcane energy coursing through them. +" << gained
         << " mana (" << manaPotion << " left)." << endl;
    animate::potionFlourish(gained, "MP", 45);  // bright blue/cyan
}

void Sorcerer::rest()
{
    Character::rest();
    const int before = mana;
    setMana(mana + REST_MANA);
    if (mana != before)
    {
        cout << "  +" << (mana - before) << " mana." << endl;
    }
}

TurnResult Sorcerer::battleMenu(Character &opponent)
{
    cout << "Choose your next move" << endl;
    vector<string> items;
    items.push_back("Cast fire orb");
    items.push_back("Drink health potion");
    items.push_back("Drink mana potion");
    items.push_back("Rest");
    items.push_back("Flee");

    const int selection = menu::choose(items);
    if (selection < 0)
    {
        return TurnResult::Fled;
    }

    switch (selection)
    {
    case 1:
        attack(opponent);
        break;
    case 2:
        drinkHealthPotion();
        break;
    case 3:
        drinkManaPotion();
        break;
    case 4:
        rest();
        break;
    case 5:
        cout << getName() << " melts into a puff of smoke and flees the fight!" << endl;
        return TurnResult::Fled;
    }
    return TurnResult::Continue;
}

TurnResult Sorcerer::battleMenuBot(Character &opponent)
{
    if (getHealth() * 3 < getMaxHealth() && getHealthPotion() > 0)
    {
        drinkHealthPotion();
    }
    else if (mana < rules::SPELL_MANA && manaPotion > 0)
    {
        drinkManaPotion();
    }
    else if (mana >= rules::SPELL_MANA && getStamina() >= rules::ATTACK_STAMINA)
    {
        attack(opponent);
    }
    else
    {
        rest();
    }
    return TurnResult::Continue;
}

void Sorcerer::fillRecord(CharacterRecord &record) const
{
    fillBaseRecord(record);
    record.hasMana = true;
    record.mana = mana;
    record.maxMana = maxMana;
    record.manaPotion = manaPotion;
}

void Sorcerer::displayInfo() const
{
    cout << getName() << " (Sorcerer) - Lv " << getLevel() << " - health: " << getHealth() << "/" << getMaxHealth()
         << " - Mana: " << getMana() << "/" << getMaxMana() << " - Stamina: " << getStamina()
         << " - Potions: " << getHealthPotion() << "hp/" << getManaPotion() << "mp - XP: " << getXP()
         << " - Coins: " << getCoins() << endl;
}
