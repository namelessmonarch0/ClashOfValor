#ifndef INPUT_H
#define INPUT_H

#include <string>

// Console input that survives bad data.
//
// Every menu used to read with a bare `cin >> selection`. On non-numeric input
// that leaves the stream in a fail state, the value unwritten, and the
// surrounding `while` loop spinning forever -- typing "abc" at any prompt hung
// the game. These helpers clear the error, discard the offending line, and
// re-prompt.
namespace input
{
// Reads an integer in [min, max], re-prompting until one arrives.
// Returns false when stdin is exhausted (EOF), so callers can unwind rather
// than loop forever against a closed stream -- which matters when the game is
// driven by a pipe or a script.
bool readInt(const std::string &prompt, int min, int max, int &out);

// Reads a whole line, trimmed of surrounding whitespace. Rejects empty input.
// Returns false on EOF.
bool readLine(const std::string &prompt, std::string &out);
}  // namespace input

#endif
