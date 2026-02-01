#ifndef MULTIPLEX_HPP
#define MULTIPLEX_HPP

#include <string>
#include <vector>
#include <iostream>

struct Pane {
    std::vector<std::string> history;
    std::string cwd;
    bool isActive;
};

class Multiplexer {
public:
    Multiplexer();
    void init();
    
    // Pane Management
    void addPane(); // Split screen
    void switchPane(int inputNumber); // 1-based index from user
    void detachActivePane();
    void retachPane(int index);
    
    // State Access
    Pane& getActivePane();
    std::vector<Pane>& getVisiblePanes() { return visiblePanes; }
    std::vector<Pane>& getBackgroundPanes() { return backgroundPanes; }
    int getActivePaneIndex() const { return activePaneIndex; }

    // Rendering
    void render();
    void logToActive(const std::string& text);

private:
    std::vector<Pane> visiblePanes;
    std::vector<Pane> backgroundPanes;
    int activePaneIndex;

    // Console Helpers
    void getConsoleSize(int& rows, int& cols);
    void setCursor(int x, int y);
    void clearScreen();
    void enableVirtualTerminal();
};

#endif // MULTIPLEX_HPP
