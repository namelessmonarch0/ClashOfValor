#ifndef MENU_H
#define MENU_H

#include <string>
#include <vector>

// Arrow-key-highlighted menu, with direct number-key selection kept as the
// primary path.
//
// TTY path (raw mode active): each item is drawn on its own line with its
// number, e.g. "2. Assassin" -- no leading "> " prompt. The currently
// highlighted item is shown in reverse video instead. Up/Down move the
// highlight and wrap; a digit key matching a visible item's number selects
// and confirms it immediately, since that's still the expected default way to
// play; Enter confirms whatever is currently highlighted.
//
// Non-TTY fallback (piped input, `make test`, CI): falls straight through to
// the existing input::readInt("> ", ...) prompt, completely unchanged. This
// is what keeps the regression suite passing without going anywhere near raw
// mode.
namespace menu
{

// Returns the 1-based index of the chosen item, or -1 if input was
// exhausted (EOF) or the underlying read failed -- callers should treat that
// the same way they already treat input::readInt returning false.
int choose(const std::vector<std::string> &items, int defaultIndex = 1);

}  // namespace menu

#endif
