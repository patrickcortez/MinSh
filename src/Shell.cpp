// Compile: g++ -Wall -std=c++17 src/main.cpp src/Shell.cpp src/Sessions.cpp -o bin/minsh
// Run: bin/minsh
// Execute: bin/minsh

#include "Shell.h"
#include "Utils.h"
#include "Sessions.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

Shell::Shell(const std::string& exePath) : isRunning(true) {
    SessionManager::init(exePath);
    SessionManager::ensureSessionDirectory();
    
    // Start at home directory
    const char* home = getenv("USERPROFILE"); // Windows
    if (!home) home = getenv("HOME"); // Linux/Unix fallback
    if (home) {
        try {
            fs::current_path(home);
        } catch (const fs::filesystem_error&) {
            // Fallback to current dir if home fails
        }
    }
}

void Shell::log(const std::string& text) {
    std::cout << text;
    sessionHistory << text;
}

void Shell::logLn(const std::string& text) {
    std::cout << text << std::endl;
    sessionHistory << text << "\n";
}

void Shell::run() {
    logLn("Welcome to Minsh!");
    logLn("- Type 'help' to view all commands");

    std::string input;
    while (isRunning) {
        std::string cwd;
        try {
            cwd = fs::current_path().string();
        } catch (const std::exception& e) {
            cwd = "unknown";
        }
        
        std::stringstream promptSs;
        promptSs << Color::CYAN << "MinSh@" << Color::RESET 
                 << Color::LIGHT_GREEN << cwd << Color::RESET << ": ";
        
        log(promptSs.str());
        
        if (!std::getline(std::cin, input)) {
            std::cout << std::endl;
            break;
        }

        sessionHistory << input << "\n";

        if (input.empty()) {
            continue;
        }

        parseAndExecute(input);
    }
}

std::vector<std::string> Shell::splitInput(const std::string& input) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(input);
    while (tokenStream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void Shell::parseAndExecute(const std::string& input) {
    std::vector<std::string> args = splitInput(input);
    if (args.empty()) return;

    std::string command = args[0];

    if (command == "exit") {
        cmdExit();
    } else if (command == "help") {
        cmdHelp();
    } else if (command == "say") {
        cmdSay(args);
    } else if (command == "cwd") {
        cmdCwd();
    } else if (command == "goto") {
        cmdGoto(args);
    } else if (command == "make") {
        cmdMake(args);
    } else if (command == "remove") {
        cmdRemove(args);
    } else if (command == "list") {
        cmdList(args);
    } else if (command == "sesh") {
        cmdSesh(args);
    } else {
        logLn("Minsh: " + command + ": command not found");
    }
}

void Shell::cmdExit() {
    isRunning = false;
}

void Shell::cmdHelp() {
    logLn("Commands:");
    logLn("  say <text>                 - prints text");
    logLn("  goto <path>                - goto any directory");
    logLn("  cwd                        - current directory");
    logLn("  make [-f/-d] <name>        - creates a file or directory");
    logLn("  remove [-f/-d] <name>      - removes a file or directory");
    logLn("  list [-all/-hidden] <path> - lists files and directories");
    logLn("  sesh <subcommand>          - session management:");
    logLn("    save <name>              - saves current session");
    logLn("    load <name>              - loads a session");
    logLn("    update                   - updates loaded session");
    logLn("    remove <name>            - removes a session");
    logLn("    list                     - lists all sessions");
    logLn("  exit                       - exits the shell");
}

void Shell::cmdSay(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        logLn();
        return;
    }
    std::stringstream ss;
    for (size_t i = 1; i < args.size(); ++i) {
        ss << args[i] << (i == args.size() - 1 ? "" : " ");
    }
    logLn(ss.str());
}

void Shell::cmdCwd() {
    try {
        logLn(fs::current_path().string());
    } catch (const fs::filesystem_error& e) {
        logLn(std::string("Minsh: cwd: ") + e.what());
    }
}

void Shell::cmdGoto(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        logLn("Minsh: goto: invalid arguments");
        return;
    }
    try {
        fs::current_path(args[1]);
    } catch (const fs::filesystem_error& e) {
        logLn("Minsh: " + args[1] + ": directory not found");
    }
}

void Shell::cmdMake(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        logLn("Minsh: make: invalid arguments");
        return;
    }

    std::string flag = args[1];
    std::string name = args[2];

    try {
        if (flag == "-f") {
            std::ofstream outfile(name);
            if (!outfile) {
                 logLn("Minsh: " + name + ": permission denied");
            }
            outfile.close();
        } else if (flag == "-d") {
            if (!fs::create_directory(name)) {
                if (!fs::exists(name)) {
                     logLn("Minsh: " + name + ": permission denied");
                }
            }
        } else {
             logLn("Minsh: make: invalid arguments");
        }
    } catch (const std::exception& e) {
        logLn(std::string("Minsh: make: ") + e.what());
    }
}

