#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <array>
#include <cstdlib>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

struct Command {
    vector<string> args;
    string inFile;
    string outFile;
    bool append = false;
};

bool parseCommand(const vector<string>& words, size_t start, size_t end,
                  Command& command) {
    for (size_t i = start; i < end; ++i) {
        const string& currentWord = words[i];

        if (currentWord == ">" || currentWord == ">>" ||
            currentWord == "<") {
            if (i + 1 >= end) {
                cerr << "redirection: missing filename\n";
                return false;
            }

            const string& filename = words[++i];
            if (currentWord == "<") {
                command.inFile = filename;
            } else {
                command.outFile = filename;
                command.append = currentWord == ">>";
            }
        } else {
            command.args.push_back(currentWord);
        }
    }

    if (command.args.empty()) {
        cerr << "syntax error near |\n";
        return false;
    }

    return true;
}

void closePipes(const vector<array<int, 2>>& pipes) {
    for (const auto& currentPipe : pipes) {
        close(currentPipe[0]);
        close(currentPipe[1]);
    }
}

void runPipeline(const vector<Command>& pipeline) {
    vector<array<int, 2>> pipes;
    for (size_t i = 0; i + 1 < pipeline.size(); ++i) {
        array<int, 2> currentPipe;
        if (pipe(currentPipe.data()) < 0) {
            perror("pipe");
            closePipes(pipes);
            return;
        }
        pipes.push_back(currentPipe);
    }

    vector<pid_t> pids;
    for (size_t i = 0; i < pipeline.size(); ++i) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            closePipes(pipes);
            for (pid_t childPid : pids) {
                waitpid(childPid, nullptr, 0);
            }
            return;
        }

        if (pid == 0) {
            if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                perror("dup2");
                closePipes(pipes);
                _exit(1);
            }

            if (i + 1 < pipeline.size() &&
                dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                perror("dup2");
                closePipes(pipes);
                _exit(1);
            }

            const Command& command = pipeline[i];
            if (!command.inFile.empty()) {
                int fd = open(command.inFile.c_str(), O_RDONLY);
                if (fd < 0) {
                    perror("open");
                    closePipes(pipes);
                    _exit(1);
                }

                if (dup2(fd, STDIN_FILENO) < 0) {
                    perror("dup2");
                    close(fd);
                    closePipes(pipes);
                    _exit(1);
                }
                close(fd);
            }

            if (!command.outFile.empty()) {
                int flags = O_WRONLY | O_CREAT;
                flags |= command.append ? O_APPEND : O_TRUNC;

                int fd = open(command.outFile.c_str(), flags, 0644);
                if (fd < 0) {
                    perror("open");
                    closePipes(pipes);
                    _exit(1);
                }

                if (dup2(fd, STDOUT_FILENO) < 0) {
                    perror("dup2");
                    close(fd);
                    closePipes(pipes);
                    _exit(1);
                }
                close(fd);
            }

            closePipes(pipes);

            if (command.args[0] == "exit") {
                _exit(0);
            }

            if (command.args[0] == "cd") {
                const char* path = command.args.size() > 1
                                       ? command.args[1].c_str()
                                       : getenv("HOME");
                if (path == nullptr) {
                    cerr << "cd: HOME not set\n";
                    _exit(1);
                }
                if (chdir(path) < 0) {
                    perror("cd");
                    _exit(1);
                }
                _exit(0);
            }

            vector<string> argumentStorage = command.args;
            vector<char*> args;
            for (string& currentWord : argumentStorage) {
                args.push_back(currentWord.data());
            }
            args.push_back(nullptr);

            execvp(args[0], args.data());
            perror("execvp");
            _exit(1);
        }

        pids.push_back(pid);
    }

    closePipes(pipes);
    for (pid_t pid : pids) {
        if (waitpid(pid, nullptr, 0) < 0) {
            perror("waitpid");
        }
    }
}

int main() {
    string line;

    while (true) {
        cout << "myshell> " << flush;

        if (!getline(cin, line)) {
            break;
        }

        istringstream input(line);
        vector<string> words;
        string word;

        while (input >> word) {
            words.push_back(word);
        }

        if (words.empty()) {
            continue;
        }

        vector<Command> pipeline;
        size_t segmentStart = 0;
        bool parseError = false;

        for (size_t i = 0; i <= words.size(); ++i) {
            if (i != words.size() && words[i] != "|") {
                continue;
            }

            Command command;
            if (!parseCommand(words, segmentStart, i, command)) {
                parseError = true;
                break;
            }
            pipeline.push_back(command);
            segmentStart = i + 1;
        }

        if (parseError) {
            continue;
        }

        if (pipeline.size() == 1 && pipeline[0].args[0] == "exit") {
            break;
        }

        if (pipeline.size() == 1 && pipeline[0].args[0] == "cd") {
            const Command& command = pipeline[0];
            const char* path = command.args.size() > 1
                                   ? command.args[1].c_str()
                                   : getenv("HOME");

            if (path == nullptr) {
                cerr << "cd: HOME not set\n";
            } else if (chdir(path) < 0) {
                perror("cd");
            }
            continue;
        }

        runPipeline(pipeline);
    }

    return 0;
}