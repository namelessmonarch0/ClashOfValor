#include "Assassin.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "Input.h"
#include "Menu.h"

using namespace std;

namespace
{
const double DEFAULT_CRIT = 1.5;
}

Assassin::Assassin() : Character()
{
    damage = 10;
    crit = DEFAULT_CRIT;
}

Assassin::Assassin(string name, int health, int maxHealth, int stamina, int level, int coins, int xp, int healthPotion,
                   double crit, int damage)
    : Character(name, health, maxHealth, stamina, level, coins, xp, healthPotion, damage)
{
    this->crit = crit > 0.0 ? crit : DEFAULT_CRIT;
}

Assassin::~Assassin() {}

void Assassin::setCrit(double crit) { this->crit = crit; }

double Assassin::getCrit() const { return crit; }

void Assassin::attack(Character &opponent)
{
    if (!spendStamina(rules::ATTACK_STAMINA))
    {
        return;
    }
    cout << getName() << " slips through the guard and lands a swift, clean strike!" << endl;
    opponent.takeDamage(getDamage());
}

void Assassin::critAttack(Character &opponent)
{
    if (!spendStamina(rules::CRIT_STAMINA))
    {
        return;
    }
    // getCrit() returns a double now, so 1.5x really is 1.5x.
    const int dealt = static_cast<int>(getDamage() * getCrit());
    cout << getName() << " vanishes into the shadows and strikes a vital spot for " << dealt << " damage!" << endl;
    opponent.takeDamage(dealt);
}

TurnResult Assassin::battleMenu(Character &opponent)
{
    cout << "Choose your next move" << endl;
    vector<string> items;
    items.push_back("Attack");
    items.push_back("Crit Attack");
    items.push_back("Drink health potion");
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
        critAttack(opponent);
        break;  // this break was missing: a crit fell straight through into "drink health potion"
    case 3:
        drinkHealthPotion();
        break;
    case 4:
        rest();
        break;
    case 5:
        cout << getName() << " melts into the shadows and flees the fight!" << endl;
        return TurnResult::Fled;
    }
    return TurnResult::Continue;
}

TurnResult Assassin::battleMenuBot(Character &opponent)
{
    if (getHealth() * 3 < getMaxHealth() && getHealthPotion() > 0)
    {
        drinkHealthPotion();
    }
    else if (getStamina() >= rules::CRIT_STAMINA && rand() % 2 == 0)
    {
        critAttack(opponent);
    }
    else if (getStamina() >= rules::ATTACK_STAMINA)
    {
        attack(opponent);
    }
    else
    {
        rest();
    }
    return TurnResult::Continue;
}

void Assassin::fillRecord(CharacterRecord &record) const
{
    fillBaseRecord(record);
    record.hasCrit = true;
    record.crit = crit;
}

void Assassin::displayInfo() const
{
    cout << getName() << " (Assassin) - Lv " << getLevel() << " - health: " << getHealth() << "/" << getMaxHealth()
         << " - Crit: " << getCrit() << "x - Stamina: " << getStamina() << " - Potions: " << getHealthPotion()
         << " - XP: " << getXP() << " - Coins: " << getCoins() << endl;
}
