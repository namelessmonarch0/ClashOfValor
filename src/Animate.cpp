#include "Animate.h"

#include <unistd.h>

#include <cstddef>
#include <iostream>

#include "RawTerminal.h"

using namespace std;

namespace animate
{

void potionFlourish(int amount, const string &label, int colorCode)
{
    if (!rawterm::isActive())
    {
        return;
    }

    // Each frame prints on a fresh line, then a relative cursor-up (CUU)
    // brings the cursor back to overwrite that same row for the next frame
    // -- deliberately not DECSC/DECRC (\x1b7/\x1b8): verified directly
    // during this session that a *second* DECRC to the same saved position
    // is not reliably idempotent (confirmed against a real terminal
    // emulation library, where it silently jumped to the screen origin
    // instead of reusing the saved coordinates). A plain relative move has
    // no such ambiguity.
    static const char *frames[] = {"✧ ✦ ✧", "✦ ✧ ✦"};
    const size_t frameCount = sizeof(frames) / sizeof(frames[0]);

    for (size_t i = 0; i < frameCount; ++i)
    {
        cout << "  \x1b[38;5;" << colorCode << "m" << frames[i] << "\x1b[0m\x1b[K\r\n";
        cout.flush();
        usleep(120 * 1000);
        cout << "\x1b[1A";
    }

    // The settled line stays -- an ordinary log entry from here on, like any
    // other narration, so it composes correctly with whatever the next
    // menu::choose() anchors after it.
    cout << "  \x1b[38;5;" << colorCode << "m+" << amount << " " << label << "\x1b[0m\x1b[K\r\n";
    cout.flush();
}

}  // namespace animate
