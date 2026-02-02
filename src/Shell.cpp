// Compile: Use CMake (mkdir build && cd build && cmake .. && cmake --build .)
// Run: bin/minsh

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
    multiplexer.init();

    if (!fs::exists("cmds")) {
        fs::create_directory("cmds");
    }

    // Start at home directory for the initial pane
    const char* home = getenv("USERPROFILE"); // Windows
    if (!home) home = getenv("HOME"); // Linux/Unix fallback
    if (home) {
        try {
            fs::current_path(home);
            multiplexer.getActivePane().cwd = home;
        } catch (const fs::filesystem_error&) {
            // Fallback
        }
    } else {
        multiplexer.getActivePane().cwd = fs::current_path().string();
    }
}

void Shell::logLn(const std::string& text) {
    multiplexer.logToActive(text + "\n");
}

void Shell::log(const std::string& text) {
    multiplexer.logToActive(text);
}

#include "Debug.hpp"

// ... imports ...

void Shell::run() {
    multiplexer.init(); 

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD prevMode;
    GetConsoleMode(hIn, &prevMode);
    if (!SetConsoleMode(hIn, ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT)) {
        std::cerr << "Error setting console mode" << std::endl;
    }
    
    std::string inputBuffer;
    
    while (isRunning) {
        try {
            try {
                fs::current_path(multiplexer.getActivePane().cwd);
            } catch (...) {}

            multiplexer.render();
            
            INPUT_RECORD ir[128];
            DWORD nRead;
            if (ReadConsoleInput(hIn, ir, 128, &nRead) && nRead > 0) {
                for (DWORD i = 0; i < nRead; ++i) {
                    if (ir[i].EventType == KEY_EVENT && ir[i].Event.KeyEvent.bKeyDown) {
                        KEY_EVENT_RECORD& ker = ir[i].Event.KeyEvent;
                        char c = ker.uChar.AsciiChar;
                        
                        if (c == '\r') { 
                            multiplexer.logToActive("\n");
                            parseAndExecute(inputBuffer);
                            inputBuffer.clear();
                            multiplexer.logToActive("\n");
                            
                            Pane& p = multiplexer.getActivePane();
                            std::string folder = fs::path(p.cwd).filename().string();
                            if (folder.empty()) folder = p.cwd;
                            std::string prompt = "\033[36mMinSh[" + std::to_string(multiplexer.getActivePaneIndex() + 1) + "]\033[0m@\033[32m" + folder + "\033[0m: ";
                            p.write(prompt);
                        } 
                        else if (c == '\b') { 
                            if (!inputBuffer.empty()) {
                                inputBuffer.pop_back();
                                multiplexer.getActivePane().backspace();
                            }
                        }
                        else if (c >= 32) { 
                            inputBuffer += c;
                            multiplexer.getActivePane().write(std::string(1, c));
                        }
                    } else if (ir[i].EventType == MOUSE_EVENT) {
                         MOUSE_EVENT_RECORD& mer = ir[i].Event.MouseEvent;
                         if (mer.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED) {
                             multiplexer.handleMouse(mer.dwMousePosition.X, mer.dwMousePosition.Y, 1);
                         }
                    }
                }
            }
        } catch (const std::exception& e) {
            debugLog("CRASH AVOIDED: " + std::string(e.what()));
            logError("Internal Crash Avoided: " + std::string(e.what()));
        } catch (...) {
            debugLog("CRASH AVOIDED: Unknown Error");
            logError("Internal Crash Avoided: Unknown Error");
        }
    }
    
    multiplexer.exitGuiMode();
    SetConsoleMode(hIn, prevMode);
}



// Helper for red errors
void Shell::logError(const std::string& text) {
    multiplexer.logToActive("\033[31m" + text + "\033[0m\n");
}

