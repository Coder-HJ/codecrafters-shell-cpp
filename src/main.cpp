#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>
#include <sys/stat.h>


using namespace std;

vector<string> permissibleCommands = {"exit", "echo", "type", "pwd", "cd"};
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

void executeEcho(const vector<string>& arguments) {
    for (auto &arg: arguments) {
        cout << arg << " ";
    }
    cout << endl;
}

// returns the absolute path of the program if found, else "";
string programLocationInPATH(const string& program) {
    // go through each directory in PATH variable
    vector<string> dirs = splitString(PATH, ':');
    for (auto &dir: dirs) {
        if (isExecutableFileInDir(dir, program)) {
            filesystem::path filePath = filesystem::path(dir) / program;
            return filePath.string();
        }
    }

    return "";
}

void executeType(const vector<string>& arguments) {
    for (auto &arg: arguments) {
        if ( find(permissibleCommands.begin(), permissibleCommands.end(), arg) != permissibleCommands.end() ) {
            cout << arg << " is a shell builtin" << endl;
        }
        else {
            string cmdLocation = programLocationInPATH(arg);

            if (!cmdLocation.empty()) {
                cout << arg << " is " << cmdLocation << endl;
            }
            else {
                cout << arg << ": not found" << endl;
            }
        }
    }
}


void executeProgram(const std::string& programLocation, const std::vector<std::string>& arguments) {
    pid_t pid = fork();

    if (pid == 0) {
        std::filesystem::path pathObj(programLocation);
        std::string programName = pathObj.filename().string();

        // Child process
        std::vector<char*> args;
        args.push_back(const_cast<char*>(programName.c_str()));
        for (const auto& arg : arguments) {
            args.push_back(const_cast<char*>(arg.c_str()));
        }
        args.push_back(nullptr);

        execv(programLocation.c_str(), args.data());
        perror("execv failed");
        _exit(1); // Exit child if execv fails
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            // std::cout << "Child exited with status " << WEXITSTATUS(status) << std::endl;
        } else {
            // std::cout << "Child terminated abnormally" << std::endl;
        }
    } else {
        // Fork failed
        perror("fork failed");
    }
}


void executePwd() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::cout << cwd << std::endl;
    } else {
        perror("getcwd failed");
    }
}

void executeCd(const std::vector<std::string>& arguments) {
    string goToPath = "";
    if (arguments.empty() || arguments[0].empty()) {
        goToPath = getenv("HOME");
        // no path is given... move to $HOME
    }
    else {
        goToPath = arguments[0];
        if (goToPath == "~") {
            goToPath = getenv("HOME");
        }
    }

    if (chdir(goToPath.c_str()) != 0) {
        cout << "cd: " << goToPath << ": No such file or directory" << endl;
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

        bool builtInCommandFound = true;
        const string& command = tokens[0];
        vector<string> arguments(tokens.begin() + 1, tokens.end());


        if (
            find(permissibleCommands.begin(), permissibleCommands.end(), command) == permissibleCommands.end()
        )
         {
            builtInCommandFound = false;
        }

        bool executeProgramInPath = true;

        if (builtInCommandFound) {
            executeProgramInPath = false;

            if (input == "exit") {
                break;
            }

            if (command == "echo") {
                executeEcho(arguments);
            }
            else if (command == "type") {
                executeType(arguments);
            }
            else if (command == "pwd") {
                executePwd();
            }
            else if (command == "cd") {
                executeCd(arguments);
            }
            else {
                cout << input << ": command not found" << endl;
            }
        }

        if (executeProgramInPath) {
            // searching for executable
            string programLocation = programLocationInPATH(command);
            if (!programLocation.empty()) {
                executeProgram(programLocation, arguments);
            }
            else {
                cout << input << ": command not found" << endl;
            }
        }


    }

    return 0;
}
