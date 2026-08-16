#include "Character.h"

#include <algorithm>
#include <iostream>
#include <string>

#include "Animate.h"
#include "SaveStore.h"

using namespace std;

Character::Character()
{
    name = "Unknown";
    health = 100;
    maxHealth = 100;
    stamina = 100;
    level = 1;
    coins = 0;
    xp = 0;
    healthPotion = 0;
    damage = 10;
}

Character::Character(string name, int health, int maxHealth, int stamina, int level, int coins, int xp,
                     int healthPotion, int damage)
{
    this->name = name;
    this->health = health;
    this->maxHealth = maxHealth;
    this->stamina = stamina;
    this->level = level;
    this->coins = coins;
    this->xp = xp;
    this->healthPotion = healthPotion;
    this->damage = damage;
}

Character::~Character() {}

void Character::setName(string name) { this->name = name; }

// Health is clamped at zero: a fatal blow used to leave the status line
// reporting things like "-15/100".
void Character::setHealth(int health) { this->health = max(0, min(health, maxHealth)); }

void Character::setStamina(int stamina) { this->stamina = max(0, stamina); }

void Character::setLevel(int level) { this->level = level; }

void Character::setCoins(int coins) { this->coins = coins; }

void Character::setXP(int xp) { this->xp = xp; }

void Character::setHealthPotion(int healthPotion) { this->healthPotion = max(0, healthPotion); }

void Character::setMaxHealth(int maxHealth) { this->maxHealth = maxHealth; }

void Character::setDamage(int damage) { this->damage = damage; }

string Character::getName() const { return name; }

int Character::getHealth() const { return health; }

int Character::getStamina() const { return stamina; }

int Character::getLevel() const { return level; }

int Character::getCoins() const { return coins; }

int Character::getHealthPotion() const { return healthPotion; }

int Character::getXP() const { return xp; }

int Character::getMaxHealth() const { return maxHealth; }

int Character::getDamage() const { return damage; }

// ACTIONS

void Character::takeDamage(int amount)
{
    if (amount <= 0)
    {
        return;
    }
    setHealth(health - amount);
}

bool Character::spendStamina(int cost)
{
    if (stamina < cost)
    {
        cout << getName() << " is too winded for that -- only " << stamina << " stamina left (needs " << cost
             << "). Rest to recover." << endl;
        return false;
    }
    stamina -= cost;
    return true;
}

void Character::rest()
{
    cout << getName()
         << " takes a moment to breathe, stepping back and regaining strength as the battle rages on." << endl;

    // Resting now restores the stamina its own flavour text always claimed to.
    // Before this, stamina only ever decreased, for the entire life of a
    // character, and rest() was strictly a weaker heal.
    const int staminaBefore = stamina;
    stamina += rules::REST_STAMINA;

    const int healthBefore = health;
    setHealth(health + rules::REST_HEALTH);

    if (health == healthBefore && stamina == staminaBefore)
    {
        cout << "  ...but is already at full strength." << endl;
        return;
    }
    cout << "  +" << (health - healthBefore) << " health, +" << (stamina - staminaBefore) << " stamina." << endl;
}

void Character::drinkHealthPotion()
{
    // The availability check comes first now. It used to print the whole
    // "soothing liquid revitalising your body" line before discovering you had
    // no potions at all.
    if (healthPotion <= 0)
    {
        cout << getName() << " reaches for a health potion, but the pouch is empty." << endl;
        return;
    }

    if (health == maxHealth)
    {
        cout << getName() << " is already at full health -- the potion stays corked." << endl;
        return;
    }

    // ...and the potion is actually consumed. The old code checked the count but
    // never decremented it, so potions were infinite.
    healthPotion--;

    const int before = health;
    setHealth(health + rules::POTION_HEALTH);
    const int gained = health - before;
    cout << getName() << " drinks a healing potion, mending their wounds. +" << gained << " health (" << healthPotion
         << " left)." << endl;
    animate::potionFlourish(gained, "HP", 82);  // bright green
}

void Character::levelUp()
{
    // `while`, not `if`, and `>=`, not `>`: a single fight can now carry a
    // character across more than one threshold, and landing exactly on 100 xp
    // counts.
    while (xp >= rules::XP_PER_LEVEL)
    {
        xp -= rules::XP_PER_LEVEL;
        level++;
        maxHealth += 10;
        damage += 2;
        health = maxHealth;  // a level-up is a full heal
        cout << "*** " << getName() << " reached level " << level << "! Max health " << maxHealth << ", damage "
             << damage << ". ***" << endl;
    }
}

void Character::awardVictorySpoils(const Character &defeated)
{
    const int gainedXP = rules::XP_PER_VICTORY * defeated.getLevel();
    const int gainedCoins = rules::COINS_PER_VICTORY * defeated.getLevel();

    xp += gainedXP;
    coins += gainedCoins;
    cout << getName() << " gains " << gainedXP << " xp and " << gainedCoins << " coins." << endl;

    levelUp();
}

void Character::fillBaseRecord(CharacterRecord &record) const
{
    record.name = name;
    record.cls = className();
    record.health = health;
    record.maxHealth = maxHealth;
    record.stamina = stamina;
    record.level = level;
    record.coins = coins;
    record.xp = xp;
    record.healthPotion = healthPotion;
    record.damage = damage;
}

bool Character::saveTo(SaveStore &store)
{
    CharacterRecord record;
    fillRecord(record);
    return store.upsert(record);
}
