#ifndef PANES_HPP
#define PANES_HPP

#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <windows.h>
#include "ShellSession.hpp"

struct GridCell {
    uint32_t data;
    uint16_t attr;
    uint8_t flags;
    
    GridCell() : data(' '), attr(0x07), flags(0) {}
};

struct GridLine {
    std::vector<GridCell> cells;
    int flags;
    
    GridLine(int width) : cells(width), flags(0) {}
};

class Grid {
public:
    Grid(int sx, int sy);
    ~Grid();

    int sx;
    int sy;
    int hsize;

    std::vector<std::unique_ptr<GridLine>> lines;
    
    void resize(int new_sx, int new_sy);
    void write_cell(int x, int y, const GridCell& cell);
    const GridCell& get_cell(int x, int y) const;
    void scroll_up();
};

class Pane {
public:
    Pane(int w, int h);
    ~Pane();
    
    std::unique_ptr<Grid> grid;
    std::unique_ptr<ShellSession> session;
    int cx, cy;
    int scrollOffset; 
    std::string cwd;
    std::string currentInput; // Added for line editing
    bool waitingForProcess = false; // Added for prompt management
    
    void write(const std::string& text);
    void resize(int w, int h);
    
    void put_char(char c);
    void new_line();
    
    void backspace();
    
    void scroll(int delta);
    void resetScroll();
    
private:
    uint16_t currentAttr;
    enum AnsiState {
        NORMAL,
        ESC,
        CSI,
        PARAM
    } state;
    std::string paramBuffer;
    void handleAnsi(char c);
};

#endif // PANES_HPP
