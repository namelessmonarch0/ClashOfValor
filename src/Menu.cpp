#include "Menu.h"

#include <unistd.h>

#include <algorithm>
#include <iostream>

#include "Input.h"
#include "RawTerminal.h"
#include "Terminal.h"

using namespace std;

namespace
{

// Redraws every item from the saved cursor position (set once, before the
// first frame, via DECSC \x1b7). Restoring to that same spot each time
// (DECRC \x1b8) is what lets this repaint in place instead of scrolling a new
// copy of the menu down the screen on every keypress.
void draw(const vector<string> &items, int highlighted, int leftPad)
{
    cout << "\x1b"
            "8";  // DECRC: back to the saved top-left of this menu block
    const string padding(static_cast<size_t>(leftPad), ' ');
    for (size_t i = 0; i < items.size(); ++i)
    {
        const string line = to_string(i + 1) + ". " + items[i];
        cout << padding;
        if (static_cast<int>(i) == highlighted)
        {
            // Reverse video stands in for the leading "> " the numeric-only
            // prompts used -- the ask was specifically no ">" text here.
            cout << "\x1b[7m" << line << "\x1b[0m";
        }
        else
        {
            cout << line;
        }
        cout << "\x1b[K"
             << "\r\n";  // clear any leftover tail from a previously longer line
    }
    cout.flush();
}

// Blanks the N rows this menu drew and leaves the cursor back at the anchor
// (not past it), so whatever prints next -- combat narration, or the next
// menu::choose() call's own fresh anchor -- overwrites the same rows rather
// than the terminal accumulating every menu a session has ever shown.
void eraseOwnLines(size_t count)
{
    cout << "\x1b"
            "8";  // DECRC, used only once: back to the anchor
    for (size_t i = 0; i < count; ++i)
    {
        cout << "\x1b[K"
                "\r\n";
    }
    // Move back up via a relative cursor-up (CUU) rather than a second
    // DECRC -- verified directly that repeated DECRC to the same saved slot
    // is not reliably idempotent across terminal implementations (pyte's
    // emulation resets to the screen origin on a second consecutive
    // restore instead of reusing the saved coordinates). CUU sidesteps that
    // ambiguity entirely: it is a plain relative move, nothing to restore.
    if (count > 0)
    {
        cout << "\x1b[" << count << "A";
    }
    cout.flush();
}

}  // namespace

namespace menu
{

int choose(const vector<string> &items, int defaultIndex)
{
    if (items.empty())
    {
        return -1;
    }

    if (!rawterm::isActive())
    {
        int value = 0;
        if (!input::readInt("> ", 1, static_cast<int>(items.size()), value))
        {
            return -1;
        }
        return value;
    }

    int highlighted = defaultIndex - 1;
    if (highlighted < 0 || highlighted >= static_cast<int>(items.size()))
    {
        highlighted = 0;
    }

    // Centered as a block: every line shares the same left margin, based on
    // the widest item, rather than each line centering to its own width --
    // that would look ragged. Computed once; the menu's own width doesn't
    // change while it's open.
    const terminal::Size size = terminal::getSize();
    int widest = 0;
    for (size_t i = 0; i < items.size(); ++i)
    {
        const int lineWidth = static_cast<int>(to_string(i + 1).size() + 2 + items[i].size());
        widest = max(widest, lineWidth);
    }
    const int leftPad = max(0, (size.cols - widest) / 2);

    cout << "\x1b"
            "7";  // DECSC: remember this spot as the top-left of the menu
    draw(items, highlighted, leftPad);

    while (true)
    {
        const rawterm::KeyEvent ev = rawterm::readKey();
        switch (ev.key)
        {
        case rawterm::Key::Up:
            highlighted = (highlighted - 1 + static_cast<int>(items.size())) % static_cast<int>(items.size());
            draw(items, highlighted, leftPad);
            break;

        case rawterm::Key::Down:
            highlighted = (highlighted + 1) % static_cast<int>(items.size());
            draw(items, highlighted, leftPad);
            break;

        case rawterm::Key::Enter:
            eraseOwnLines(items.size());
            return highlighted + 1;

        case rawterm::Key::Digit:
        {
            const int n = ev.ch - '0';
            if (n >= 1 && n <= static_cast<int>(items.size()))
            {
                // Show the jump before erasing so the choice is visible for
                // an instant rather than the menu just vanishing -- without
                // the delay, draw() and eraseOwnLines() happen close enough
                // together that the highlight would never actually be seen.
                highlighted = n - 1;
                draw(items, highlighted, leftPad);
                usleep(100 * 1000);
                eraseOwnLines(items.size());
                return n;
            }
            break;  // out of range for this menu; ignore and keep waiting
        }

        case rawterm::Key::Eof:
            eraseOwnLines(items.size());
            return -1;

        case rawterm::Key::Interrupted:
            // A signal (SIGWINCH, most likely) broke the blocking read with
            // nothing typed. Re-poll rather than treating it as a keypress.
            break;

        case rawterm::Key::Left:
        case rawterm::Key::Right:
        case rawterm::Key::Escape:
        case rawterm::Key::Backspace:
        case rawterm::Key::Char:
            break;  // not meaningful here; keep waiting
        }
    }
}

}  // namespace menu
