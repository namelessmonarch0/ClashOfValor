#ifndef TERMINAL_H
#define TERMINAL_H

// Terminal size and interactivity queries.
//
// The game used to print a 162-column banner with a bare getline loop and no
// awareness of the terminal at all, so it wrapped and broke on anything
// narrower. Every size- or TTY-dependent decision in the game goes through
// this module so there is exactly one place that answers "how big is the
// terminal" and "are we even talking to one".
namespace terminal
{
struct Size
{
    int cols;
    int rows;
};

// Reads the real terminal size via ioctl(TIOCGWINSZ). Falls back to 80x24 if
// stdout is not a terminal or the ioctl fails (e.g. output is piped) -- that
// fallback matters less for layout, since isInteractive() gates whether any
// of the new terminal-control code runs at all, but callers still need a
// sane, non-zero value to divide by.
Size getSize();

// True only when both stdin and stdout are real terminals. Every piece of
// this session's new UI -- raw mode, the alternate screen, the splash, the
// arrow-key menus -- is gated on this. When it's false (make test, a scripted
// playthrough, a pipe, CI) the game must behave exactly as it always has:
// sequential prompts, no ANSI, no raw mode. That fallback is what keeps the
// existing regression suite passing without touching it.
bool isInteractive();

// Call once near startup (after isInteractive() is true) to catch SIGWINCH.
// Safe to call unconditionally; it is a no-op cost if never queried.
void watchForResize();

// True if a SIGWINCH has arrived since the last call, which also clears the
// flag. The main loop polls this between redraws rather than doing real work
// inside the signal handler.
bool resizePending();

}  // namespace terminal

#endif
