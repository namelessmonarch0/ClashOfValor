#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Assassin.h"
#include "Character.h"
#include "Input.h"
#include "Paths.h"
#include "SaveStore.h"
#include "Sorcerer.h"
#include "Warrior.h"

using namespace std;

namespace
{

// Ownership lives in unique_ptr now. The old globals were raw Character* that
// were reassigned on every trip through the menu without ever being deleted.
typedef unique_ptr<Character> CharacterPtr;

enum class BattleOutcome
{
    FirstWon,
    SecondWon,
    Fled
};

void printASCII(const string &filename)
{
    ifstream inFile(filename.c_str());
    if (!inFile.is_open())
    {
        return;
    }
    string line;
    while (getline(inFile, line))
    {
        cout << line << endl;
    }
}

void showStatus(const Character &a, const Character &b)
{
    cout << "--- Current Status ---" << endl;
    a.displayInfo();
    b.displayInfo();
    cout << "______________________________________________________________" << endl << endl;
}

// One battle loop for both game modes. The only difference is whether the
// second combatant is driven by a human or the bot heuristics.
//
// Each death check returns immediately. The old playerVSbot() printed the
// victory line and then carried straight on to give the corpse another turn.
BattleOutcome runBattle(Character &first, Character &second, bool secondIsBot)
{
    while (true)
    {
        cout << first.getName() << "'s turn" << endl;
        if (first.battleMenu(second) == TurnResult::Fled)
        {
            return BattleOutcome::Fled;
        }
        if (!second.isAlive())
        {
            cout << endl << second.getName() << " has been defeated! " << first.getName() << " wins!" << endl;
            return BattleOutcome::FirstWon;
        }
        showStatus(first, second);

        cout << second.getName() << "'s turn" << endl;
        const TurnResult result = secondIsBot ? second.battleMenuBot(first) : second.battleMenu(first);
        if (result == TurnResult::Fled)
        {
            return BattleOutcome::Fled;
        }
        if (!first.isAlive())
        {
            cout << endl << first.getName() << " has been defeated! " << second.getName() << " wins!" << endl;
            return BattleOutcome::SecondWon;
        }
        showStatus(first, second);
    }
}

CharacterPtr createRandomOpponent()
{
    const int randomType = rand() % 3 + 1;
    const int l = rand() % 3 + 1;  // scale stats with a random level
    const int hp = l * 100;
    const int dmg = l * 10;

    switch (randomType)
    {
    case 1:
        return CharacterPtr(new Warrior("Warrior", hp, hp, 100, l, 0, 0, 2, l * 25, dmg));
    case 2:
        return CharacterPtr(new Sorcerer("Sorcerer", hp, hp, 100, l, 0, 0, 2, l * 50, l * 50, 3, dmg));
    default:
        return CharacterPtr(new Assassin("Assassin", hp, hp, 100, l, 0, 0, 2, 1.5, dmg));
    }
}

CharacterPtr newCharacter()
{
    string playerName;
    if (!input::readLine("Enter your character name: ", playerName))
    {
        return CharacterPtr();
    }

    cout << "Select your class:" << endl;
    cout << "------------------" << endl;
    cout << "1. Warrior  (carries a shield that absorbs damage before health)" << endl;
    cout << "2. Assassin (trades survivability for a high-damage critical strike)" << endl;
    cout << "3. Sorcerer (spends mana on fire orbs that hit harder than steel)" << endl;

    int classSelection = 0;
    if (!input::readInt("> ", 1, 3, classSelection))
    {
        return CharacterPtr();
    }

    switch (classSelection)
    {
    case 1:
        return CharacterPtr(new Warrior(playerName, 100, 100, 100, 1, 0, 0, 2, 25, 10));
    case 2:
        return CharacterPtr(new Assassin(playerName, 100, 100, 100, 1, 0, 0, 2, 1.5, 10));
    default:
        return CharacterPtr(new Sorcerer(playerName, 100, 100, 100, 1, 0, 0, 2, 50, 50, 3, 10));
    }
}

// Lists what is actually in the database and lets the player pick by number.
// The old version asked for a filename typed from memory, then read the stats
// from cin instead of the file.
CharacterPtr loadCharacter(SaveStore &store)
{
    vector<pair<string, string> > saved;
    if (!store.listCharacters(saved))
    {
        cout << "Could not read saved characters: " << store.lastError() << endl;
        return CharacterPtr();
    }
    if (saved.empty())
    {
        cout << "No saved characters yet." << endl;
        return CharacterPtr();
    }

    cout << "Saved characters:" << endl;
    for (size_t i = 0; i < saved.size(); ++i)
    {
        cout << "  " << (i + 1) << ". " << saved[i].first << " (" << saved[i].second << ")" << endl;
    }

    int choice = 0;
    if (!input::readInt("Which character? ", 1, static_cast<int>(saved.size()), choice))
    {
        return CharacterPtr();
    }

    CharacterPtr loaded(store.load(saved[choice - 1].first));
    if (!loaded)
    {
        cout << "Failed to load character: " << store.lastError() << endl;
        return CharacterPtr();
    }
    cout << "Loaded " << loaded->getName() << "." << endl;
    return loaded;
}

void offerSave(SaveStore &store, Character &character)
{
    int choice = 0;
    if (!input::readInt("Save this character? (1 = yes, 2 = no) ", 1, 2, choice) || choice != 1)
    {
        return;
    }
    if (character.saveTo(store))
    {
        cout << character.getName() << " saved." << endl;
    }
    else
    {
        cout << "Save failed: " << store.lastError() << endl;
    }
}

void playerVsBot(SaveStore &store)
{
    while (true)
    {
        cout << endl << "1. New Character" << endl << "2. Load Character" << endl << "3. Back" << endl;

        int choice = 0;
        if (!input::readInt("> ", 1, 3, choice) || choice == 3)
        {
            return;
        }

        CharacterPtr player = (choice == 1) ? newCharacter() : loadCharacter(store);
        if (!player)
        {
            continue;
        }

        CharacterPtr opponent = createRandomOpponent();
        cout << endl << "A level " << opponent->getLevel() << " " << opponent->className() << " blocks your path!"
             << endl << endl;

        const BattleOutcome outcome = runBattle(*player, *opponent, true);
        if (outcome == BattleOutcome::FirstWon)
        {
            // xp and coins were never awarded and levelUp() was never called by
            // anything -- the whole progression system was dead code.
            player->awardVictorySpoils(*opponent);
        }

        player->displayInfo();
        offerSave(store, *player);
        return;
    }
}

void playerVsPlayer()
{
    cout << "Create player 1" << endl;
    CharacterPtr first = newCharacter();
    if (!first)
    {
        return;
    }

    cout << "Now create player 2" << endl;
    CharacterPtr second = newCharacter();
    if (!second)
    {
        return;
    }

    cout << "Fight!" << endl << endl;
    const BattleOutcome outcome = runBattle(*first, *second, false);
    if (outcome == BattleOutcome::FirstWon)
    {
        first->awardVictorySpoils(*second);
    }
    else if (outcome == BattleOutcome::SecondWon)
    {
        second->awardVictorySpoils(*first);
    }
}

}  // namespace

int main(int argc, char **argv)
{
    (void)argc;
    // Anchors assets and saves to the executable's directory, so the game works
    // no matter where it is launched from.
    paths::init(argv[0]);

    srand(static_cast<unsigned>(time(0)));  // seeded once, not on every menu pass

    printASCII(paths::asset("art.txt"));

    SaveStore store;
    if (!store.open(paths::data("saves.db")))
    {
        cout << "Warning: saves are unavailable (" << store.lastError() << ")." << endl;
    }

    while (true)
    {
        cout << endl << "1. Player vs Bot" << endl << "2. Player vs Player" << endl << "3. Exit" << endl;

        int menuSelection = 0;
        if (!input::readInt("> ", 1, 3, menuSelection) || menuSelection == 3)
        {
            cout << "Exiting..." << endl;
            break;
        }

        if (menuSelection == 1)
        {
            playerVsBot(store);
        }
        else
        {
            playerVsPlayer();
        }
    }

    return 0;
}
