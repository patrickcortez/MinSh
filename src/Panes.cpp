#include "Panes.hpp"
#include <filesystem>

namespace fs = std::filesystem;

Grid::Grid(int sx, int sy) : sx(sx), sy(sy), hsize(0) {
    for (int i = 0; i < sy; ++i) {
        lines.push_back(std::make_unique<GridLine>(sx));
    }
}

Grid::~Grid() {}

void Grid::resize(int new_sx, int new_sy) {
    if (new_sx != sx) {
        for (auto& line : lines) {
            line->cells.resize(new_sx);
        }
        sx = new_sx;
    }
    sy = new_sy;
    while (lines.size() < (size_t)sy) {
        lines.push_back(std::make_unique<GridLine>(sx));
    }
}

const GridCell& Grid::get_cell(int x, int y) const {
    if (y >= 0 && y < (int)lines.size()) {
        if (x >= 0 && x < (int)lines[y]->cells.size()) {
            return lines[y]->cells[x];
        }
    }
    static GridCell empty;
    return empty;
}

void Grid::write_cell(int x, int y, const GridCell& cell) {
    if (y >= 0 && y < (int)lines.size()) {
         if (x >= 0 && x < sx) {
             lines[y]->cells[x] = cell;
         }
    }
}

void Grid::scroll_up() {
    lines.push_back(std::make_unique<GridLine>(sx));
    hsize++;
    if (lines.size() > 2000) {
        lines.erase(lines.begin());
        hsize--;
    }
}

Pane::Pane(int w, int h) : cx(0), cy(0), scrollOffset(0), currentAttr(0x07), state(NORMAL) {
    grid = std::make_unique<Grid>(w, h);
    session = std::make_unique<ShellSession>();
    
    // Get HOME
    const char* home = std::getenv("USERPROFILE");
    if (!home) home = std::getenv("HOME");
    if (home) cwd = home;
    else cwd = ".";
    
    session->setCwd(cwd);
}

Pane::~Pane() {}

void Pane::resize(int w, int h) {
    if (w <= 0 || h <= 0) return;
    grid->resize(w, h);
    if (cx >= w) cx = w - 1;
    if (cy >= h) cy = h - 1;
}

void Pane::repaint() {
    grid = std::make_unique<Grid>(grid->sx, grid->sy);
    cx = 0;
    cy = 0;
    
    std::string folder = fs::path(cwd).filename().string();
    if (folder.empty()) folder = cwd;
    
    // Prompt reconstruction
    std::string prompt = "\n\033[36mMinSh[" + std::to_string(id) + "]\033[0m@\033[32m" + folder + "\033[0m: ";
    write(prompt);
    write(currentInput);
    
    // Restore visual cursor to inputCursor
    // After write(currentInput), cx is at end.
    // We need to move back (length - inputCursor)
    int dist = currentInput.length() - inputCursor;
    for(int i=0; i<dist; ++i) {
        if (cx > 0) cx--;
        else if (cy > 0) { cy--; cx = grid->sx - 1; }
    }
}

void Pane::write(const std::string& text) {
    for (char c : text) {
        put_char(c);
    }
}

void Pane::put_char(char c) {
    if (state != NORMAL) {
        handleAnsi(c);
        return;
    }

    if (c == '\033') {
        state = ESC;
    } else if (c == '\n') {
        new_line();
        cx = 0;
    } else if (c == '\r') {
        cx = 0;
    } else if (c == '\b') {
        backspace();
    } else if (c >= 32) {
        GridCell cell;
        cell.data = c;
        cell.attr = currentAttr; 
        
        if (cx >= grid->sx) {
            new_line();
            cx = 0;
        }
        
        int abs_y = 0;
        if (grid->lines.size() < (size_t)grid->sy) {
            abs_y = cy; 
        } else {
            int start_y = grid->lines.size() - grid->sy;
            abs_y = start_y + cy;
        }
        
        grid->write_cell(cx, abs_y, cell);
        cx++;
    }
}

