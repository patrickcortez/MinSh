#include "Multiplex.hpp"
#include "Utils.h" // For colors if needed
#include <windows.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

Multiplexer::Multiplexer() : activePaneIndex(0) {
    // Start with one pane
    Pane mainPane;
    try {
        mainPane.cwd = fs::current_path().string();
    } catch (...) {
        mainPane.cwd = "C:/";
    }
    mainPane.isActive = true;
    mainPane.history.push_back("Welcome to MinSh Multiplexer!");
    visiblePanes.push_back(mainPane);
}

void Multiplexer::init() {
    enableVirtualTerminal();
}

void Multiplexer::enableVirtualTerminal() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

void Multiplexer::getConsoleSize(int& rows, int& cols) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        cols = 80;
        rows = 24;
    }
}

void Multiplexer::setCursor(int x, int y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void Multiplexer::clearScreen() {
    std::cout << "\033[2J\033[H";
}

Pane& Multiplexer::getActivePane() {
    if (visiblePanes.empty()) {
        // Should not happen, but safe fallback
        static Pane empty;
        return empty;
    }
    if (activePaneIndex >= (int)visiblePanes.size()) activePaneIndex = 0;
    return visiblePanes[activePaneIndex];
}

void Multiplexer::addPane() {
    Pane newPane;
    try {
        newPane.cwd = fs::current_path().string();
    } catch (...) {
        newPane.cwd = "C:/";
    }
    newPane.isActive = false;
    newPane.history.push_back("New Session Started");
    
    visiblePanes.push_back(newPane);
    // Switch to new pane? User usually expects this.
    activePaneIndex = visiblePanes.size() - 1;
}

void Multiplexer::switchPane(int inputNumber) {
    int index = inputNumber - 1; // 1-based to 0-based
    if (index >= 0 && index < (int)visiblePanes.size()) {
        activePaneIndex = index;
        // Sync process CWD
        try {
            fs::current_path(visiblePanes[activePaneIndex].cwd);
        } catch (...) {}
    }
}

void Multiplexer::detachActivePane() {
    if (visiblePanes.size() <= 1) {
        logToActive("Minsh: cannot detach last visible pane");
        return;
    }
    
    Pane p = visiblePanes[activePaneIndex];
    backgroundPanes.push_back(p);
    
    visiblePanes.erase(visiblePanes.begin() + activePaneIndex);
    if (activePaneIndex >= (int)visiblePanes.size()) {
        activePaneIndex = visiblePanes.size() - 1;
    }
    
    // Sync CWD to new active
    try {
        fs::current_path(visiblePanes[activePaneIndex].cwd);
    } catch (...) {}
}

void Multiplexer::retachPane(int index) {
    if (index >= 0 && index < (int)backgroundPanes.size()) {
        visiblePanes.push_back(backgroundPanes[index]);
        backgroundPanes.erase(backgroundPanes.begin() + index);
    } else {
        logToActive("Minsh: invalid background session index");
    }
}

void Multiplexer::logToActive(const std::string& text) {
    // Handle newlines by splitting
    std::string temp = text;
    size_t pos = 0;
    while ((pos = temp.find('\n')) != std::string::npos) {
        getActivePane().history.push_back(temp.substr(0, pos));
        temp.erase(0, pos + 1);
    }
    if (!temp.empty()) {
        getActivePane().history.push_back(temp);
    }
}

void Multiplexer::render() {
    clearScreen();
    
    int rows, cols;
    getConsoleSize(rows, cols);
    
    if (visiblePanes.empty()) return;

    int paneWidth = cols / visiblePanes.size();
    
    // Draw each pane
    for (size_t i = 0; i < visiblePanes.size(); ++i) {
        int startCol = i * paneWidth;
        int endCol = (i == visiblePanes.size() - 1) ? cols : (startCol + paneWidth - 1); // -1 for divider if not last
        
        Pane& p = visiblePanes[i];
        
        // Draw History (limit to visible rows - 2 maybe?)
        int maxHistory = rows - 2; // Leave room for prompt
        int startHistory = 0;
        if ((int)p.history.size() > maxHistory) {
            startHistory = p.history.size() - maxHistory;
        }
        
        for (int r = 0; r < maxHistory; ++r) {
            int histIdx = startHistory + r;
            if (histIdx < (int)p.history.size()) {
                setCursor(startCol, r);
                std::string line = p.history[histIdx];
                if ((int)line.length() > (endCol - startCol)) {
                    line = line.substr(0, endCol - startCol);
                }
                std::cout << line;
            }
        }
        
        // Draw Divider if not last
        if (i < visiblePanes.size() - 1) {
            for (int r = 0; r < rows; ++r) {
                setCursor(endCol, r);
                std::cout << "|";
            }
        }
        
        // Draw Prompt area at bottom
        setCursor(startCol, rows - 1);
        std::string folderName = fs::path(p.cwd).filename().string();
        if (folderName.empty()) folderName = p.cwd;

        if ((int)i == activePaneIndex) {
             std::cout << "\033[36mMinSh[" << (i+1) << "]@\033[92m" << folderName << "\033[0m: "; 
        } else {
            std::cout << "Pane " << (i+1) << " | " << folderName; // Dimmed
        }
    }
    
    // Set cursor for input at the end of active prompt
    int activeStartCol = activePaneIndex * paneWidth;
    
    // Position cursor for input
    setCursor(activeStartCol, rows - 1);
    
    // Recalculate folderName for the active pane to ensure exact match (or just skip re-printing if we trust the cursor pos)
    // To be safe against ANSI code lengths confusing the cursor, we re-print the exact same string
    // which puts the cursor effectively at the end of it.
    std::string activeFolder = fs::path(visiblePanes[activePaneIndex].cwd).filename().string();
    if (activeFolder.empty()) activeFolder = visiblePanes[activePaneIndex].cwd;
    
    std::cout << "\033[36mMinSh[" << (activePaneIndex+1) << "]@\033[92m" << activeFolder << "\033[0m: ";
}
