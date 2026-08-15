# Clash of Valor

A turn-based terminal RPG in C++. Pick a class, fight a randomly generated
opponent or a friend at the same keyboard, and carry your character forward
between sessions.

## Build and run

```
make        # build ./game
make run    # build and play
make test   # build and run the regression suite
make clean  # remove build/ and the binary
```

Requires a C++11 compiler and SQLite. SQLite ships with macOS (Command Line
Tools) and is available as `libsqlite3-dev` on Debian/Ubuntu; nothing else needs
installing.

## Classes

| Class | Mechanic |
|---|---|
| Warrior | Carries a shield that absorbs damage before health, and can re-brace it mid-fight |
| Assassin | Critical strike deals 1.5x damage for double the stamina |
| Sorcerer | Spends mana on fire orbs that hit harder than steel; resting restores mana |

All three spend stamina to act and recover it by resting. Winning a fight awards
xp and coins, and enough xp raises your level, max health, and damage.

## Saving

Characters are stored in a SQLite database, `saves.db`, created next to the
binary on first run. The load menu lists everything saved, so you pick a
character by number rather than typing a filename.

Inspect saves directly with:

```
sqlite3 saves.db "SELECT name, class, level, health, xp, coins FROM characters;"
```

## Layout

```
src/            all sources and headers, co-located per class
tests/          regression suite (make test)
assets/         art.txt and any future data files
build/          objects and binaries, generated (gitignored)
```

Headers sit beside their implementations rather than in a separate `include/`
tree: the game ships no public API, so a split would add a second place to edit
for every change without buying anything.

Assets and saves are resolved relative to the executable, so the game runs
correctly from any working directory.
