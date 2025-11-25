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

    vector<string> permissibleCommands = {"exit", "echo"};

    // Uncomment this block to pass the first stage

    std::string input;
    while (true) {
        std::cout << "$ ";
        std::getline(std::cin, input);

        vector<string> tokens = splitString(input, ' ');

        if (tokens.empty() || (tokens[0] != "echo" && find(permissibleCommands.begin(), permissibleCommands.end(), input) == permissibleCommands.end())) {
            cout << input << ": command not found" << endl;
        }

        if (tokens.empty()) continue;

        if (input == "exit") {
            break;
        }

        const string& command = tokens[0];

        if (command == "echo") {
            for (int i=1; i<tokens.size(); i++) {
                cout << tokens[i] << " ";
            }
            cout << endl;
        }

    }

    return 0;
}
