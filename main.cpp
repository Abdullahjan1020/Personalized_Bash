#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <cstdlib>
#include <csignal>
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

// Ek segment (words) se Command banata hai. false = syntax error.
bool parseCommand(const vector<string>& words, Command& command) {
    for (size_t i = 0; i < words.size(); ++i) {
        const string& w = words[i];

        if (w == ">" || w == ">>" || w == "<") {
            if (i + 1 >= words.size()) {
                cerr << "redirection: missing filename\n";
                return false;
            }
            const string& filename = words[++i];
            if (w == "<") {
                command.inFile = filename;
            } else {
                command.outFile = filename;
                command.append = (w == ">>");
            }
        } else {
            command.args.push_back(w);
        }
    }

    if (command.args.empty()) {
        cerr << "syntax error: empty command\n";
        return false;
    }
    return true;
}

// Child ke andar file redirection lagata hai
void applyRedirection(const Command& command) {
    if (!command.inFile.empty()) {
        int fd = open(command.inFile.c_str(), O_RDONLY);
        if (fd < 0) {
            perror("open");
            _exit(1);
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2");
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
            _exit(1);
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2");
            _exit(1);
        }
        close(fd);
    }
}

void runPipeline(vector<Command>& pipeline, bool background) {
    int n = pipeline.size();

    // n-1 pipes: pipe i = fds[2*i] (read), fds[2*i+1] (write)
    vector<int> fds(2 * (n - 1));
    for (int i = 0; i < n - 1; ++i) {
        if (pipe(&fds[2 * i]) < 0) {
            perror("pipe");
            for (int j = 0; j < 2 * i; ++j) {
                close(fds[j]);
            }
            return;
        }
    }

    vector<pid_t> pids;

    for (int i = 0; i < n; ++i) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            break;
        }

        if (pid == 0) {
            // Foreground job Ctrl+C se marna chahiye, background nahi
            if (!background) {
                signal(SIGINT, SIG_DFL);
            }

            // Background job keyboard se input na pade
            if (background && i == 0 && pipeline[i].inFile.empty()) {
                int devnull = open("/dev/null", O_RDONLY);
                if (devnull >= 0) {
                    dup2(devnull, STDIN_FILENO);
                    close(devnull);
                }
            }

            if (i > 0) {
                dup2(fds[2 * (i - 1)], STDIN_FILENO);
            }
            if (i < n - 1) {
                dup2(fds[2 * i + 1], STDOUT_FILENO);
            }

            // File redirection baad mein, taake pipe ko override kar sake
            applyRedirection(pipeline[i]);

            // Sabse important: saare pipe fds close
            for (size_t j = 0; j < fds.size(); ++j) {
                close(fds[j]);
            }

            vector<char*> args;
            for (string& s : pipeline[i].args) {
                args.push_back(s.data());
            }
            args.push_back(nullptr);

            execvp(args[0], args.data());
            perror("execvp");
            _exit(1);
        }

        pids.push_back(pid);
    }

    // Parent apne saare pipe fds close kare
    for (size_t j = 0; j < fds.size(); ++j) {
        close(fds[j]);
    }

    if (background) {
        cout << "[bg] started pid " << pids.back() << "\n";
        return;
    }

    for (size_t i = 0; i < pids.size(); ++i) {
        int status;
        if (waitpid(pids[i], &status, 0) < 0) {
            perror("waitpid");
        }
    }
}

int main() {
    // Shell khud Ctrl+C se na mare
    signal(SIGINT, SIG_IGN);

    string line;

    while (true) {
        // Khatam ho chuke background jobs ko reap karo (zombies nahi bante)
        while (waitpid(-1, nullptr, WNOHANG) > 0) {
        }

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

        // Background check: last word "&"
        bool background = false;
        if (words.back() == "&") {
            background = true;
            words.pop_back();
            if (words.empty()) {
                continue;
            }
        }

        // "|" pe split karke pipeline banao
        vector<Command> pipeline;
        vector<string> segment;
        bool syntaxError = false;

        for (size_t i = 0; i <= words.size(); ++i) {
            if (i == words.size() || words[i] == "|") {
                Command command;
                if (!parseCommand(segment, command)) {
                    syntaxError = true;
                    break;
                }
                pipeline.push_back(command);
                segment.clear();
            } else {
                segment.push_back(words[i]);
            }
        }

        if (syntaxError) {
            continue;
        }

        // Builtins sirf single command ke case mein, parent mein
        if (pipeline.size() == 1) {
            Command& c = pipeline[0];

            if (c.args[0] == "exit") {
                break;
            }

            if (c.args[0] == "cd") {
                const char* path = nullptr;

                if (c.args.size() > 1) {
                    path = c.args[1].c_str();
                } else {
                    path = getenv("HOME");
                }

                if (path == nullptr) {
                    cerr << "cd: HOME not set\n";
                } else if (chdir(path) < 0) {
                    perror("cd");
                }
                continue;
            }
        }

        runPipeline(pipeline, background);
    }

    return 0;
}