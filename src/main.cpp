#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


using namespace std;

typedef struct {
    vector<string> tokens;
    string standardOutputFile;
    string standardErrorFile;
} ParsedCommand;

typedef struct {
    string token;
    int endIndex;
} ParsedToken;

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

// from s[startIndex....], fetches the first argument entity and returns it. @endIndex points to the position after the very last character of the argument.
ParsedToken fetchNextToken(const string &s, int startIndex) {
    int i = startIndex;
    while (s[i] == ' ') i++;

    string runningArgument = "";
    bool openingSingleQuoteFound = false;
    bool openingDoubleQuoteFound = false;
    bool backSlashFound = false;
    ParsedToken parsedToken;

     while (i < s.size()){

        if (s[i] == '\\') {
            if ((!openingSingleQuoteFound && !openingDoubleQuoteFound) || (openingDoubleQuoteFound)) {
                if (backSlashFound) {
                    runningArgument += string(1, s[i]);
                }
                backSlashFound = !backSlashFound;
            }
            else {
                runningArgument += string(1, s[i]);
            }
        }
        else if (s[i] == ' ') {
            if (backSlashFound) {
                if (openingDoubleQuoteFound) {
                    runningArgument += "\\" + string(1, s[i]);
                }
                else {
                    runningArgument += string(1, s[i]);
                }
                backSlashFound = false;
            }
            else if (openingSingleQuoteFound || openingDoubleQuoteFound) {
                // treat space as a normal character
                runningArgument += string(1, s[i]);
            }
            else {
                // push back runningArgument to tokens
                if (!runningArgument.empty()) {
                    parsedToken.token = runningArgument;
                    parsedToken.endIndex = i+1;
                    return parsedToken;
                }
            }
        }
        else if (s[i] == '\'') {
            if (backSlashFound) {
                if (openingDoubleQuoteFound) {
                    runningArgument += "\\" + string(1, s[i]);
                }
                else {
                    runningArgument += string(1, s[i]);
                }

                backSlashFound = false;

            }
            else if (openingDoubleQuoteFound) {
                // treat single quote as a normal character
                runningArgument += string(1, s[i]);
            }
            else if (openingSingleQuoteFound) {
                openingSingleQuoteFound = false;
            }
            else {
                openingSingleQuoteFound = true;
            }
        }
        else if (s[i] == '"') {
            if (backSlashFound) {
                runningArgument += string(1, s[i]);
                backSlashFound = false;
            }
            else if (openingSingleQuoteFound) {
                runningArgument += string(1, s[i]);
            }
            else {
                openingDoubleQuoteFound = !openingDoubleQuoteFound;
            }
        }
        else if ( s[i] == '>') {
            if (!openingSingleQuoteFound && !openingDoubleQuoteFound) {
                // not considering escape character before redirection operators as of now.
                runningArgument = ">";
                parsedToken.token = runningArgument;
                parsedToken.endIndex = i+1;
                return parsedToken;
            }
        }
        else if (i+1 < s.size() && s.substr(i, 2) == "1>") {
            if (!openingSingleQuoteFound && !openingDoubleQuoteFound) {
                // not considering escape character before redirection operators as of now.
                runningArgument = "1>";
                parsedToken.token = runningArgument;
                parsedToken.endIndex = i+2;
                return parsedToken;
            }
        }
        else if (i+1 < s.size() && s.substr(i, 2) == "2>") {
            if (!openingSingleQuoteFound && !openingDoubleQuoteFound) {
                // not considering escape character before redirection operators as of now.
                runningArgument = "2>";
                parsedToken.token = runningArgument;
                parsedToken.endIndex = i+2;
                return parsedToken;
            }
        }
        else {
            if (backSlashFound) {
                if (openingDoubleQuoteFound) {
                    runningArgument += "\\" + string(1, s[i]);
                }
                else {
                    runningArgument += string(1, s[i]);
                }

                backSlashFound = false;
            }
            else {
                runningArgument += string(1, s[i]);
            }
        }

        i++;
    }


    if (openingSingleQuoteFound || openingDoubleQuoteFound || backSlashFound) {
        cout << "Invalid Input. No closing quote or backslash found...." << endl;
        exit(1);
    }

    if (!runningArgument.empty()) {
        parsedToken.token = runningArgument;
        parsedToken.endIndex = s.size();
        return parsedToken;
    }


    return parsedToken;

}



ParsedCommand fetchTokens(const string& s) {

    ParsedCommand parsedCommand;
    ParsedToken nextToken;


    bool redirectStdoutBit = false; // if this is set to true it means that the next token is the file where stdout would be redirected
    bool redirectStderrBit = false; // if this is set to true it means that the next token is the file where stderr would be redirected

    int i = 0;
    while (i < s.size()) {
        nextToken = fetchNextToken(s, i);

        string nextTokenValue = nextToken.token;

        if (nextTokenValue == ">" || nextTokenValue == "1>") {
            redirectStdoutBit = true;
            redirectStderrBit = false;
        }
        else if (nextTokenValue == "2>") {
            redirectStdoutBit = false;
            redirectStderrBit = true;
        }
        else {
            if (redirectStdoutBit) {
                // nextTokenValue represents the stdout file
                parsedCommand.standardOutputFile = nextTokenValue;
            }
            else if (redirectStderrBit) {
                // nextTokenValue represents the stderr file
                parsedCommand.standardErrorFile = nextTokenValue;
            }
            else {
                parsedCommand.tokens.push_back(nextTokenValue);
            }

            redirectStdoutBit = false;
            redirectStderrBit = false;
        }

        i = nextToken.endIndex;
    }

    return parsedCommand;
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

        ParsedCommand parsedCommand = fetchTokens(input);
        vector<string> tokens = parsedCommand.tokens;


        if (tokens.empty()) continue;

        // for (auto &t: tokens) cout << "token - " << t << endl;
        // cout << "standardOutputFile: " << parsedCommand.standardOutputFile << endl;
        // cout << "standardErrorFile: " << parsedCommand.standardErrorFile << endl;

        int default_stdout = dup(STDOUT_FILENO);
        int default_stderr = dup(STDERR_FILENO);

        if (!parsedCommand.standardOutputFile.empty()) {
            // we need to point stdout to a file...

            int fd = open(parsedCommand.standardOutputFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                cerr << "Error while opening stdout file..." << endl;
                exit(1);
            }
            if (dup2(fd, STDOUT_FILENO) < 0) {
                cerr << "Error while dup2 on stdout file..." << endl;
                exit(1);
            }
            close(fd);
        }

        if (!parsedCommand.standardErrorFile.empty()) {
            // we need to point stdout to a file...

            int fd = open(parsedCommand.standardErrorFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                cerr << "Error while opening stderr file..." << endl;
                exit(1);
            }
            if (dup2(fd, STDERR_FILENO) < 0) {
                cerr << "Error while dup2 on stderr file..." << endl;
                exit(1);
            }
            close(fd);
        }


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


        dup2(default_stdout, STDOUT_FILENO);
        close(default_stdout);

        dup2(default_stderr, STDERR_FILENO);
        close(default_stderr);


    }

    return 0;
}