void Shell::parseAndExecute(const std::string& input) {
    try {
        auto tokens = Lexer::tokenize(input);
        if (tokens.empty()) return;

        std::vector<std::string> args;
        for (const auto& token : tokens) {
            args.push_back(token.value);
        }

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
            executeExternal(command, args);
        }
    } catch (const std::exception& e) {
        logError("Minsh: internal error: " + std::string(e.what()));
    } catch (...) {
        logError("Minsh: unknown internal error");
    }
}

void Shell::executeExternal(const std::string& cmd, const std::vector<std::string>& args) {
    std::string execCmd = cmd;
    bool foundInCmds = false;
    
    // Check cmds folder
    fs::path cmdsDir("cmds");
    std::vector<std::string> extensions = {"", ".exe", ".bat", ".cmd", ".com"};
    
    for (const auto& ext : extensions) {
        fs::path p = cmdsDir / (cmd + ext);
        if (fs::exists(p)) {
            execCmd = p.string();
            foundInCmds = true;
            break;
        }
    }

    // Build command line
    std::string commandLine = "";
    if (foundInCmds) { 
        // If found in ./cmds/ convert to absolute or relative path that system() understands
        // system() is fine with relative paths like "cmds\foo.exe"
        commandLine = execCmd;
    } else {
        commandLine = cmd; 
    }

    for (size_t i = 1; i < args.size(); ++i) {
        commandLine += " \"" + args[i] + "\"";
    }

    // Restore console mode for the child process
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD prevMode = 0;
    GetConsoleMode(hIn, &prevMode);
    SetConsoleMode(hIn, ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // CreateProcess requires a mutable string
    if (!CreateProcess(NULL,   // No module name (use command line)
        &commandLine[0],        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi)           // Pointer to PROCESS_INFORMATION structure
        ) 
    {
        if (!foundInCmds) {
            logError("Minsh: " + cmd + ": command not found or failed to execute");
        } else {
             logError("Minsh: " + execCmd + ": execution failed (" + std::to_string(GetLastError()) + ")");
        }
    } else {
        // Wait until child process exits.
        WaitForSingleObject(pi.hProcess, INFINITE);

        // Close process and thread handles. 
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    // Restore raw mode
    SetConsoleMode(hIn, prevMode);
    
    // Ideally we force repaint after external command returns
    multiplexer.render();
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
    logLn("    add                      - splits screen with new session");
    logLn("    switch <number>          - switches focus to session N");
    logLn("    detach                   - moves active session to background");
    logLn("    retach <index>           - brings background session to foreground");
    logLn("  exit                       - exits the shell");
}

void Shell::cmdSay(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        logLn("");
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
        logError(std::string("Minsh: cwd: ") + e.what());
    }
}

void Shell::cmdGoto(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        logError("Minsh: goto: invalid arguments");
        return;
    }
    try {
        fs::current_path(args[1]);
    } catch (const fs::filesystem_error& e) {
        logError("Minsh: " + args[1] + ": directory not found");
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
                 logError("Minsh: " + name + ": permission denied");
            }
            outfile.close();
        } else if (flag == "-d") {
            if (!fs::create_directory(name)) {
                if (!fs::exists(name)) {
                     logError("Minsh: " + name + ": permission denied");
                }
            }
        } else {
             logError("Minsh: make: invalid arguments");
        }
    } catch (const std::exception& e) {
        logError(std::string("Minsh: make: ") + e.what());
    }
}

void Shell::cmdRemove(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        logError("Minsh: remove: invalid arguments");
        return;
    }

    std::string flag = args[1];
    std::string name = args[2];

    try {
        if (!fs::exists(name)) {
            if (flag == "-d") logError("Minsh: " + name + ": directory not found");
            else logError("Minsh: " + name + ": file not found");
            return;
        }

        if (flag == "-f") {
            if (fs::is_directory(name)) {
                 logError("Minsh: " + name + ": is a directory");
            } else {
                fs::remove(name);
            }
        } else if (flag == "-d") {
             if (!fs::is_directory(name)) {
                  logError("Minsh: " + name + ": is not a directory");
             } else {
                 fs::remove_all(name);
             }
        } else {
             logError("Minsh: remove: invalid arguments");
        }
    } catch (const fs::filesystem_error& e) {
        logError("Minsh: remove: permission denied");
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
             logError("Minsh: " + pathString + ": directory not found");
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
        logError("Minsh: list: permission denied");
    }
}

