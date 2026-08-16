#include "Input.h"

#include <iostream>
#include <limits>
#include <sstream>
#include <string>

using namespace std;

namespace input
{

bool readInt(const string &prompt, int min, int max, int &out)
{
    while (true)
    {
        cout << prompt;
        cout.flush();

        int value = 0;
        if (cin >> value)
        {
            if (value >= min && value <= max)
            {
                out = value;
                return true;
            }
            cout << "Please choose a number between " << min << " and " << max << "." << endl;
            continue;
        }

        // Distinguish a closed stream from a typo: only the latter is worth
        // re-prompting for.
        if (cin.eof())
        {
            return false;
        }

        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "That is not a number. Try again." << endl;
    }
}

bool readLine(const string &prompt, string &out)
{
    cout << prompt;
    cout.flush();

    // `>> ws` discards the newline a preceding `cin >> n` left behind, so the
    // first getline after a numeric prompt does not come back empty. Doing it
    // here means no call site has to remember a cin.ignore() -- getting that
    // ordering wrong is what made the old loadCharacter() read a blank filename.
    string line;
    if (!getline(cin >> ws, line))
    {
        return false;
    }

    const size_t last = line.find_last_not_of(" \t\r\n");
    out = (last == string::npos) ? line : line.substr(0, last + 1);
    return true;
}

}  // namespace input
