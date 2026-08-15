#include "Warrior.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

#include "Input.h"

using namespace std;

namespace
{
const int SHIELD_PER_LEVEL = 25;
const int RAISE_SHIELD_STAMINA = 15;
}  // namespace

Warrior::Warrior() : Character()
{
    damage = 10;
    maxShield = SHIELD_PER_LEVEL;
    shield = maxShield;
}

Warrior::Warrior(string name, int health, int maxHealth, int stamina, int level, int coins, int xp, int healthPotion,
                 int shield, int damage)
    : Character(name, health, maxHealth, stamina, level, coins, xp, healthPotion, damage)
{
    this->maxShield = SHIELD_PER_LEVEL * (level > 0 ? level : 1);
    this->shield = max(0, min(shield, this->maxShield));
}

Warrior::~Warrior() {}

void Warrior::setShield(int shield) { this->shield = max(0, min(shield, maxShield)); }

void Warrior::setMaxShield(int maxShield)
{
    this->maxShield = max(0, maxShield);
    this->shield = min(shield, this->maxShield);
}

int Warrior::getShield() const { return shield; }

int Warrior::getMaxShield() const { return maxShield; }

// Damage eats the shield first and only spills into health once the shield is
// gone. This override is the reason Character::takeDamage exists at all.
void Warrior::takeDamage(int amount)
{
    if (amount <= 0)
    {
        return;
    }

    if (shield > 0)
    {
        const int absorbed = min(shield, amount);
        shield -= absorbed;
        amount -= absorbed;
        cout << "  " << getName() << "'s shield absorbs " << absorbed << " damage (" << shield << "/" << maxShield
             << " remaining)." << endl;
    }

    if (amount > 0)
    {
        setHealth(getHealth() - amount);
    }
}

void Warrior::raiseShield()
{
    if (!spendStamina(RAISE_SHIELD_STAMINA))
    {
        return;
    }
    if (shield >= maxShield)
    {
        cout << getName() << "'s shield is already braced at full strength." << endl;
        return;
    }
    const int before = shield;
    setShield(shield + SHIELD_PER_LEVEL);
    cout << getName() << " braces their shield against the next blow. +" << (shield - before) << " shield ("
         << shield << "/" << maxShield << ")." << endl;
}

void Warrior::attack(Character &opponent)
{
    if (!spendStamina(rules::ATTACK_STAMINA))
    {
        return;
    }
    cout << getName() << " unleashes a devastating slash with their blade, cleaving through with raw power!" << endl;
    opponent.takeDamage(getDamage());
}

TurnResult Warrior::battleMenu(Character &opponent)
{
    cout << "Choose your next move" << endl;
    cout << "1. Attack" << endl;
    cout << "2. Raise shield" << endl;
    cout << "3. Drink health potion" << endl;
    cout << "4. Rest" << endl;
    cout << "5. Flee" << endl;

    int selection = 0;
    if (!input::readInt("> ", 1, 5, selection))
    {
        return TurnResult::Fled;  // stdin closed; leave the fight rather than spin
    }

    switch (selection)
    {
    case 1:
        attack(opponent);
        break;
    case 2:
        raiseShield();
        break;
    case 3:
        drinkHealthPotion();
        break;
    case 4:
        rest();
        break;
    case 5:
        cout << getName() << " breaks off and flees the fight!" << endl;
        return TurnResult::Fled;
    }
    return TurnResult::Continue;
}

TurnResult Warrior::battleMenuBot(Character &opponent)
{
    // Heal when badly hurt, brace when the shield is down, otherwise swing --
    // the bot used to pick uniformly at random and would happily quaff potions
    // at full health.
    if (getHealth() * 3 < getMaxHealth() && getHealthPotion() > 0)
    {
        drinkHealthPotion();
    }
    else if (shield == 0 && getStamina() >= RAISE_SHIELD_STAMINA && rand() % 3 == 0)
    {
        raiseShield();
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

void Warrior::fillRecord(CharacterRecord &record) const
{
    fillBaseRecord(record);
    record.hasShield = true;
    record.shield = shield;
    record.maxShield = maxShield;
}

void Warrior::displayInfo() const
{
    cout << getName() << " (Warrior) - Lv " << getLevel() << " - health: " << getHealth() << "/" << getMaxHealth()
         << " - Shield: " << getShield() << "/" << getMaxShield() << " - Stamina: " << getStamina()
         << " - Potions: " << getHealthPotion() << " - XP: " << getXP() << " - Coins: " << getCoins() << endl;
}
