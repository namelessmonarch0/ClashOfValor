#include "Intro.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Layout.h"
#include "Paths.h"
#include "RawTerminal.h"
#include "Terminal.h"

using namespace std;

namespace
{

// Manual PATH search plus the one fallback location that actually matters
// here: `cargo install`'s default install root, ~/.cargo/bin, which is not
// guaranteed to be on PATH even immediately after installing -- confirmed
// empirically on the machine this was built on, where `which ttfx` failed
// right after `cargo install --git ... ttfx` succeeded.
string findExecutable(const string &name)
{
    if (const char *pathEnv = getenv("PATH"))
    {
        const string path(pathEnv);
        size_t start = 0;
        while (start <= path.size())
        {
            const size_t colon = path.find(':', start);
            const string dir = (colon == string::npos) ? path.substr(start) : path.substr(start, colon - start);
            if (!dir.empty())
            {
                const string candidate = dir + "/" + name;
                if (access(candidate.c_str(), X_OK) == 0)
                {
                    return candidate;
                }
            }
            if (colon == string::npos)
            {
                break;
            }
            start = colon + 1;
        }
    }

    if (const char *home = getenv("HOME"))
    {
        const string candidate = string(home) + "/.cargo/bin/" + name;
        if (access(candidate.c_str(), X_OK) == 0)
        {
            return candidate;
        }
    }

    return "";
}

// Runs `<ttfxPath> --input-file <assetPath> burn` with the real terminal
// inherited on stdout/stderr, racing the child's natural exit against a
// keypress so the effect is always skippable -- ttfx has no built-in skip of
// its own (confirmed by inspecting its output byte-for-byte: it never reads
// stdin when given --input-file, so nothing on our end tells it to stop
// early). Returns true if the effect actually played, whether it finished on
// its own or was skipped; false if it could not even be started, so the
// caller knows to fall back to the native reveal instead.
bool runTtfxBurn(const string &ttfxPath, const string &assetPath)
{
    const pid_t pid = fork();
    if (pid < 0)
    {
        return false;
    }

    if (pid == 0)
    {
        execl(ttfxPath.c_str(), ttfxPath.c_str(), "--input-file", assetPath.c_str(), "burn", static_cast<char *>(0));
        _exit(127);  // execl only returns on failure; 127 is our own sentinel for that
    }

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
// visibly half-drawn.
//
// Plain byte-length centering is accurate here because the splash assets are
// figlet-generated ASCII, not the UTF-8 block glyphs Layout's own width
// measurement guards against more generally for future assets.
void nativeReveal(int termWidth, int termHeight)
{
    const string path = layout::pickSplashAssetPath(termWidth, termHeight);
    if (path.empty())
    {
        return;
    }

    vector<string> lines;
    int width = 0;
    {
        ifstream in(path.c_str());
        if (!in.is_open())
        {
            return;
        }
        string line;
        while (getline(in, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
            {
                line.erase(line.size() - 1);
            }
            width = max(width, static_cast<int>(line.size()));
            lines.push_back(line);
        }
    }
    if (lines.empty())
    {
        return;
    }

    const int pad = max(0, (termWidth - width) / 2);
    const string padding(pad, ' ');

    static const int colors[] = {220, 214, 208, 202, 196};  // gold -> orange -> red
    const size_t colorCount = sizeof(colors) / sizeof(colors[0]);

    bool skipped = false;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const int color = colors[min(i, colorCount - 1)];
        cout << padding << "\x1b[38;5;" << color << "m" << lines[i] << "\x1b[0m\r\n";
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
    const string assetPath = layout::pickSplashAssetPath(size.cols, size.rows);

    cout << "(press any key to skip)\r\n\r\n";
    cout.flush();

    bool played = false;
    if (!assetPath.empty())
    {
        const string ttfxPath = findExecutable("ttfx");
        if (!ttfxPath.empty())
        {
            played = runTtfxBurn(ttfxPath, assetPath);
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
