#ifndef LAYOUT_H
#define LAYOUT_H

#include <string>
#include <vector>

// Width/height-aware art rendering for the two banners the game uses: the
// full-screen splash ("CLASH OF VALOR") and the pinned header ("hello,
// traveler") that stays on screen underneath it.
//
// Both banners ship three pre-rendered tiers (wide / compact / plain) rather
// than being generated on the fly, because a hand-picked figlet font at a
// sane size reads far better than anything a generic scaling algorithm would
// produce from the same source art. The job here is picking the widest tier
// that actually fits and centering it -- not resizing glyphs.
namespace layout
{

// Prints the best-fitting splash tier, centered, for the given terminal size.
// Never throws for an unreasonably small terminal: the plain tier is a single
// line of literal text and always fits anywhere the game is likely to run.
void printSplash(int termWidth, int termHeight);

// A splash tier fully composed for a given terminal size: horizontally
// centered (leading spaces baked into each art line) and vertically centered
// within whatever space remains below `reservedTopRows` (leading blank
// lines). `artStartIndex` marks where the real content begins, so a caller
// animating line-by-line (Intro's native reveal) can print the blank padding
// instantly and only animate the art itself. Empty `lines` if no tier's
// asset could be read at all.
struct SplashBlock
{
    std::vector<std::string> lines;
    int artStartIndex;
};
SplashBlock composeSplash(int termWidth, int termHeight, int reservedTopRows);

// Draws the best-fitting header tier at the top of the screen, centered, and
// pins it with a VT100 scrolling region (DECSTBM) so everything printed
// afterwards scrolls beneath it instead of overwriting it. `minScrollRows` is
// the smallest scroll area worth reserving a multi-line header for; below
// that the header collapses to its one-line tier regardless of width, since a
// header that eats the whole screen defeats its own purpose.
//
// Returns the number of rows the header occupies, i.e. the first row of the
// scrolling region is (return value + 1).
int drawHeader(int termWidth, int termHeight, int minScrollRows = 6);

}  // namespace layout

#endif
