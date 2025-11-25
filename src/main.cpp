#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>

#include <filesystem>
#include <sys/stat.h>


using namespace std;

vector<string> permissibleCommands = {"exit", "echo", "type"};
string PATH = getenv("PATH");

vector<string> splitString(const string& s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

bool isExecutableFileInDir(const string& dir, const string& fileName) {
    filesystem::path filePath = filesystem::path(dir) / fileName;
    if (!::filesystem::exists(filePath) || !filesystem::is_regular_file(filePath)) {
        return false;
    }
    struct stat sb;
    if (stat(filePath.c_str(), &sb) == 0 && (sb.st_mode & S_IXUSR)) {
        return true;
    }
    return false;
}

void executeEcho(vector<string> arguments) {
    for (auto &arg: arguments) {
        cout << arg << " ";
    }
    cout << endl;
}

void executeType(vector<string> arguments) {
    for (auto &arg: arguments) {
        if ( find(permissibleCommands.begin(), permissibleCommands.end(), arg) != permissibleCommands.end() ) {
            cout << arg << " is a shell builtin" << endl;
            continue;
        }

        // go through each directory in PATH variable
        vector<string> dirs = splitString(PATH, ':');
        bool commandFoundInPath = false;
        for (auto &dir: dirs) {
            if (isExecutableFileInDir(dir, arg)) {
                string tempDir = dir;
                if (!dir.empty() && dir.back() == '/') {
                    tempDir.pop_back();
                }
                commandFoundInPath = true;
                cout << arg << " is " << tempDir << "/" << arg << endl;
                break;
            }
        }

        if (commandFoundInPath) {
            continue;
        }

        cout << arg << ": not found" << endl;
    }
}

int main() {
    // Flush after every cout / std:cerr
    cout << unitbuf;
    cerr << unitbuf;


    string input;


    while (true) {
        cout << "$ ";
        getline(cin, input);

        vector<string> tokens = splitString(input, ' ');
        if (tokens.empty()) continue;

        if (
            find(permissibleCommands.begin(), permissibleCommands.end(), tokens[0]) == permissibleCommands.end()
        )
         {
            cout << input << ": command not found" << endl;
            continue;
        }


        if (input == "exit") {
            break;
        }

        const string& command = tokens[0];
        vector<string> arguments(tokens.begin() + 1, tokens.end());


        if (command == "echo") {
            executeEcho(arguments);
        }
        else if (command == "type") {
            executeType(arguments);
        }
        else {
            cout << input << ": command not found" << endl;
        }
    }

    return 0;
}
