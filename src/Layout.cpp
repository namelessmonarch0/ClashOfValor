#include "Layout.h"

#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Paths.h"

using namespace std;

namespace
{

struct Art
{
    vector<string> lines;
    int width;  // display columns of the widest line
};

// Counts Unicode codepoints, not bytes. The generated banner assets are plain
// ASCII so this is equivalent to length() for them, but treating a run of
// UTF-8 continuation bytes (10xxxxxx) as a single column is what keeps this
// correct if a banner ever uses box-drawing or block-element glyphs again --
// both are single-column in virtually every terminal font despite being
// multi-byte in UTF-8, which is not the same question as their *byte* length.
int displayWidth(const string &line)
{
    int width = 0;
    for (size_t i = 0; i < line.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(line[i]);
        if ((c & 0xC0) != 0x80)  // not a continuation byte -> starts a new codepoint
        {
            width++;
        }
    }
    return width;
}

bool loadArt(const string &assetName, Art &out)
{
    ifstream in(paths::asset(assetName).c_str());
    if (!in.is_open())
    {
        return false;
    }
    out.lines.clear();
    out.width = 0;
    string line;
    while (getline(in, line))
    {
        // Trailing \r would otherwise count as an extra column and, worse,
        // land mid-line once printed.
        if (!line.empty() && line[line.size() - 1] == '\r')
        {
            line.erase(line.size() - 1);
        }
        out.width = max(out.width, displayWidth(line));
        out.lines.push_back(line);
    }
    return !out.lines.empty();
}

void printCentered(const Art &art, int termWidth)
{
    const int pad = max(0, (termWidth - art.width) / 2);
    const string padding(pad, ' ');
    for (size_t i = 0; i < art.lines.size(); ++i)
    {
        cout << padding << art.lines[i] << "\r\n";
    }
}

// Tries each tier from most to least detailed and returns the first one that
// fits both dimensions, along with which asset name was chosen (needed by
// pickSplashAssetPath(), which wants the file rather than its printed form).
// `reserveRows` is headroom the caller needs beyond the art itself (e.g. a
// "press any key" line for the splash, or the minimum scroll area for the
// header). Always succeeds if at least one tier could be read at all: the
// last tier tried is expected to be a one-line literal fallback that fits
// nearly anywhere.
bool pickFittingTier(const vector<string> &tierAssetNames, int termWidth, int termHeight, int reserveRows, Art &out,
                     string &outName)
{
    for (size_t i = 0; i < tierAssetNames.size(); ++i)
    {
        Art candidate;
        if (!loadArt(tierAssetNames[i], candidate))
        {
            continue;
        }
        const int neededRows = static_cast<int>(candidate.lines.size()) + reserveRows;
        const bool isLastTier = (i + 1 == tierAssetNames.size());
        if (isLastTier || (candidate.width <= termWidth && neededRows <= termHeight))
        {
            out = candidate;
            outName = tierAssetNames[i];
            return true;
        }
    }
    return false;
}

vector<string> splashTierNames()
{
    vector<string> tiers;
    tiers.push_back("splash-wide.txt");
    tiers.push_back("splash-compact.txt");
    tiers.push_back("splash-plain.txt");
    return tiers;
}

}  // namespace

namespace layout
{

void printSplash(int termWidth, int termHeight)
{
    Art art;
    string chosenName;
    // Reserve 2 rows: a blank line plus a "press any key" hint printed by the
    // caller once the art is up.
    if (!pickFittingTier(splashTierNames(), termWidth, termHeight, 2, art, chosenName))
    {
        return;  // no splash asset could be read at all; not fatal, just skip it
    }
    printCentered(art, termWidth);
}

string pickSplashAssetPath(int termWidth, int termHeight)
{
    Art art;
    string chosenName;
    if (!pickFittingTier(splashTierNames(), termWidth, termHeight, 2, art, chosenName))
    {
        return "";
    }
    return paths::asset(chosenName);
}

int drawHeader(int termWidth, int termHeight, int minScrollRows)
{
    vector<string> tiers;
    tiers.push_back("header-wide.txt");
    tiers.push_back("header-compact.txt");
    tiers.push_back("header-plain.txt");

    Art art;
    string chosenName;
    if (!pickFittingTier(tiers, termWidth, termHeight, minScrollRows, art, chosenName))
    {
        return 0;  // nothing to draw; leave the scroll region untouched
    }

    cout << "\x1b[H";  // cursor to row 1, col 1
    printCentered(art, termWidth);

    const int headerRows = static_cast<int>(art.lines.size());
    const int scrollTop = headerRows + 1;
    if (scrollTop < termHeight)
    {
        // DECSTBM: everything printed from here on is confined to rows
        // scrollTop..termHeight by the terminal itself, not by any bookkeeping
        // this program does.
        cout << "\x1b[" << scrollTop << ";" << termHeight << "r";
        cout << "\x1b[" << scrollTop << ";1H";
    }
    cout.flush();
    return headerRows;
}

}  // namespace layout
