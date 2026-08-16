#include "Intro.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "ExternalTool.h"
#include "Layout.h"
#include "RawTerminal.h"
#include "Terminal.h"

using namespace std;

namespace
{

// Runs `<ttfxPath> burn` with `content` piped into its stdin -- the same
// mechanism ttfx's own README documents (`cat file | ttfx burn`), used here
// instead of --input-file specifically so the content can already be
// centered for the current terminal before ttfx ever sees it: ttfx has no
// centering of its own, it just renders whatever text it's handed starting
// from the cursor's current position.
//
// The real terminal is inherited on stdout/stderr, and the child's natural
// exit races a keypress so the effect is always skippable -- ttfx has no
// built-in skip of its own (confirmed by inspecting its output byte-for-byte
// in an earlier session: it never reads stdin at all when given
// --input-file, and here it only reads the piped content, not further
// keystrokes). Returns true if the effect actually played, whether it
// finished on its own or was skipped; false if it could not even be
// started, so the caller knows to fall back to the native reveal instead.
bool runTtfxBurn(const string &ttfxPath, const string &content)
{
    int pipefd[2];
    if (pipe(pipefd) != 0)
    {
        return false;
    }

    const pid_t pid = fork();
    if (pid < 0)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0)
    {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        execl(ttfxPath.c_str(), ttfxPath.c_str(), "burn", static_cast<char *>(0));
        _exit(127);  // execl only returns on failure; 127 is our own sentinel for that
    }

    close(pipefd[0]);  // parent only writes
    {
        // The composed splash is a few KB at most -- comfortably under what
        // a pipe write completes without blocking even before a reader
        // starts, so no partial-write/select loop is needed here, just an
        // EINTR-retrying write().
        const char *data = content.data();
        size_t remaining = content.size();
        while (remaining > 0)
        {
            const ssize_t n = write(pipefd[1], data, remaining);
            if (n < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                break;  // pipe error (e.g. child already exited); stop writing
            }
            data += static_cast<size_t>(n);
            remaining -= static_cast<size_t>(n);
        }
    }
    close(pipefd[1]);  // EOF -- ttfx stops reading and proceeds to render

    // A Ctrl-C here has to be able to kill this specific child too -- without
    // this, SIGINT would restore the terminal and end our process while ttfx
    // kept animating into the pty for the rest of its run, unread by anyone.
    rawterm::registerChildForCleanup(pid);

    // Poll the child's exit status against a keypress. Reaching the code
    // after this loop always means a key won the race -- natural completion
    // returns directly from inside it.
    while (true)
    {
        int status = 0;
        const pid_t reaped = waitpid(pid, &status, WNOHANG);
        if (reaped == pid)
        {
            rawterm::registerChildForCleanup(0);
            const bool execFailed = WIFEXITED(status) && WEXITSTATUS(status) == 127;
            return !execFailed;
        }
        if (rawterm::keyAvailable(50))
        {
            rawterm::readKey();  // consume it so it doesn't leak into the header
            break;
        }
    }

    kill(pid, SIGTERM);
    bool reaped = false;
    for (int i = 0; i < 20 && !reaped; ++i)  // ~200ms grace window before escalating
    {
        int status = 0;
        if (waitpid(pid, &status, WNOHANG) == pid)
        {
            reaped = true;
        }
        else
        {
            usleep(10 * 1000);
        }
    }
    if (!reaped)
    {
        kill(pid, SIGKILL);
        waitpid(pid, 0, 0);
    }
    rawterm::registerChildForCleanup(0);
    return true;
}

// Same-spirit fallback for when ttfx is not available: reveals the splash
// line by line under a warm gradient. A skip keypress fast-forwards the
// remaining lines to print instantly rather than cutting the banner off
// mid-reveal, so the transition into the header never leaves something
// visibly half-drawn. Blank vertical-centering padding lines carry no visual
// interest and print instantly, never part of the timed reveal.
void nativeReveal(int termWidth, int termHeight)
{
    const layout::SplashBlock block = layout::composeSplash(termWidth, termHeight, 2);
    if (block.lines.empty())
    {
        return;
    }

    for (int i = 0; i < block.artStartIndex; ++i)
    {
        cout << "\r\n";
    }

    static const int colors[] = {220, 214, 208, 202, 196};  // gold -> orange -> red
    const size_t colorCount = sizeof(colors) / sizeof(colors[0]);

    bool skipped = false;
    for (size_t i = static_cast<size_t>(block.artStartIndex); i < block.lines.size(); ++i)
    {
        const size_t frameIndex = i - static_cast<size_t>(block.artStartIndex);
        const int color = colors[min(frameIndex, colorCount - 1)];
        cout << "\x1b[38;5;" << color << "m" << block.lines[i] << "\x1b[0m\r\n";
        cout.flush();

        if (skipped)
        {
            continue;  // dump the remaining lines instantly, no more waits
        }
        if (rawterm::keyAvailable(90))
        {
            rawterm::readKey();
            skipped = true;
        }
    }
}

}  // namespace

namespace intro
{

void playSplash()
{
    // Only meaningful while raw mode is genuinely active: skip detection
    // needs single-keystroke polling, and there is nothing to skip on a
    // piped/non-interactive run anyway.
    if (!rawterm::isActive())
    {
        const terminal::Size size = terminal::getSize();
        layout::printSplash(size.cols, size.rows);
        return;
    }

    const terminal::Size size = terminal::getSize();

    {
        const string hint = "(press any key to skip)";
        const int pad = max(0, (size.cols - static_cast<int>(hint.size())) / 2);
        cout << string(static_cast<size_t>(pad), ' ') << hint << "\r\n\r\n";
        cout.flush();
    }

    bool played = false;
    const layout::SplashBlock block = layout::composeSplash(size.cols, size.rows, 2);
    if (!block.lines.empty())
    {
        const string ttfxPath = externaltool::findExecutable("ttfx");
        if (!ttfxPath.empty())
        {
            string content;
            for (size_t i = 0; i < block.lines.size(); ++i)
            {
                content += block.lines[i];
                content += '\n';
            }
            played = runTtfxBurn(ttfxPath, content);
        }
    }

    if (!played)
    {
        nativeReveal(size.cols, size.rows);
    }

    // Running ttfx can silently reset termios out from under us -- confirmed
    // directly on this build, where ICANON and ECHO come back on after ttfx
    // runs even though nothing here ever calls suspend(). Without this, every
    // keypress at the menu that follows would need an extra keystroke to
    // terminate (canonical mode waits for a line) and would go unechoed by
    // us while echoed by the kernel instead, since termios belongs to the tty
    // device, not to any one process. Cheap and safe to call even when
    // nothing needed fixing.
    rawterm::reassert();

    // ttfx shows the cursor again on natural completion but leaves it hidden
    // if killed for a skip; the native reveal never touches cursor
    // visibility at all. Rather than track which of those happened, just
    // restore this session's steady state (hidden, as RawTerminal set it up)
    // unconditionally.
    cout << "\x1b[?25l";
    cout.flush();
}

}  // namespace intro