void Shell::cmdSesh(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        logError("Minsh: sesh: invalid arguments. Use save, load, list, add, switch, detach, retach.");
        return;
    }

    std::string subcmd = args[1];

    if (subcmd == "save") {
        if (args.size() < 3) {
            logLn("Minsh: sesh save: missing session name");
            return;
        }
        std::string name = args[2];
        // Join all history lines - Hard to extract from Grid perfectly without iterating full grid
    } else if (subcmd == "save") {
        if (args.size() < 3) {
            logLn("Minsh: sesh save: missing session name");
            return;
        }
        std::string name = args[2];
        std::ostringstream oss;
        Pane& p = multiplexer.getActivePane();
        for (const auto& line : p.grid->lines) {
             std::string lineStr;
             for (const auto& c : line->cells) if (c.data != 0) lineStr += (char)c.data;
             while (!lineStr.empty() && lineStr.back() == ' ') lineStr.pop_back();
             if(!lineStr.empty()) oss << lineStr << "\n";
        }
        std::string cwd = p.cwd;
        
        if (SessionManager::saveSession(name, oss.str(), cwd)) {
            logLn("Session '" + name + "' saved.");
        } else {
            logError("Minsh: sesh save: failed to save session");
        }

    } else if (subcmd == "load") {
        if (args.size() < 3) {
            logError("Minsh: sesh load: missing session name");
            return;
        }
        std::string name = args[2];
        SessionData data = SessionManager::loadSession(name);
        if (data.content.empty() && data.cwd.empty()) {
            logError("Minsh: sesh load: session not found or empty");
        } else {
            Pane& p = multiplexer.getActivePane();
            p.cwd = data.cwd;
            p.grid = std::make_unique<Grid>(p.grid->sx, p.grid->sy); // Clear
            p.write(data.content);
            try {
                fs::current_path(p.cwd);
            } catch (...) {}
        }

    } else if (subcmd == "add") {
        multiplexer.addPane();
    } else if (subcmd == "switch") {
        if (args.size() < 3) {
            logError("Minsh: sesh switch: missing number");
            return;
        }
        try {
            int num = std::stoi(args[2]);
            multiplexer.switchPane(num);
        } catch (...) {
            logError("Minsh: sesh switch: invalid number");
        }
    } else if (subcmd == "detach") {
        multiplexer.detachActivePane();
    } else if (subcmd == "retach") {
        if (args.size() < 3) {
            logError("Minsh: sesh retach: missing index");
            return;
        }
        try {
            int num = std::stoi(args[2]);
            multiplexer.retachPane(num);
        } catch (...) {
            logError("Minsh: sesh retach: invalid number");
        }
    } else if (subcmd == "list") {
        // List on disk
        std::vector<std::string> sessions = SessionManager::listSessions();
        if (!sessions.empty()) {
            logLn("Saved Sessions:");
            for (const auto& s : sessions) {
                logLn("  " + s);
            }
        }
        // List background
        auto bg = multiplexer.getBackgroundPanes();
        if (!bg.empty()) {
            logLn("Background Panes:");
            for (size_t i = 0; i < bg.size(); ++i) {
                logLn("  [" + std::to_string(i) + "] CWD: " + bg[i]->cwd);
            }
        }
        if (sessions.empty() && bg.empty()) {
            logLn("No sessions found.");
        }
    } else {
        logError("Minsh: sesh: unknown subcommand '" + subcmd + "'");
    }
}
