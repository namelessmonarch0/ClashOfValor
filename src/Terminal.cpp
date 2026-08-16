#include "Terminal.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <csignal>
#include <cstdlib>

namespace
{
// sig_atomic_t, not bool: the only type the C++ standard guarantees is safe
// to write from a signal handler without a data race.
volatile sig_atomic_t g_resizeFlag = 0;

extern "C" void onSigwinch(int) { g_resizeFlag = 1; }
}  // namespace

namespace terminal
{

Size getSize()
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0)
    {
        Size s;
        s.cols = ws.ws_col;
        s.rows = ws.ws_row;
        return s;
    }
    Size fallback;
    fallback.cols = 80;
    fallback.rows = 24;
    return fallback;
}

bool isInteractive() { return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO); }

void watchForResize()
{
    struct sigaction sa;
    sa.sa_handler = onSigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, 0);
}

bool resizePending()
{
    if (g_resizeFlag)
    {
        g_resizeFlag = 0;
        return true;
    }
    return false;
}

}  // namespace terminal