void Shell::cmdRemove(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        logLn("Minsh: remove: invalid arguments");
        return;
    }

    std::string flag = args[1];
    std::string name = args[2];

    try {
        if (!fs::exists(name)) {
            if (flag == "-d") logLn("Minsh: " + name + ": directory not found");
            else logLn("Minsh: " + name + ": file not found");
            return;
        }

        if (flag == "-f") {
            if (fs::is_directory(name)) {
                 logLn("Minsh: " + name + ": is a directory");
            } else {
                fs::remove(name);
            }
        } else if (flag == "-d") {
             if (!fs::is_directory(name)) {
                  logLn("Minsh: " + name + ": is not a directory");
             } else {
                 fs::remove_all(name);
             }
        } else {
             logLn("Minsh: remove: invalid arguments");
        }
    } catch (const fs::filesystem_error& e) {
        logLn("Minsh: remove: permission denied");
    }
}

void Shell::cmdList(const std::vector<std::string>& args) {
    bool showHidden = false;
    std::string pathString = ".";
    
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-all" || args[i] == "-hidden") {
            showHidden = true;
        } else {
            pathString = args[i];
        }
    }

    try {
        if (!fs::exists(pathString)) {
             logLn("Minsh: " + pathString + ": directory not found");
             return;
        }
        
        for (const auto& entry : fs::directory_iterator(pathString)) {
            std::string filename = entry.path().filename().string();
            if (!showHidden && filename[0] == '.') {
                continue;
            }
            logLn(filename);
        }
    } catch (const fs::filesystem_error& e) {
        logLn("Minsh: list: permission denied");
    }
}

void Shell::cmdSesh(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        logLn("Minsh: sesh: invalid arguments. Use save, load, or list.");
        return;
    }

    std::string subcmd = args[1];

    if (subcmd == "save") {
        if (args.size() < 3) {
            logLn("Minsh: sesh save: missing session name");
            return;
        }
        std::string name = args[2];
        std::string cwd = fs::current_path().string();
        if (SessionManager::saveSession(name, sessionHistory.str(), cwd)) {
            currentSessionName = name;
            logLn("Session '" + name + "' saved.");
        } else {
            logLn("Minsh: sesh save: failed to save session");
        }

    } else if (subcmd == "load") {
        if (args.size() < 3) {
            logLn("Minsh: sesh load: missing session name");
            return;
        }
        std::string name = args[2];
        SessionData data = SessionManager::loadSession(name);
        if (data.content.empty() && data.cwd.empty()) {
            logLn("Minsh: sesh load: session not found or empty");
        } else {
            currentSessionName = name;
            
            // Restore CWD
            if (!data.cwd.empty()) {
                try {
                    fs::current_path(data.cwd);
                } catch (const fs::filesystem_error& e) {
                    // Just log error but continue loading history
                    sessionHistory << "Minsh: warning: could not restore directory " << data.cwd << "\n";
                }
            }

            std::cout << "\033[2J\033[H";
            std::cout << data.content;
            
            sessionHistory.str("");
            sessionHistory << data.content;
        }

    } else if (subcmd == "update") {
        if (currentSessionName.empty()) {
            logLn("Minsh: sesh update: no session loaded");
            return;
        }
        std::string cwd = fs::current_path().string();
        if (SessionManager::saveSession(currentSessionName, sessionHistory.str(), cwd)) {
            logLn("Session '" + currentSessionName + "' updated.");
        } else {
            logLn("Minsh: sesh update: failed to update session");
        }

    } else if (subcmd == "remove") {
        if (args.size() < 3) {
            logLn("Minsh: sesh remove: missing session name");
            return;
        }
        std::string name = args[2];
        if (SessionManager::removeSession(name)) {
            if (currentSessionName == name) {
                currentSessionName = "";
            }
            logLn("Session '" + name + "' removed.");
        } else {
            logLn("Minsh: " + name + ": session not found");
        }

    } else if (subcmd == "list") {
        std::vector<std::string> sessions = SessionManager::listSessions();
        if (sessions.empty()) {
            logLn("No sessions found.");
        } else {
            logLn("Available sessions:");
            for (const auto& s : sessions) {
                logLn("  " + s);
            }
        }
    } else {
        logLn("Minsh: sesh: unknown subcommand '" + subcmd + "'");
    }
}
