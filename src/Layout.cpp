#include "Layout.h"

#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "ExternalTool.h"
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

// Renders assets/header.png through chafa, sized to the current terminal,
// for the pinned header. Unlike the splash's fixed tiers, this scales
// continuously -- a different terminal width gets a differently-sized
// render, not a jump between discrete breakpoints. Returns false (leaving
// `out` untouched) if chafa isn't installed, the render fails, or the
// result doesn't fit -- the caller falls back to the plain-text tiers in
// that case, the same way the splash falls back to nativeReveal() when ttfx
// isn't available.
bool tryChafaHeader(int termWidth, int termHeight, int minScrollRows, Art &out)
{
    const string chafaPath = externaltool::findExecutable("chafa");
    if (chafaPath.empty())
    {
        return false;
    }

    // A conservative target width: a cap so the header never dominates a
    // very wide terminal, and a floor below which chafa's own resolution
    // stops reading as text (verified empirically during planning -- ~50
    // columns was the practical lower bound for "hello traveler" staying
    // legible).
    const int targetWidth = min(termWidth - 4, 70);
    if (targetWidth < 30)
    {
        return false;
    }

    const string pngPath = paths::asset("header.png");
    const string cmd = "\"" + chafaPath + "\" -f symbols \"" + pngPath + "\" --size " + to_string(targetWidth) +
                       "x --symbols block -c none 2>/dev/null";
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        return false;
    }

    string output;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0)
    {
        output.append(buf, n);
    }
    const int rc = pclose(pipe);
    if (rc != 0 || output.empty())
    {
        return false;
    }

    Art art;
    string line;
    for (size_t i = 0; i < output.size(); ++i)
    {
        if (output[i] == '\n')
        {
            art.width = max(art.width, displayWidth(line));
            art.lines.push_back(line);
            line.clear();
        }
        else if (output[i] != '\r')
        {
            line += output[i];
        }
    }
    if (!line.empty())
    {
        art.width = max(art.width, displayWidth(line));
        art.lines.push_back(line);
    }
    // chafa pads its canvas with blank rows/columns to fill the requested
    // box exactly; trim the blank rows so the header's height reflects the
    // glyphs actually drawn, not chafa's fixed output grid.
    while (!art.lines.empty() && art.lines.back().find_first_not_of(' ') == string::npos)
    {
        art.lines.pop_back();
    }
    while (!art.lines.empty() && art.lines.front().find_first_not_of(' ') == string::npos)
    {
        art.lines.erase(art.lines.begin());
    }

    if (art.lines.empty())
    {
        return false;
    }
    const int neededRows = static_cast<int>(art.lines.size()) + minScrollRows;
    if (art.width > termWidth || neededRows > termHeight)
    {
        return false;
    }

    out = art;
    return true;
}

}  // namespace

namespace layout
{

void printSplash(int termWidth, int termHeight)
{
    // No skip hint is ever printed on this (non-interactive) path, so there
    // is nothing to reserve room for.
    const SplashBlock block = composeSplash(termWidth, termHeight, 0);
    for (size_t i = 0; i < block.lines.size(); ++i)
    {
        cout << block.lines[i] << "\r\n";
    }
}

SplashBlock composeSplash(int termWidth, int termHeight, int reservedTopRows)
{
    SplashBlock block;
    block.artStartIndex = 0;

    Art art;
    string chosenName;
    if (!pickFittingTier(splashTierNames(), termWidth, termHeight, reservedTopRows, art, chosenName))
    {
        return block;  // no splash asset could be read at all; not fatal, just nothing to show
    }

    const int availableRows = max(0, termHeight - reservedTopRows);
    const int topPad = max(0, (availableRows - static_cast<int>(art.lines.size())) / 2);
    for (int i = 0; i < topPad; ++i)
    {
        block.lines.push_back("");
    }
    block.artStartIndex = static_cast<int>(block.lines.size());

    const int pad = max(0, (termWidth - art.width) / 2);
    const string padding(pad, ' ');
    for (size_t i = 0; i < art.lines.size(); ++i)
    {
        block.lines.push_back(padding + art.lines[i]);
    }
    return block;
}

int drawHeader(int termWidth, int termHeight, int minScrollRows)
{
    Art art;
    bool haveArt = tryChafaHeader(termWidth, termHeight, minScrollRows, art);

    if (!haveArt)
    {
        // header-wide.txt (a plain-ASCII figlet substitute for the original
        // block-glyph art) is deliberately not in this list any more --
        // chafa now serves the role that tier played. These two remain as
        // the fallback when chafa isn't installed or nothing fits.
        vector<string> tiers;
        tiers.push_back("header-compact.txt");
        tiers.push_back("header-plain.txt");
        string chosenName;
        haveArt = pickFittingTier(tiers, termWidth, termHeight, minScrollRows, art, chosenName);
    }

    if (!haveArt)
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
