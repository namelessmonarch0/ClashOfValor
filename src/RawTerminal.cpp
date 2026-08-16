#include "RawTerminal.h"

#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>

namespace
{

// Signal handlers can only safely touch plain data and call a short list of
// async-signal-safe functions (write(), tcsetattr(), _exit() among them) --
// not iostream, not a class method through a `this` we would have to smuggle
// out. Keeping the restore state as file-static globals, read by both the
// class methods and the handler, is what makes that safe.
struct termios g_savedTermios;
bool g_haveSavedTermios = false;
bool g_rawActive = false;      // ICANON/ECHO currently off
bool g_altScreenActive = false;
volatile pid_t g_childPid = 0;  // a live splash child to kill on a fatal signal, if any

void writeRaw(const char *s)
{
    size_t len = 0;
    while (s[len] != '\0')
    {
        len++;
    }
    ssize_t ignored = ::write(STDOUT_FILENO, s, len);
    (void)ignored;  // best-effort during cleanup; nothing sensible to do if it fails
}

// Leaves the terminal exactly as it was before any RawTerminal existed.
// Idempotent and callable from a signal handler.
void restoreTerminalNow()
{
    if (g_rawActive && g_haveSavedTermios)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_savedTermios);
        g_rawActive = false;
    }
    if (g_altScreenActive)
    {
        writeRaw("\x1b[r");       // reset the scrolling region Layout may have set
        writeRaw("\x1b[?25h");    // show cursor
        writeRaw("\x1b[?1049l");  // leave the alternate screen
        g_altScreenActive = false;
    }
}

// The cbreak termios this whole module wants applied whenever raw mode is
// active: canonical line-buffering and local echo off, single-byte reads.
// ISIG stays on -- Ctrl-C still delivers SIGINT rather than arriving as a
// byte we would have to interpret ourselves.
struct termios cbreakFrom(const struct termios &base)
{
    struct termios cbreak = base;
    cbreak.c_lflag &= ~(static_cast<unsigned long>(ICANON) | static_cast<unsigned long>(ECHO));
    cbreak.c_cc[VMIN] = 1;
    cbreak.c_cc[VTIME] = 0;
    return cbreak;
}

extern "C" void onFatalSignal(int sig)
{
    if (g_childPid > 0)
    {
        // SIGKILL, not a graceful SIGTERM-then-wait: this handler is racing
        // to exit and kill() is async-signal-safe, unlike a wait loop with
        // sleeps. Not reaping the child here is fine -- it becomes init's
        // responsibility once this process is gone, same as any orphan.
        kill(g_childPid, SIGKILL);
    }
    restoreTerminalNow();
    // _exit, not exit: skips atexit handlers and stdio flushing, both of
    // which are unsafe to run from a signal handler. Nothing buffered in
    // iostream matters once the terminal has already been restored with a
    // direct write().
    _exit(128 + sig);
}

void installSignalHandlers()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = onFatalSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);
}

}  // namespace

namespace rawterm
{

RawTerminal::RawTerminal() : active(false)
{
    if (tcgetattr(STDIN_FILENO, &g_savedTermios) != 0)
    {
        return;  // not a real terminal after all; stay inactive
    }
    g_haveSavedTermios = true;

    // The signal handler installed below is what actually restores the
    // terminal before the process exits.
    const struct termios cbreak = cbreakFrom(g_savedTermios);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &cbreak) != 0)
    {
        return;
    }
    g_rawActive = true;

    writeRaw("\x1b[?1049h");  // alternate screen
    writeRaw("\x1b[?25l");    // hide cursor
    g_altScreenActive = true;

    installSignalHandlers();
    active = true;
}

RawTerminal::~RawTerminal()
{
    if (active)
    {
        restoreTerminalNow();
        active = false;
    }
}

bool isActive() { return g_rawActive; }

void suspend()
{
    if (!g_haveSavedTermios || !g_rawActive)
    {
        return;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &g_savedTermios);
    g_rawActive = false;
}

void resume()
{
    if (!g_haveSavedTermios || g_rawActive)
    {
        return;
    }
    const struct termios cbreak = cbreakFrom(g_savedTermios);
    tcsetattr(STDIN_FILENO, TCSANOW, &cbreak);
    g_rawActive = true;
}

void reassert()
{
    if (!g_haveSavedTermios)
    {
        return;
    }
    const struct termios cbreak = cbreakFrom(g_savedTermios);
    tcsetattr(STDIN_FILENO, TCSANOW, &cbreak);
    g_rawActive = true;
}

void registerChildForCleanup(pid_t pid) { g_childPid = pid; }

bool keyAvailable(int timeoutMs)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    const int rv = select(STDIN_FILENO + 1, &fds, 0, 0, &tv);
    return rv > 0 && FD_ISSET(STDIN_FILENO, &fds);
}

KeyEvent readKey()
{
    unsigned char c = 0;
    const ssize_t n = ::read(STDIN_FILENO, &c, 1);
    if (n <= 0)
    {
        if (n < 0 && errno == EINTR)
        {
            KeyEvent ev = {Key::Interrupted, 0};
            return ev;
        }
        KeyEvent ev = {Key::Eof, 0};
        return ev;
    }

    if (c == '\r' || c == '\n')
    {
        KeyEvent ev = {Key::Enter, static_cast<char>(c)};
        return ev;
    }
    if (c == 127 || c == 8)
    {
        KeyEvent ev = {Key::Backspace, static_cast<char>(c)};
        return ev;
    }
    if (c >= '0' && c <= '9')
    {
        KeyEvent ev = {Key::Digit, static_cast<char>(c)};
        return ev;
    }

    if (c == 0x1b)
    {
        // A real arrow-key sequence (ESC [ A/B/C/D) arrives from the terminal
        // driver as an atomic unit; a human pressing Esc alone does not
        // immediately follow it with '['. A short wait is what tells the two
        // apart without permanently mistaking a lone Esc for the start of a
        // sequence that never arrives.
        if (!keyAvailable(50))
        {
            KeyEvent ev = {Key::Escape, 0};
            return ev;
        }
        unsigned char c2 = 0;
        if (::read(STDIN_FILENO, &c2, 1) <= 0 || c2 != '[')
        {
            KeyEvent ev = {Key::Escape, 0};
            return ev;
        }
        if (!keyAvailable(50))
        {
            KeyEvent ev = {Key::Escape, 0};
            return ev;
        }
        unsigned char c3 = 0;
        if (::read(STDIN_FILENO, &c3, 1) <= 0)
        {
            KeyEvent ev = {Key::Escape, 0};
            return ev;
        }
        switch (c3)
        {
        case 'A':
        {
            KeyEvent ev = {Key::Up, 0};
            return ev;
        }
        case 'B':
        {
            KeyEvent ev = {Key::Down, 0};
            return ev;
        }
        case 'C':
        {
            KeyEvent ev = {Key::Right, 0};
            return ev;
        }
        case 'D':
        {
            KeyEvent ev = {Key::Left, 0};
            return ev;
        }
        default:
        {
            KeyEvent ev = {Key::Escape, 0};
            return ev;
        }
        }
    }

    KeyEvent ev = {Key::Char, static_cast<char>(c)};
    return ev;
}

}  // namespace rawterm