void Pane::handleAnsi(char c) {
    if (state == ESC) {
        if (c == '[') {
            state = CSI;
            paramBuffer.clear();
        } else {
            state = NORMAL; 
        }
    } else if (state == CSI) {
        if (isdigit(c) || c == ';') {
            paramBuffer += c;
        } else if (c == 'm') {
            // SGR
            if (paramBuffer.empty()) {
                currentAttr = 0x07; 
            } else {
                std::vector<int> codes;
                std::string num;
                for (char p : paramBuffer) {
                    if (p == ';') {
                        if (!num.empty()) codes.push_back(std::stoi(num));
                        num.clear();
                    } else {
                        num += p;
                    }
                }
                if (!num.empty()) codes.push_back(std::stoi(num));
                
                if (codes.empty()) codes.push_back(0); // Default 0 if empty params
                
                for (int code : codes) {
                    if (code == 0) {
                        currentAttr = 0x07;
                    } else if (code == 1) {
                        currentAttr |= FOREGROUND_INTENSITY;
                    } else if (code >= 30 && code <= 37) {
                        // clear fg
                        currentAttr &= ~(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                        // map 30-37 to windows attributes
                        // 30=Black, 31=Red, 32=Green, 33=Yellow, 34=Blue, 35=Magenta, 36=Cyan, 37=White
                        if (code == 31) currentAttr |= FOREGROUND_RED;
                        else if (code == 32) currentAttr |= FOREGROUND_GREEN;
                        else if (code == 33) currentAttr |= (FOREGROUND_RED | FOREGROUND_GREEN);
                        else if (code == 34) currentAttr |= FOREGROUND_BLUE;
                        else if (code == 35) currentAttr |= (FOREGROUND_RED | FOREGROUND_BLUE);
                        else if (code == 36) currentAttr |= (FOREGROUND_GREEN | FOREGROUND_BLUE);
                        else if (code == 37) currentAttr |= (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                    } else if (code >= 90 && code <= 97) {
                         // Bright colors (same mapping + intensity)
                        currentAttr &= ~(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // clear base
                        currentAttr |= FOREGROUND_INTENSITY;
                        int base = code - 60; // Map 9x back to 3x range logic approx
                        if (base == 31) currentAttr |= FOREGROUND_RED;
                        else if (base == 32) currentAttr |= FOREGROUND_GREEN;
                        else if (base == 33) currentAttr |= (FOREGROUND_RED | FOREGROUND_GREEN);
                        else if (base == 34) currentAttr |= FOREGROUND_BLUE;
                        else if (base == 35) currentAttr |= (FOREGROUND_RED | FOREGROUND_BLUE);
                        else if (base == 36) currentAttr |= (FOREGROUND_GREEN | FOREGROUND_BLUE);
                        else if (base == 37) currentAttr |= (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                    }
                }
            }
            state = NORMAL;
        } else {
            state = NORMAL; 
        }
    }
}

void Pane::new_line() {
    cy++;
    if (cy >= grid->sy) {
        grid->scroll_up();
        cy = grid->sy - 1;
    }
}

void Pane::backspace() {
    if (cx > 0) {
        cx--;
        GridCell empty; 
        empty.attr = currentAttr;
        int abs_y = 0;
        if (grid->lines.size() < (size_t)grid->sy) {
             abs_y = cy;
        } else {
             abs_y = (grid->lines.size() - grid->sy) + cy;
        }
        grid->write_cell(cx, abs_y, empty);
    } else if (cx == 0 && cy > 0) {
        
    }
}

void Pane::scroll(int delta) {
    scrollOffset += delta;
    if (scrollOffset < 0) scrollOffset = 0;
}

void Pane::resetScroll() {
    scrollOffset = 0;
}

// ---- Manual Editing Implementations ----

void Pane::insertChar(char c) {
    if (c < 32) return;
    
    if (inputCursor < 0) inputCursor = 0;
    if (inputCursor > (int)currentInput.length()) inputCursor = currentInput.length();
    
    currentInput.insert(inputCursor, 1, c);
    inputCursor++;
    
    put_char(c);
    
    if (inputCursor < (int)currentInput.length()) {
        std::string tail = currentInput.substr(inputCursor);
        for(char tc : tail) put_char(tc);
        
        for(size_t i=0; i<tail.length(); ++i) {
            if (cx > 0) cx--;
            else if (cy > 0) { cy--; cx = grid->sx - 1; }
        }
    }
}

void Pane::deleteChar() {
    if (inputCursor > 0 && !currentInput.empty()) {
        inputCursor--;
        currentInput.erase(inputCursor, 1);
        backspace();
        
        if (inputCursor < (int)currentInput.length()) {
             std::string tail = currentInput.substr(inputCursor);
             for(char tc : tail) put_char(tc);
             put_char(' ');
             for(size_t i=0; i<tail.length() + 1; ++i) {
                if (cx > 0) cx--;
                else if (cy > 0) { cy--; cx = grid->sx - 1; }
             }
        }
    }
}

void Pane::deleteCharForward() {
    if (inputCursor < (int)currentInput.length()) {
        currentInput.erase(inputCursor, 1);
        
        std::string tail = currentInput.substr(inputCursor);
        for(char tc : tail) put_char(tc);
        put_char(' ');
        
        for(size_t i=0; i<tail.length() + 1; ++i) {
             if (cx > 0) cx--;
             else if (cy > 0) { cy--; cx = grid->sx - 1; }
        }
    }
}

void Pane::moveCursor(int delta) {
    int newPos = inputCursor + delta;
    if (newPos < 0) newPos = 0;
    if (newPos > (int)currentInput.length()) newPos = currentInput.length();
    
    int move = newPos - inputCursor;
    inputCursor = newPos;
    
    if (move > 0) {
        for(int i=0; i<move; ++i) {
             cx++;
             if (cx >= grid->sx) { 
                 cx=0; 
                 cy++; 
                 if (cy >= grid->sy) cy = grid->sy-1;
             }
        }
    } else if (move < 0) {
         for(int i=0; i<-move; ++i) {
             if (cx > 0) cx--;
             else if (cy > 0) { cy--; cx = grid->sx - 1; }
         }
    }
}
