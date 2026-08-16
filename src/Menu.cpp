#include "Menu.h"

#include <iostream>

#include "Input.h"
#include "RawTerminal.h"

using namespace std;

namespace
{

// Redraws every item from the saved cursor position (set once, before the
// first frame, via DECSC \x1b7). Restoring to that same spot each time
// (DECRC \x1b8) is what lets this repaint in place instead of scrolling a new
// copy of the menu down the screen on every keypress.
void draw(const vector<string> &items, int highlighted)
{
    cout << "\x1b"
            "8";  // DECRC: back to the saved top-left of this menu block
    for (size_t i = 0; i < items.size(); ++i)
    {
        const string line = to_string(i + 1) + ". " + items[i];
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

    cout << "\x1b"
            "7";  // DECSC: remember this spot as the top-left of the menu
    draw(items, highlighted);

    while (true)
    {
        const rawterm::KeyEvent ev = rawterm::readKey();
        switch (ev.key)
        {
        case rawterm::Key::Up:
            highlighted = (highlighted - 1 + static_cast<int>(items.size())) % static_cast<int>(items.size());
            draw(items, highlighted);
            break;

        case rawterm::Key::Down:
            highlighted = (highlighted + 1) % static_cast<int>(items.size());
            draw(items, highlighted);
            break;

        case rawterm::Key::Enter:
            return highlighted + 1;

        case rawterm::Key::Digit:
        {
            const int n = ev.ch - '0';
            if (n >= 1 && n <= static_cast<int>(items.size()))
            {
                // Show the jump before returning so the choice is visible for
                // an instant rather than the menu just vanishing.
                highlighted = n - 1;
                draw(items, highlighted);
                return n;
            }
            break;  // out of range for this menu; ignore and keep waiting
        }

        case rawterm::Key::Eof:
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
