#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Assassin.h"
#include "Character.h"
#include "Input.h"
#include "Intro.h"
#include "Layout.h"
#include "Menu.h"
#include "Paths.h"
#include "RawTerminal.h"
#include "SaveStore.h"
#include "Sorcerer.h"
#include "Terminal.h"
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
    // Ordinary line editing (visible echo, backspace, Enter to finish) for
    // the one prompt that asks for free text rather than a menu choice.
    // No-ops harmlessly when raw mode was never active.
    rawterm::suspend();
    string playerName;
    const bool gotName = input::readLine("Enter your character name: ", playerName);
    rawterm::resume();
    if (!gotName)
    {
        return CharacterPtr();
    }

    cout << "Select your class:" << endl;
    cout << "------------------" << endl;
    vector<string> classItems;
    classItems.push_back("Warrior  (carries a shield that absorbs damage before health)");
    classItems.push_back("Assassin (trades survivability for a high-damage critical strike)");
    classItems.push_back("Sorcerer (spends mana on fire orbs that hit harder than steel)");

    const int classSelection = menu::choose(classItems);
    if (classSelection < 0)
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
    vector<string> items;
    for (size_t i = 0; i < saved.size(); ++i)
    {
        items.push_back(saved[i].first + " (" + saved[i].second + ")");
    }

    const int choice = menu::choose(items);
    if (choice < 0)
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
    cout << "Save this character?" << endl;
    vector<string> items;
    items.push_back("Yes, save");
    items.push_back("No, discard");
    if (menu::choose(items) != 1)
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
        cout << endl;
        vector<string> items;
        items.push_back("New Character");
        items.push_back("Load Character");
        items.push_back("Back");
        const int choice = menu::choose(items);
        if (choice < 0 || choice == 3)
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

    // Full-screen mode is opt-in by circumstance, not configuration: it only
    // activates when both stdin and stdout are real terminals. Piped input,
    // `make test`, and CI all fall through untouched, which is what keeps the
    // existing regression suite passing without knowing any of this exists.
    unique_ptr<rawterm::RawTerminal> rawSession;
    if (terminal::isInteractive())
    {
        terminal::watchForResize();
        rawSession.reset(new rawterm::RawTerminal());
        if (!rawSession->isActive())
        {
            // tcgetattr/tcsetattr failed despite isatty() succeeding -- rare,
            // but fall back to the plain sequential experience rather than
            // half-apply a broken raw mode.
            rawSession.reset();
        }
    }

    // Raw mode has to be up *before* the splash, not after: skipping it is
    // detected by polling for a single keystroke, which only works once
    // stdin is in cbreak mode. intro::playSplash() checks rawterm::isActive()
    // itself and falls back to a single plain print when it isn't (piped
    // input, or the rare raw-mode failure above).
    if (rawSession)
    {
        cout << "\x1b[2J\x1b[H";  // fresh canvas for the splash
    }
    intro::playSplash();
    if (rawSession)
    {
        cout << "\x1b[2J\x1b[H";  // the splash disappears before the header appears
        const terminal::Size size = terminal::getSize();
        layout::drawHeader(size.cols, size.rows);
    }

    SaveStore store;
    if (!store.open(paths::data("saves.db")))
    {
        cout << "Warning: saves are unavailable (" << store.lastError() << ")." << endl;
    }

    while (true)
    {
        // Reflow is lazy by design: it applies at the next natural redraw
        // point (here, returning to the top-level menu) rather than
        // interrupting whatever prompt is currently blocked on input. Doing
        // better would mean giving every blocking `cin >>` in Input.cpp an
        // EINTR-aware retry loop, which starts to matter once Menu::choose()
        // (Phase C) reads single keystrokes directly and can poll for a
        // resize between them -- not before.
        if (rawSession && terminal::resizePending())
        {
            const terminal::Size size = terminal::getSize();
            layout::drawHeader(size.cols, size.rows);
        }

        cout << endl;
        vector<string> topItems;
        topItems.push_back("Player vs Bot");
        topItems.push_back("Player vs Player");
        topItems.push_back("Exit");
        const int menuSelection = menu::choose(topItems);
        if (menuSelection < 0 || menuSelection == 3)
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
