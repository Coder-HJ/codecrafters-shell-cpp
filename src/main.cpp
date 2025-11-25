#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>


using namespace std;

std::vector<std::string> splitString(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

int main() {
    // Flush after every std::cout / std:cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    vector<string> permissibleCommands = {"exit", "echo", "type"};

    // Uncomment this block to pass the first stage

    std::string input;
    while (true) {
        std::cout << "$ ";
        std::getline(std::cin, input);

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
            for (int i=1; i<tokens.size(); i++) {
                cout << tokens[i] << " ";
            }
            cout << endl;
        }
        else if (command == "type") {
            for (auto &arg: arguments) {
                if ( find(permissibleCommands.begin(), permissibleCommands.end(), arg) != permissibleCommands.end() ) {
                    cout << arg << " is a shell builtin" << endl;
                }
                else {
                    cout << arg << ": not found" << endl;
                }
            }
        }
        else {
            cout << input << ": command not found" << endl;
        }
    }

    return 0;
}
