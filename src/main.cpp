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
#include <termios.h>
#include <unordered_set>

#include "utils/Trie.cpp"


using namespace std;

typedef struct {
    string fileName;
    string mode;
} StreamRedirectionMetadata;

typedef struct {
    vector<string> tokens;
    StreamRedirectionMetadata standardOutputFile;
    StreamRedirectionMetadata standardErrorFile;
} ParsedCommand;

typedef struct {
    string token;
    int endIndex;
} ParsedToken;

vector<string> permissibleCommands = {"exit", "echo", "type", "pwd", "cd", "history"};
vector<string> commandSuggestions = {"exit", "echo"};

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
        else if ( s[i] == '|') {
            if (!openingSingleQuoteFound && !openingDoubleQuoteFound) {

                // not considering escape character before pipe operators as of now.
                runningArgument = "|"; // assuming there would be a space "|", and ">" ...etc
                parsedToken.token = runningArgument;
                parsedToken.endIndex = i+1;

                return parsedToken;
            }
        }
        else if ( s[i] == '>') {
            if (!openingSingleQuoteFound && !openingDoubleQuoteFound) {

                // not considering escape character before redirection operators as of now.
                if (i+1 < s.size() && s[i+1] == '>') {
                    // append mode
                    runningArgument = ">>";
                    parsedToken.token = runningArgument;
                    parsedToken.endIndex = i+2;
                }
                else {
                    runningArgument = ">";
                    parsedToken.token = runningArgument;
                    parsedToken.endIndex = i+1;
                }

                return parsedToken;
            }
        }
        else if (i+1 < s.size() && s.substr(i, 2) == "1>") {
            if (!openingSingleQuoteFound && !openingDoubleQuoteFound) {
                // not considering escape character before redirection operators as of now.
                if (i+2 < s.size() && s[i+2] == '>') {
                    // append mode
                    runningArgument = "1>>";
                    parsedToken.token = runningArgument;
                    parsedToken.endIndex = i+3;
                }
                else {
                    runningArgument = "1>";
                    parsedToken.token = runningArgument;
                    parsedToken.endIndex = i+2;
                }

                return parsedToken;
            }
        }
        else if (i+1 < s.size() && s.substr(i, 2) == "2>") {
            if (!openingSingleQuoteFound && !openingDoubleQuoteFound) {
                // not considering escape character before redirection operators as of now.
                if (i+2 < s.size() && s[i+2] == '>') {
                    // append mode
                    runningArgument = "2>>";
                    parsedToken.token = runningArgument;
                    parsedToken.endIndex = i+3;
                }
                else {
                    runningArgument = "2>";
                    parsedToken.token = runningArgument;
                    parsedToken.endIndex = i+2;
                }

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



vector<ParsedCommand> parseInput(const string& s) {
    vector<ParsedCommand> parsedCommands;



    ParsedCommand parsedCommand;
    ParsedToken nextToken;


    bool redirectStdoutBit = false; // if this is set to true it means that the next token is the file where stdout would be redirected
    string redirectStdoutMode = "";
    bool redirectStderrBit = false; // if this is set to true it means that the next token is the file where stderr would be redirected
    string redirectStderrmode = "";

    int i = 0;
    while (i < s.size()) {
        nextToken = fetchNextToken(s, i);

        string nextTokenValue = nextToken.token;

        if (nextTokenValue == "|") {
            parsedCommands.push_back(parsedCommand);

            // // this step is actually redundant, as the logic below already handles it
            redirectStdoutBit = false;
            redirectStdoutMode = "";
            redirectStderrBit = false;
            redirectStderrmode = "";

            parsedCommand = ParsedCommand();
        }
        else if (nextTokenValue == ">" || nextTokenValue == "1>") {
            redirectStdoutBit = true;
            redirectStderrBit = false;

            redirectStdoutMode = "w";
        }
        else if (nextTokenValue == ">>" || nextTokenValue == "1>>") {
            redirectStdoutBit = true;
            redirectStderrBit = false;

            redirectStdoutMode = "a";
        }
        else if (nextTokenValue == "2>") {
            redirectStdoutBit = false;
            redirectStderrBit = true;

            redirectStderrmode = "w";
        }
        else if (nextTokenValue == "2>>") {
            redirectStdoutBit = false;
            redirectStderrBit = true;

            redirectStderrmode = "a";
        }
        else {
            if (redirectStdoutBit) {
                // nextTokenValue represents the stdout file
                parsedCommand.standardOutputFile.fileName = nextTokenValue;
                parsedCommand.standardOutputFile.mode = redirectStdoutMode;
            }
            else if (redirectStderrBit) {
                // nextTokenValue represents the stderr file
                parsedCommand.standardErrorFile.fileName = nextTokenValue;
                parsedCommand.standardErrorFile.mode = redirectStderrmode;
            }
            else {
                parsedCommand.tokens.push_back(nextTokenValue);
            }

            redirectStdoutBit = false;
            redirectStderrBit = false;
            redirectStdoutMode = "";
            redirectStderrmode = "";
        }

        i = nextToken.endIndex;
    }

    if (!parsedCommand.tokens.empty()) {
        parsedCommands.push_back(parsedCommand);
    }

    return parsedCommands;
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
    for (int i=0; i< arguments.size(); i++) {
        cout << arguments[i];
        if (i != arguments.size()-1) {
            cout << " ";
        }
    }
    cout << endl;
}

vector<string> directoriesInPath() {
    return splitString(PATH, ':');
}

// returns the absolute path of the program if found, else "";
string programLocationInPATH(const string& program) {
    // go through each directory in PATH variable
    vector<string> dirs = directoriesInPath();
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

void executeProgramWithoutFork(const std::string& programLocation, const std::vector<std::string>& arguments, bool doFork=false) {
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
}

void executeProgram(const std::string& programLocation, const std::vector<std::string>& arguments, bool doFork=false) {
    if (doFork) {
        pid_t pid = fork();

        if (pid == 0) {
            executeProgramWithoutFork(programLocation, arguments);
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
    else {
        executeProgramWithoutFork(programLocation, arguments);
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

void printTermios(const termios& t) {
    std::cout << "c_iflag: " << t.c_iflag << std::endl;
    std::cout << "c_oflag: " << t.c_oflag << std::endl;
    std::cout << "c_cflag: " << t.c_cflag << std::endl;
    std::cout << "c_lflag: " << t.c_lflag << std::endl;
    // Print control characters
    for (int i = 0; i < NCCS; ++i) {
        std::cout << "c_cc[" << i << "]: " << (int)t.c_cc[i] << std::endl;
    }
}

// A custom function to replicate getch() behavior
int custom_getch() {
    struct termios oldt, newt;
    int ch;

    // 1. Get the current terminal settings
    // STDIN_FILENO means we are getting settings for standard input (keyboard)
    tcgetattr(STDIN_FILENO, &oldt);

    // 2. Copy the settings to a new structure
    newt = oldt;

    // 3. Modify the new settings:
    //    ICANON disables canonical mode (line buffering, no Enter key needed)
    //    ECHO disables character echoing (the key pressed won't show on screen)
    newt.c_lflag &= ~(ICANON | ECHO);

    // 4. Apply the new settings immediately (TCSANOW)
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // 5. Read a single character from standard input
    ch = getchar();

    // 6. Restore the original terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    // 7. Return the read character
    return ch;
}



vector<string> fetchAllExecutablesInPath() {
    vector<string> filesInPath;
    vector<string> dirs = directoriesInPath();

    for (auto &dir: dirs) {
        if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
            for (const auto& entry : filesystem::directory_iterator(dir)) {
                const auto& path = entry.path();
                if (access(path.c_str(), R_OK) == 0 && filesystem::is_regular_file(path)) {
                    string fileName = path.filename().string();
                    if (isExecutableFileInDir(dir, fileName)) {
                        filesInPath.push_back(fileName);
                    }
                }
            }
        }
    }

    return filesInPath;
}

string findLongestPrefix(vector<string> strs) {
    if (strs.empty()) return "";
    for (size_t i = 0; i < strs[0].size(); ++i) {
        char c = strs[0][i];
        for (size_t j = 1; j < strs.size(); ++j) {
            if (i >= strs[j].size() || strs[j][i] != c)
                return strs[0].substr(0, i);
        }
    }
    return strs[0];
}


string collectInput() {
    string input = "";
    char ch = custom_getch();
    int tabPressedCount = 0;

    while (ch != '\n') {
        if (ch == 8 || ch == 127) { // backspace
            if (!input.empty()) {
                input.pop_back();
                std::cout << "\b \b";   // \b (backspace): Moves the cursor one position left. (space): Overwrites the character at the cursor. \b (backspace): Moves the cursor back again.
            }
        }
        else if ( ch == '\t') {
            // display suggestions
            if (input == "ech") {
                input = "echo ";
                cout << "o ";
            }
            else if (input == "exi") {
                input = "exit";
                cout << "t ";
            }
            else if (input == "typ") {
                input = "type ";
                cout << "e ";
            }
            else {
                tabPressedCount++;
                // no built in command present for autocompletion

                vector<string> allFilesInPath = fetchAllExecutablesInPath();
                Trie myTrie;
                myTrie.add(allFilesInPath);
                vector<string> foundExecutables = myTrie.getAllByPrefix(input);
                if (input.empty() || foundExecutables.empty()) {
                    cout << '\a';
                    tabPressedCount = 0;
                }
                else if (foundExecutables.size() == 1) {
                    if (input != foundExecutables[0]) {
                        const string& suitableCommand = foundExecutables[0];
                        for (int i=input.size(); i<suitableCommand.size(); i++) {
                            cout << suitableCommand[i];
                        }
                        cout << " ";
                        input = suitableCommand + " ";
                        tabPressedCount = 0;
                    }
                    else {
                        cout << '\a';
                        tabPressedCount = 0;
                    }
                }
                else {
                    // multiple suggestions found
                    string longestPrefix = findLongestPrefix(foundExecutables);
                    if (longestPrefix.size() > input.size()) {
                        for (int i=input.size(); i<longestPrefix.size(); i++) {
                            cout << longestPrefix[i];
                        }
                        input = longestPrefix;
                        tabPressedCount = 0;
                    }
                    else if (tabPressedCount == 1) {
                        cout << '\a';
                    }
                    else {
                        // display all suggestions
                        cout << endl;
                        sort(foundExecutables.begin(), foundExecutables.end());
                        for (auto &s: foundExecutables) {
                            cout << s << " " << " ";
                        }
                        cout << endl;
                        cout << "$ " << input;
                    }
                }
            }
        }
        else {
            cout << ch;
            input += string(1, ch);
        }

        ch = custom_getch();
    }
    cout << endl;

    return input;
}


vector<string> commandHistory;

void executeHistory() {
    for (int i=0; i<commandHistory.size(); i++) {
        cout << "    " << (i+1) << "  " << commandHistory[i] << endl;
    }
}

// returns -1 if the REPL has to exit;
int executeCommand(string input, vector<ParsedCommand> parsedCommands, int commandIndex, bool isForkedProcess=true) {
    // child process
    const ParsedCommand &parsedCommand = parsedCommands[commandIndex];
    vector<string> tokens = parsedCommand.tokens;


    if (tokens.empty()) return 0;

    // for (auto &t: tokens) cout << "token - " << t << endl;
    // cout << "standardOutputFile: " << parsedCommand.standardOutputFile << endl;
    // cout << "standardErrorFile: " << parsedCommand.standardErrorFile << endl;

    int default_stdout = dup(STDOUT_FILENO);
    int default_stderr = dup(STDERR_FILENO);

    if (!parsedCommand.standardOutputFile.fileName.empty()) {
        // we need to point stdout to a file...

        int fd;

        if (parsedCommand.standardOutputFile.mode == "w") {
            fd = open(parsedCommand.standardOutputFile.fileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                      0644);
        } else if (parsedCommand.standardOutputFile.mode == "a") {
            fd = open(parsedCommand.standardOutputFile.fileName.c_str(), O_WRONLY | O_CREAT | O_APPEND,
                      0644);
        } else {
            cerr << "Something went wrong...Invalid file mode" << endl;
            exit(1);
        }

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

    if (!parsedCommand.standardErrorFile.fileName.empty()) {
        // we need to point stdout to a file...

        int fd;

        if (parsedCommand.standardErrorFile.mode == "w") {
            fd = open(parsedCommand.standardErrorFile.fileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        } else if (parsedCommand.standardErrorFile.mode == "a") {
            fd = open(parsedCommand.standardErrorFile.fileName.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        } else {
            cerr << "Something went wrong...Invalid file mode" << endl;
            exit(1);
        }

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
    const string &command = tokens[0];
    vector<string> arguments(tokens.begin() + 1, tokens.end());

    if (find(permissibleCommands.begin(), permissibleCommands.end(), command) == permissibleCommands.end()) {
        builtInCommandFound = false;
    }

    bool executeProgramInPath = true;

    if (builtInCommandFound) {
        executeProgramInPath = false;

        if (input == "exit") {
            return -1;
        }

        if (command == "echo") {
            executeEcho(arguments);
        } else if (command == "type") {
            executeType(arguments);
        } else if (command == "pwd") {
            executePwd();
        } else if (command == "cd") {
            executeCd(arguments);
        } else if (command == "history") {
            executeHistory();
        }
        else {
            cout << input << ": command not found" << endl;
        }
    }

    if (executeProgramInPath) {
        // searching for executable
        string programLocation = programLocationInPATH(command);
        if (!programLocation.empty()) {
            executeProgram(programLocation, arguments, !isForkedProcess);
        } else {
            cout << input << ": command not found" << endl;
        }
    }


    dup2(default_stdout, STDOUT_FILENO);
    close(default_stdout);

    dup2(default_stderr, STDERR_FILENO);
    close(default_stderr);

    return 0;
}


int main() {
    // Flush after every cout / std:cerr
    cout << unitbuf;
    cerr << unitbuf;

    string input;


    while (true) {
        cout << "$ ";

        string input = collectInput();

        commandHistory.push_back(input);


        vector<ParsedCommand> parsedCommands = parseInput(input); // parsedCommands are connected via pipe
        int totalCommands = parsedCommands.size();

        if (totalCommands == 0) {
            continue;
        }

        if(totalCommands == 1) {
            if (executeCommand(input, parsedCommands, 0, false) == -1) break;
            continue;
        }

        // piped commands. run each of them in a child process;

        int totalPipes = (int) totalCommands - 1;
        vector<vector<int>> pipes(totalPipes, vector<int>({0, 0}));
        for (auto &p: pipes) {
            pipe(p.data());
        }
        /*
         total commands = 3; total pipes = 2;
          command 0:
              stdin remains intact
              stdout goes to pipes[0][1]
          command 1:
              stdin is pipes[0][0];
              stdout goes to pipes[1][1]
          command 2(last command):
              stdin is pipes[1][0];
              stdout remains intact

        So, for command n,

            if(n==0) {
                // stdin remains intact
                // stdout goes to pipes[0][1];
            }
            else if(n < commands.size-1) {
                // stdin is pipes[n-1][0];
                // stdout is pipes[n][1];
            }
            if(n == commands.size()-1){
                // stdin is pipes[n-1][0];
                // stdout remains intact
            }
         */

        for (int subcommand = 0; subcommand < totalCommands; subcommand++) {
            string subcommandName = parsedCommands[subcommand].tokens.front();
            pid_t pid = fork();
            if (pid == 0) {

                // used to track all the used pipe file descriptors
                unordered_map<int, unordered_set<int>> usedPipeFds;

                if (subcommand == 0) {
                    // stdin remains intact;
                    dup2(pipes[0][1], STDOUT_FILENO);
                    usedPipeFds[0].insert(1);
                }
                else if (subcommand < totalCommands-1) {
                    dup2(pipes[subcommand-1][0], STDIN_FILENO);
                    dup2(pipes[subcommand][1], STDOUT_FILENO);

                    usedPipeFds[subcommand-1].insert(0);
                    usedPipeFds[subcommand].insert(1);
                }
                else {
                    dup2(pipes[subcommand-1][0], STDIN_FILENO);
                    usedPipeFds[subcommand-1].insert(0);
                    // stdout remains intact
                }

                // close all the pipes this process won't interact with at all
                for (int p=0; p<totalPipes; p++) {
                    if (!usedPipeFds[p].count(0)) {
                        close(pipes[p][0]);
                    }
                    if (!usedPipeFds[p].count(1)) {
                        close(pipes[p][1]);
                    }
                }
                
                executeCommand(input, parsedCommands, subcommand);

                if (subcommand == 0) {
                    close(pipes[0][1]);
                }
                else if (subcommand < totalCommands - 1) {
                    close(pipes[subcommand-1][0]);
                    close(pipes[subcommand][1]);
                }
                else {
                    close(pipes[subcommand-1][0]);
                }
                exit(0);
            }
            if (pid < 0) {
                perror("fork failed");
            }
        }

        // Closing the parent's pipe file descriptors ensures proper piping behavior, allowing EOF to be detected at the read end.
        for (auto &p: pipes) {
            close(p[0]);
            close(p[1]);
        }


        for (int i = 0; i <totalCommands; ++i) {
            wait(nullptr);
        }


    }

    return 0;
}
