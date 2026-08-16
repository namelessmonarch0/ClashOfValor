#ifndef INTRO_H
#define INTRO_H

// Plays the "CLASH OF VALOR" splash once, at startup.
//
// Interactive sessions (raw mode active): tries ttfx's `burn` effect first --
// found on PATH, or at the `cargo install` default location if PATH doesn't
// happen to include it -- run as a child process against the splash art file
// so the effect animates directly to the real terminal. It is always
// skippable on the first keypress, which is not something ttfx offers on its
// own: this module polls for a keystroke alongside the child's exit status
// and kills it the moment one arrives. If ttfx cannot be found (or fails to
// launch), a native fallback reveals the same art line by line under a warm
// color gradient -- not a reimplementation of `burn`, just a same-spirit
// effect with zero new dependencies.
//
// Non-interactive sessions (piped input, `make test`, CI): prints the
// best-fitting splash tier once, instantly, and returns -- there is no one
// watching an animation play out on the other end of a pipe.
namespace intro
{
void playSplash();
}

#endif
