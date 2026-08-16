#ifndef ANIMATE_H
#define ANIMATE_H

#include <string>

// Short inline flourishes for combat actions.
namespace animate
{

// Plays a brief (~0.3-0.5s) sparkle flourish where the cursor currently is,
// settling into a permanent "+N LABEL" callout in `colorCode` (an ANSI
// 256-color index) -- e.g. potionFlourish(50, "HP", 82) after a health
// potion narration line. No-op when raw mode isn't active, so
// non-interactive runs (make test, piped play) are unaffected, the same
// gating pattern used throughout the rest of the raw-mode UI.
void potionFlourish(int amount, const std::string &label, int colorCode);

}  // namespace animate

#endif
