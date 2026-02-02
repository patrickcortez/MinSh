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
#include "Signal.hpp"
#include "Interrupts.hpp"

namespace fs = std::filesystem;

Shell::Shell(const std::string& exePath) : isRunning(true) {
    SessionManager::init(exePath);
    SessionManager::ensureSessionDirectory();
    multiplexer.init();
    
    // Init main pane history
    multiplexer.getActivePane().session->initHistory(exePath);

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
            multiplexer.getActivePane().session->setCwd(home);
        } catch (const fs::filesystem_error&) {
            // Fallback
        }
    } else {
        std::string current = fs::current_path().string();
        multiplexer.getActivePane().cwd = current;
        multiplexer.getActivePane().session->setCwd(current);
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
    Signal::init();

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD prevMode;
    GetConsoleMode(hIn, &prevMode);
    if (!SetConsoleMode(hIn, ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT)) {
        std::cerr << "Error setting console mode" << std::endl;
    }
    
    while (isRunning) {
        try {
            // 1. Poll Sessions
            auto panes = multiplexer.getAllPanes();
            for (auto* pane : panes) {
                if (pane->session) {
                    bool busy = pane->session->isBusy();
                    std::string out = pane->session->pollOutput();
                    if (!out.empty()) pane->write(out);
                    
                    if (pane->waitingForProcess && !busy) {
                         pane->waitingForProcess = false;
                         
                         // Print Prompt
                         std::string folder = fs::path(pane->session->getCwd()).filename().string();
                         if (folder.empty()) folder = pane->session->getCwd();
                         
                         
                         // Print Prompt
                         std::string prompt = "\n\033[36mMinSh[" + std::to_string(pane->id) + "]\033[0m@\033[32m" + folder + "\033[0m: ";
                         pane->write(prompt);
                         
                         // Reset Input State on new prompt
                         pane->currentInput.clear();
                         pane->inputCursor = 0;
                    }
                }
            }

            // 2. Sync CWD
            try {
                fs::current_path(multiplexer.getActivePane().session->getCwd());
            } catch (...) {}

            // 3. Render
            multiplexer.render();
            
            // 4. Input Handling
            DWORD nAvailable = 0;
            GetNumberOfConsoleInputEvents(hIn, &nAvailable);
            
            if (nAvailable > 0) {
                INPUT_RECORD ir[128];
                DWORD nRead;
                if (ReadConsoleInput(hIn, ir, 128, &nRead) && nRead > 0) {
                    for (DWORD i = 0; i < nRead; ++i) {
                        if (ir[i].EventType == KEY_EVENT) {
                            Pane& p = multiplexer.getActivePane();
                            bool bKeyDown = ir[i].Event.KeyEvent.bKeyDown;
                            WORD vk = ir[i].Event.KeyEvent.wVirtualKeyCode;
                            char c = ir[i].Event.KeyEvent.uChar.AsciiChar;
                            DWORD dwCtrl = ir[i].Event.KeyEvent.dwControlKeyState;
                            
                            bool ctrl = (dwCtrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
                            bool shift = (dwCtrl & SHIFT_PRESSED) != 0;
                            
                            // Define Prompt Helper
                            auto printPrompt = [&](Pane& p) {
                                std::string folder = fs::path(p.session->getCwd()).filename().string();
                                if (folder.empty()) folder = p.session->getCwd();
                                std::string prompt = "\n\033[36mMinSh[" + std::to_string(p.id) + "]\033[0m@\033[32m" + folder + "\033[0m: ";
                                p.write(prompt);
                                p.currentInput.clear();
                                p.inputCursor = 0;
                            };

                            if (bKeyDown && ctrl && shift && vk == 'C') {
                                // Global Copy
                                Input::handleClipboardCopy(p);
                                continue; 
                            }

                            if (p.session && p.session->isBusy()) {
                                // Busy State
                                if (bKeyDown && ctrl && !shift && vk == 'C') {
                                    // SIGINT (CTRL+C)
                                    GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
                                    // p.write("^C"); // Optional visual
                                } else if (bKeyDown) {
                                    // Forward chars
                                    if (c != 0) {
                                        std::string s(1, c);
                                        p.session->writeInput(s);
                                        p.write(s); 
                                    }
                                }
                            } else {
                                // Shell Idle State
                                if (bKeyDown && ctrl && !shift && vk == 'C') {
                                     // Cancel Input
                                     p.write("^C");
                                     printPrompt(p);
                                } else {
                                    // Line Editing
                                    Interrupts::processKey(p, ir[i].Event.KeyEvent);
                                    
                                    // Check Enter
                                    if (bKeyDown && c == '\r') {
                                        p.write("\n");
                                        std::string cmd = p.currentInput;
                                        p.currentInput.clear();
                                        p.inputCursor = 0;
                                        
                                        if (!cmd.empty()) {
                                            p.session->addHistory(cmd);
                                            p.session->resetHistoryIndex();
                                            parseAndExecute(cmd);
                                        } else {
                                            printPrompt(p);
                                        }
                                        
                                        if (!p.waitingForProcess && !cmd.empty()) {
                                             printPrompt(p);
                                        }
                                    }
                                }
                            }
                        } else if (ir[i].EventType == MOUSE_EVENT) {
                             MOUSE_EVENT_RECORD& mer = ir[i].Event.MouseEvent;
                             if (mer.dwEventFlags & MOUSE_WHEELED) {
                                  // High word is delta
                                  short delta = (short)(mer.dwButtonState >> 16);
                                  multiplexer.handleMouseWheel(mer.dwMousePosition.X, mer.dwMousePosition.Y, delta);
                             } else if (mer.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED) {
                                 multiplexer.handleMouse(mer.dwMousePosition.X, mer.dwMousePosition.Y, 1);
                             }
                        }
                    }
                }
            } else {
                Sleep(10); // Prevent CPU burn
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
        } else if (command == "read") {
            cmdRead(args);
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
    Pane& p = multiplexer.getActivePane();
    
    std::string execCmd = cmd;
    bool foundInCmds = false;
    
    // Check cmds folder
    fs::path cmdsDir("cmds");
    std::vector<std::string> extensions = {"", ".exe", ".bat", ".cmd", ".com"};
    
    for (const auto& ext : extensions) {
        fs::path pPath = cmdsDir / (cmd + ext);
        if (fs::exists(pPath)) {
            execCmd = pPath.string();
            foundInCmds = true;
            break;
        }
    }

    // Build command line
    std::string commandLine = "";
    if (foundInCmds) { 
        commandLine = execCmd;
    } else {
        commandLine = cmd; 
    }

    for (size_t i = 1; i < args.size(); ++i) {
        commandLine += " \"" + args[i] + "\"";
    }

    if (p.session) {
        if (p.session->execute(commandLine)) {
            p.waitingForProcess = true;
        } else {
             logError("Minsh: " + cmd + ": command not found or failed to execute (" + std::to_string(GetLastError()) + ")");
        }
    } else {
        logError("Minsh: internal error: no session");
    }
    
    // Force poll immediately? No, loop handles it.
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
    logLn("  read <file> [flags]        - reads file content");
    logLn("    -h(\"word\")              - highlights word in red");
    logLn("    -f(n)                    - reads first n lines");
    logLn("    -l(n)                    - reads last n lines");
    logLn("  sesh <subcommand>          - session management:");
    logLn("    save <name>              - saves current session");
    logLn("    load <name>              - loads a session");
    logLn("    update <name>            - updates saved session");
    logLn("    remove <name>            - removes a session");
    logLn("    list [-b]                - lists sessions (-b for background only)");
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
        logLn(multiplexer.getActivePane().session->getCwd());
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
        Pane& p = multiplexer.getActivePane();
        fs::path target(args[1]);
        if (target.is_relative()) {
            target = fs::path(p.session->getCwd()) / target;
        }
        
        target = fs::canonical(target); // Resolve .. etc
        
        if (fs::exists(target) && fs::is_directory(target)) {
             p.session->setCwd(target.string());
             p.cwd = target.string();
             // fs::current_path(target); // Loop will sync this
        } else {
             logError("Minsh: " + args[1] + ": directory not found");
        }
    } catch (const fs::filesystem_error& e) {
        logError("Minsh: " + args[1] + ": directory not found"); // canonical throws if not found
    } catch (...) {
        logError("Minsh: " + args[1] + ": error changing directory");
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
            // User inputs 1-based index (e.g. MinSh[1]), convert to 0-based for internal logic
            if (!multiplexer.switchToPane(num - 1)) {
                logError("Minsh: sesh switch: pane " + std::to_string(num) + " does not exist");
            }
        } catch (...) {
            logError("Minsh: sesh switch: invalid number");
        }
    } else if (subcmd == "detach") {
        if (!multiplexer.detachActivePane()) {
            logError("Minsh: sesh detach: cannot detach the last pane");
        }
    } else if (subcmd == "retach") {
        if (args.size() < 3) {
            logError("Minsh: sesh retach: missing index");
            return;
        }
        try {
            int num = std::stoi(args[2]);
            if (!multiplexer.retachPane(num)) {
                 logError("Minsh: sesh retach: invalid index " + std::to_string(num));
            }
        } catch (...) {
            logError("Minsh: sesh retach: invalid number");
        }
    } else if (subcmd == "remove") {
         if (args.size() < 3) {
            logError("Minsh: sesh remove: missing session name");
            return;
        }
        if (SessionManager::removeSession(args[2])) {
             logLn("Session '" + args[2] + "' removed.");
        } else {
             logError("Minsh: sesh remove: session not found");
        }
    } else if (subcmd == "update") {
        // Update loaded session? No, typically "update <name>" overwrites it.
        // Assuming current pane has a session loaded. But session info isn't stored in pane struct strongly.
        // Let's implement as "update <name>".
         if (args.size() < 3) {
            logError("Minsh: sesh update: missing session name");
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
        // Save overwrites.
        if (SessionManager::saveSession(name, oss.str(), p.cwd)) {
            logLn("Session '" + name + "' updated.");
        } else {
            logError("Minsh: sesh update: failed to update session");
        }
    } else if (subcmd == "list") {
        bool onlyBackground = false;
        if (args.size() > 2 && args[2] == "-b") {
            onlyBackground = true;
        }

        // List on disk (only if not -b)
        if (!onlyBackground) {
            std::vector<std::string> sessions = SessionManager::listSessions();
            if (!sessions.empty()) {
                logLn("Saved Sessions:");
                for (const auto& s : sessions) {
                    logLn("  " + s);
                }
            }
        }
        
        // List background
        auto bg = multiplexer.getBackgroundPanes();
        if (!bg.empty()) {
            if (onlyBackground) logLn("Background Panes (Detached):");
            else logLn("Background Panes:");
            
            auto now = std::chrono::steady_clock::now();
            
            for (size_t i = 0; i < bg.size(); ++i) {
                std::string durStr = "";
                if (bg[i]->detachTime.time_since_epoch().count() > 0) {
                     auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - bg[i]->detachTime).count();
                     durStr = " (Detached: " + std::to_string(dur) + "s ago)";
                }
                logLn("  [" + std::to_string(i) + "] CWD: " + bg[i]->cwd + durStr);
            }
        }
        
        if (!onlyBackground && SessionManager::listSessions().empty() && bg.empty()) {
            logLn("No sessions found.");
        } else if (onlyBackground && bg.empty()) {
            logLn("No background sessions found.");
        }
    } else {
        logError("Minsh: sesh: unknown subcommand '" + subcmd + "'");
    }
}

void Shell::cmdRead(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        logError("Minsh: read: missing file operand");
        return;
    }

    std::string filename;
    std::string highlightWord;
    int headCount = -1;
    int tailCount = -1;

    // Parse Args
    for (size_t i = 1; i < args.size(); ++i) {
        std::string arg = args[i];
        if (arg.rfind("-h(", 0) == 0 && arg.back() == ')') {
            // Highlight: -h("word") or -h(word)
            std::string val = arg.substr(3, arg.length() - 4);
            // Strip quotes if present
            if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\''))) {
                val = val.substr(1, val.length() - 2);
            }
            highlightWord = val;
        } else if (arg.rfind("-l(", 0) == 0 && arg.back() == ')') {
            // Tail: -l(10)
            try {
                tailCount = std::stoi(arg.substr(3, arg.length() - 4));
            } catch (...) { logError("Minsh: read: invalid tail count"); return; }
        } else if (arg.rfind("-f(", 0) == 0 && arg.back() == ')') {
             // Head: -f(10)
            try {
                headCount = std::stoi(arg.substr(3, arg.length() - 4));
            } catch (...) { logError("Minsh: read: invalid head count"); return; }
        } else {
            filename = arg;
        }
    }
    
    if (filename.empty()) {
        logError("Minsh: read: missing filename");
        return; 
    }
    
    if (!fs::exists(filename)) {
        logError("Minsh: read: " + filename + ": no such file or directory");
        return;
    }
    
    std::ifstream file(filename);
    if (!file) {
        logError("Minsh: read: permission denied");
        return;
    }
    
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    
    // Apply Head (-f)
    if (headCount != -1) {
        if (headCount < (int)lines.size()) {
            lines.resize(headCount);
        }
    }
    
    // Apply Tail (-l)
    if (tailCount != -1) {
        if (tailCount < (int)lines.size()) {
             std::vector<std::string> newLines;
             for (size_t i = lines.size() - tailCount; i < lines.size(); ++i) {
                 newLines.push_back(lines[i]);
             }
             lines = newLines;
        }
    }
    
    // Output with Highlight
    for (const auto& l : lines) {
        if (!highlightWord.empty()) {
            // Find and replace all occurrences
            std::string printLine = "";
            size_t pos = 0;
            size_t found;
            while ((found = l.find(highlightWord, pos)) != std::string::npos) {
                printLine += l.substr(pos, found - pos);
                printLine += "\033[31m" + highlightWord + "\033[0m";
                pos = found + highlightWord.length();
            }
            printLine += l.substr(pos);
            logLn(printLine);
        } else {
            logLn(l);
        }
    }
}
