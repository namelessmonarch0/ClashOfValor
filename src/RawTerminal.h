#ifndef RAW_TERMINAL_H
#define RAW_TERMINAL_H

#include <sys/types.h>  // pid_t

// RAII terminal-mode guard: raw input mode + the alternate screen buffer.
//
// Constructing a RawTerminal switches the real terminal into raw/cbreak mode
// (no line buffering, no local echo -- keystrokes arrive one at a time
// instead of after Enter) and swaps to the alternate screen, the same
// mechanism vim and less use so the game's full-screen UI does not clobber
// the user's shell scrollback. Destroying it undoes both, in the opposite
// order, unconditionally -- including on SIGINT/SIGTERM, which this class
// also installs handlers for. A raw-mode session that dies without restoring
// the terminal leaves the user's shell echo-less and reading from the wrong
// screen buffer, which is a well-known way to make a terminal app hostile to
// use.
//
// Only ever construct this after terminal::isInteractive() is true.
namespace rawterm
{

class RawTerminal
{
public:
    RawTerminal();
    ~RawTerminal();

    bool isActive() const { return active; }

private:
    RawTerminal(const RawTerminal &);
    RawTerminal &operator=(const RawTerminal &);

    bool active;
};

// A single logical keypress, decoded from however many raw bytes it took
// (arrow keys arrive as a 3-byte escape sequence; everything else is one
// byte). Reading is only meaningful while a RawTerminal is active.
enum class Key
{
    Up,
    Down,
    Left,
    Right,
    Enter,
    Escape,  // a bare Esc, i.e. not the start of a recognized escape sequence
    Backspace,
    Digit,    // ch holds '0'..'9'
    Char,     // ch holds the raw character
    Eof,      // stdin closed
    Interrupted  // a signal (e.g. resize) broke the read; caller should re-poll
};

struct KeyEvent
{
    Key key;
    char ch;
};

// True while raw (non-canonical, non-echoing) mode is actually in effect right
// now -- not just "a terminal is attached", but "single-keystroke reads via
// readKey() will behave correctly this instant". False before any
// RawTerminal exists, after one is destroyed, and during a suspend()/resume()
// window (e.g. while the caller is doing ordinary line-buffered name entry).
// This is what Menu gates its raw-input UI on, rather than
// terminal::isInteractive(), since that only says raw mode *could* work, not
// that it currently does.
bool isActive();

// Temporarily hands canonical (line-buffered, echoing) mode back, without
// leaving the alternate screen or touching the scroll region. Used around
// character-name entry, which wants ordinary backspace/Enter line editing
// rather than a hand-rolled line editor. Free functions rather than
// RawTerminal methods: name entry happens several call frames away from
// main()'s single RawTerminal instance (newCharacter() is called from
// playerVsBot()/playerVsPlayer(), neither of which holds a reference to it),
// and threading that reference through every intermediate call is more
// churn than it's worth when isActive() and reassert() already work this
// way. Both are no-ops if no RawTerminal was ever successfully constructed,
// so callers do not need to guard on isActive() first.
void suspend();
void resume();

// Forcibly re-applies raw/cbreak mode, independent of whatever isActive()'s
// bookkeeping currently believes. Needed after running a child process (e.g.
// ttfx for the splash) that shares this terminal: some such programs reset
// termios to a canonical/echoing default as part of their own startup or
// teardown -- observed directly on this build, where ttfx's mere presence
// silently flips ICANON and ECHO back on even though nothing in this
// codebase ever calls suspend(). Termios belongs to the tty device, not to
// any one process, so nothing guarantees a child process leaves it as it
// found it. A no-op if no RawTerminal was ever successfully constructed.
void reassert();

// Blocks until one key event is available.
KeyEvent readKey();

// Registers a child process (e.g. ttfx, forked by Intro to play the splash)
// to be killed if a fatal signal arrives before it exits on its own. Without
// this, Ctrl-C during the splash would restore the terminal and end this
// process while ttfx kept running as an orphan for the rest of its ~8-second
// effect, animating into a pty nobody is reading from anymore -- visibly
// garbling the user's shell prompt seconds after they thought they had quit.
// Pass 0 once the child has been reaped normally, so a later signal can't
// target a pid that's since been reused by an unrelated process.
void registerChildForCleanup(pid_t pid);

// True if a byte is waiting within timeoutMs, without consuming it. Used by
// the splash screen to poll for a skip keypress alongside waiting on a child
// process.
bool keyAvailable(int timeoutMs);

}  // namespace rawterm

#endif
