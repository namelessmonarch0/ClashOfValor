#include "Paths.h"

#include <string>

using namespace std;

namespace
{
// Directory the executable lives in, with no trailing slash. "." until init()
// runs, which keeps behaviour sane if a path is requested early.
string baseDir = ".";
}  // namespace

namespace paths
{

void init(const char *argv0)
{
    if (argv0 == 0)
    {
        return;
    }

    const string invocation(argv0);
    const size_t slash = invocation.find_last_of('/');

    // No slash means the binary was found on PATH, in which case argv[0] tells
    // us nothing about where it lives; fall back to the working directory.
    baseDir = (slash == string::npos) ? "." : invocation.substr(0, slash);
    if (baseDir.empty())
    {
        baseDir = "/";
    }
}

string asset(const string &name) { return baseDir + "/assets/" + name; }

string data(const string &name) { return baseDir + "/" + name; }

}  // namespace paths
