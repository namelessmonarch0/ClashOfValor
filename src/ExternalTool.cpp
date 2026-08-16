#include "ExternalTool.h"

#include <unistd.h>

#include <cstdlib>

using namespace std;

namespace externaltool
{

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

}  // namespace externaltool
