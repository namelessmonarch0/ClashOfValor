#ifndef PATHS_H
#define PATHS_H

#include <string>

// Locates files relative to the executable rather than the current working
// directory.
//
// The game used to open "art.txt" and "saves.db" as bare relative paths, so it
// only behaved correctly when launched from the repository root. Running
// `/path/to/game` from anywhere else silently lost the title banner and created
// a stray saves.db in whatever directory you happened to be in.
namespace paths
{
// Call once from main() with argv[0] before requesting any path.
void init(const char *argv0);

// assets/<name>, next to the executable.
std::string asset(const std::string &name);

// <name> alongside the executable -- used for saves.db.
std::string data(const std::string &name);
}  // namespace paths

#endif
