#ifndef EXTERNAL_TOOL_H
#define EXTERNAL_TOOL_H

#include <string>

// Locating optional external binaries (ttfx for the splash, chafa for the
// header) that the game enhances itself with when present but degrades
// gracefully without.
namespace externaltool
{

// Manual PATH search plus one fallback: `cargo install`'s default install
// root, ~/.cargo/bin, which is not guaranteed to be on PATH even immediately
// after installing -- confirmed empirically on the machine this was built
// on, where `which ttfx` failed right after `cargo install --git ... ttfx`
// succeeded. Homebrew-installed tools (chafa) are already on the standard
// PATH, so this fallback is a no-op for them; kept unconditional since it's
// harmless either way and keeps one search routine for every optional tool.
// Returns an empty string if `name` cannot be found or is not executable.
std::string findExecutable(const std::string &name);

}  // namespace externaltool

#endif
