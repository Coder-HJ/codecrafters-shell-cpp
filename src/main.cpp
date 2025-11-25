#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

int main() {
    // Flush after every std::cout / std:cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    vector<string> permissibleCommands;

    // Uncomment this block to pass the first stage

    std::string input;
    while (true) {
        std::cout << "$ ";
        std::getline(std::cin, input);

        if (find(permissibleCommands.begin(), permissibleCommands.end(), input) == permissibleCommands.end()) {
            cout << input << ": command not found" << endl;
        }
    }

    return 0;
}
