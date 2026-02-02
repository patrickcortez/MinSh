#include "Multiplex.hpp"
#include <iostream>
#include <algorithm>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

Multiplexer::Multiplexer() {
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    updateSize();
    
    root = std::make_unique<LayoutNode>();
    root->pane = std::make_unique<Pane>(cols, rows);
    activeNode = root.get();
    
    root->pane->write("Welcome to Minsh!\n- Type 'help' to view all commands\n\n");
    std::string folder = fs::path(root->pane->cwd).filename().string();
    if (folder.empty()) folder = root->pane->cwd;
    std::string prompt = "\033[36mMinSh[1]\033[0m@\033[32m" + folder + "\033[0m: ";
    root->pane->write(prompt);
    
    calculateLayout(root.get(), {0, 0, cols, rows});
}

void Multiplexer::init() {
    enterGuiMode();
}

void Multiplexer::enterGuiMode() {
    std::cout << "\033[?1049h"; 
    std::cout.flush();
}

void Multiplexer::exitGuiMode() {
    std::cout << "\033[?1049l";
    std::cout.flush();
}

void Multiplexer::updateSize() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        cols = 80;
        rows = 24;
    }
}

Pane& Multiplexer::getActivePane() {
    if (!activeNode || !activeNode->pane) {
        if (root && root->pane) return *root->pane;
        static Pane empty(80, 24);
        return empty;
    }
    return *activeNode->pane;
}

int Multiplexer::getActivePaneIndex() const {
    if (!root) return 0;
    std::vector<Pane*> panes;
    auto traverse = [&](auto&& self, LayoutNode* n) -> void {
        if (!n) return;
        if (n->type == SPLIT_NONE) {
            if (n->pane) panes.push_back(n->pane.get());
        } else {
            self(self, n->childA.get());
            self(self, n->childB.get());
        }
    };
    traverse(traverse, root.get());
    
    for (size_t i = 0; i < panes.size(); ++i) {
        if (activeNode && activeNode->pane.get() == panes[i]) return (int)i;
    }
    return 0;
}

#include "Debug.hpp"

// ...

void Multiplexer::addPane() {
    debugLog("addPane: Called");
    if (!activeNode) {
        debugLog("addPane: activeNode is null");
        return;
    }
    
    debugLog("addPane: Moving old pane");
    auto oldPane = std::move(activeNode->pane);
    
    Rect r = activeNode->cachedRect;
    debugLog("addPane: Split Logic. Rect: " + std::to_string(r.w) + "x" + std::to_string(r.h));
    
    if (r.w > (r.h * 3)) { 
        activeNode->type = SPLIT_VERTICAL;
        debugLog("addPane: SPLIT_VERTICAL");
    } else {
        activeNode->type = SPLIT_HORIZONTAL; 
        debugLog("addPane: SPLIT_HORIZONTAL");
    }
     
    activeNode->childA = std::make_unique<LayoutNode>();
    activeNode->childB = std::make_unique<LayoutNode>();
    
    activeNode->childA->parent = activeNode;
    activeNode->childB->parent = activeNode;
    
    activeNode->childA->pane = std::move(oldPane);
    
    updateSize();
    debugLog("addPane: Creating new pane");
    activeNode->childB->pane = std::make_unique<Pane>(cols, rows);
    
    activeNode = activeNode->childB.get();
    
    std::string folder = fs::path(activeNode->pane->cwd).filename().string();
    if (folder.empty()) folder = activeNode->pane->cwd;
    
    std::string prompt = "\033[36mMinSh[" + std::to_string(getActivePaneIndex() + 1) + "]\033[0m@\033[32m" + folder + "\033[0m: ";
    activeNode->pane->write(prompt);
    
    debugLog("addPane: recalculateLayout");
    calculateLayout(root.get(), {0, 0, cols, rows});
    debugLog("addPane: Done");
}

void Multiplexer::switchPane(int direction) {
    std::vector<LayoutNode*> nodes;
    auto traverse = [&](auto&& self, LayoutNode* n) -> void {
        if (!n) return;
        if (n->type == SPLIT_NONE) nodes.push_back(n);
        else {
            self(self, n->childA.get());
            self(self, n->childB.get());
        }
    };
    traverse(traverse, root.get());
    
    if (nodes.empty()) return;
    
    int idx = -1;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i] == activeNode) {
            idx = i;
            break;
        }
    }
    
    if (idx != -1) {
        idx = (idx + 1) % nodes.size();
        activeNode = nodes[idx];
        try { fs::current_path(activeNode->pane->cwd); } catch(...) {}
    }
}

void Multiplexer::logToActive(const std::string& text) {
    getActivePane().write(text);
}

void Multiplexer::calculateLayout(LayoutNode* node, Rect r) {
    if (!node) return;
    node->cachedRect = r;
    
    if (node->type == SPLIT_NONE) {
        if (node->pane) {
            node->pane->resize(r.w, r.h);
        }
    } else {
        Rect rA = r;
        Rect rB = r;
        
        if (node->type == SPLIT_VERTICAL) {
            int wA = (int)(r.w * node->splitRatio);
            rA.w = wA;
            rB.x = r.x + wA + 1; 
            rB.w = r.w - wA - 1;
        } else {
            int hA = (int)(r.h * node->splitRatio);
            rA.h = hA;
            rB.y = r.y + hA + 1;
            rB.h = r.h - hA - 1;
        }
        
        calculateLayout(node->childA.get(), rA);
        calculateLayout(node->childB.get(), rB);
    }
}

void Multiplexer::setCursor(int x, int y) {
    COORD c; c.X = x; c.Y = y;
    SetConsoleCursorPosition(hOut, c);
}

void Multiplexer::render() {
    updateSize();
    
    calculateLayout(root.get(), {0, 0, cols, rows});
    
    renderBuffer.resize(cols * rows);
    for (auto& c : renderBuffer) {
        c.Char.UnicodeChar = ' ';
        c.Attributes = 0x07;
    }
    
    renderNode(root.get());
    
    if (activeNode && activeNode->pane) {
        Rect r = activeNode->cachedRect;
        Pane* p = activeNode->pane.get();
        int cx = r.x + p->cx;
        int cy = r.y + p->cy;
        if (cx >= cols) cx = cols - 1;
        if (cy >= rows) cy = rows - 1;
        setCursor(cx, cy);
    }
    
    COORD bufSize = { (SHORT)cols, (SHORT)rows };
    COORD bufCoord = { 0, 0 };
    SMALL_RECT writeRegion = { 0, 0, (SHORT)(cols - 1), (SHORT)(rows - 1) };
    
    WriteConsoleOutputW(hOut, renderBuffer.data(), bufSize, bufCoord, &writeRegion);
}

void Multiplexer::renderNode(LayoutNode* node) {
    if (!node) return;
    
    if (node->type == SPLIT_NONE) {
        if (!node->pane) return;
        Pane* p = node->pane.get();
        Rect r = node->cachedRect;
        
        int totalLines = p->grid->lines.size();
        int gridH = p->grid->sy;
        
        // Calculate startLine based on scrollOffset
        // scrollOffset = 0 means bottom.
        // startLine = totalLines - gridH - p->scrollOffset
        int startLine = 0;
        if (totalLines > gridH) {
            startLine = totalLines - gridH - p->scrollOffset;
        } else {
             startLine = -p->scrollOffset; // Handle small content
        }
        if (startLine < 0) startLine = 0;
        
        
        for (int y = 0; y < gridH && y < r.h; ++y) {
            int absY = startLine + y;
            if (absY < totalLines) {
                 const GridLine& gl = *p->grid->lines[absY];
                 for (int x = 0; x < (int)gl.cells.size() && x < r.w; ++x) {
                     int dest = (r.y + y) * cols + (r.x + x);
                     if (dest < (int)renderBuffer.size()) {
                         renderBuffer[dest].Char.UnicodeChar = (WCHAR)gl.cells[x].data;
                         renderBuffer[dest].Attributes = gl.cells[x].attr;
                     }
                 }
            }
        }
        
        if (totalLines > r.h) {
            int sbX = r.x + r.w - 1;
            if (sbX < cols) {
                 float ratio = (float)r.h / totalLines;
                 if (ratio > 1.0f) ratio = 1.0f;
                 int thumbSize = std::max(1, (int)(r.h * ratio));
                 
                 // Thumb position:
                 // viewport top = startLine
                 // range = 0 to totalLines - r.h
                 // But visual scroll is inverted: 0 is top.
                 // startLine 0 is top. startLine max is bottom.
                 
                 // wait, startLine increases as we scroll DOWN (to bottom).
                 // if scrollOffset is 0, startLine is Max.
                 
                 // Normal visual scrollbar: Top is index 0.
                 // Thumb Y / Track H = startLine / TotalLines
                 
                 int thumbPos = (int)((float)startLine / totalLines * r.h);
                 if (thumbPos < 0) thumbPos = 0;
                 if (thumbPos + thumbSize > r.h) thumbPos = r.h - thumbSize;

                 for (int y = 0; y < r.h; ++y) {
                      int dest = (r.y + y) * cols + sbX;
                      if (dest < (int)renderBuffer.size()) {
                          if (y >= thumbPos && y < thumbPos + thumbSize) {
                              renderBuffer[dest].Char.UnicodeChar = 0x2588; 
                              renderBuffer[dest].Attributes = 0x08;
                          } else {
                              renderBuffer[dest].Char.UnicodeChar = 0x2502; // Track
                              renderBuffer[dest].Attributes = 0x08;
                          }
                      }
                 }
            }
        }
        
    } else {
        renderNode(node->childA.get());
        renderNode(node->childB.get());
        
        Rect r = node->cachedRect;
        if (node->type == SPLIT_VERTICAL) {
             int divX = (int)(r.w * node->splitRatio) + r.x;
             for (int y = r.y; y < r.y + r.h; ++y) {
                 int dest = y * cols + divX;
                 if (dest < (int)renderBuffer.size()) {
                     renderBuffer[dest].Char.UnicodeChar = 0x2502; 
                     renderBuffer[dest].Attributes = 0x08;
                 }
             }
        } else {
             int divY = (int)(r.h * node->splitRatio) + r.y;
             for (int x = r.x; x < r.x + r.w; ++x) {
                 int dest = divY * cols + x;
                 if (dest < (int)renderBuffer.size()) {
                     renderBuffer[dest].Char.UnicodeChar = 0x2500; 
                     renderBuffer[dest].Attributes = 0x08;
                 }
             }
        }
    }
}

void Multiplexer::handleMouse(int x, int y, int button) {
    // Find leaf logic
    LayoutNode* node = root.get();
    while (node && node->type != SPLIT_NONE) {
        // Simple hit test
        Rect rA = node->childA->cachedRect;
        if (x >= rA.x && x < rA.x + rA.w && y >= rA.y && y < rA.y + rA.h) {
            node = node->childA.get();
        } else {
            node = node->childB.get();
        }
    }
    
    if (node && node->pane) {
         // activeNode = node; // Click to focus DISABLED per user request
         
         Rect r = node->cachedRect;
         int sbX = r.x + r.w - 1;
         
         if (x == sbX) {
             // Scrollbar click
             // Map Y to scroll position
             Pane* p = node->pane.get();
             int totalLines = p->grid->lines.size();
             
             if (totalLines > r.h) {
                 float clickRatio = (float)(y - r.y) / r.h;
                 int targetLine = (int)(totalLines * clickRatio);
                 
                 // scrollOffset = totalLines - gridH - startLine
                 // startLine = targetLine
                 
                 int gridH = p->grid->sy;
                 // p->scrollOffset = totalLines - gridH - targetLine;
                 // But wait, if targetLine is 0 (top), we want to see top.
                 // scrollOffset = total - gridH - 0 = large positive.
                 
                 // If targetLine is total (bottom), scrollOffset = 0.
                 
                 int newOffset = totalLines - gridH - targetLine;
                  if (newOffset < 0) newOffset = 0;
                 
                  p->scrollOffset = newOffset;
             }
         } else {
             // Handle wheel? Or just focus.
         }
    }
}

void Multiplexer::flattenPanes(LayoutNode* node, std::vector<Pane*>& out) {
    if (!node) return;
    if (node->type == SPLIT_NONE) {
        if (node->pane) out.push_back(node->pane.get());
    } else {
        flattenPanes(node->childA.get(), out);
        flattenPanes(node->childB.get(), out);
    }
}

void Multiplexer::detachActivePane() {
    debugLog("detachActivePane: Called");
    if (!activeNode) { debugLog("detach: No activeNode"); return; }
    
    if (!activeNode->parent) {
        if (activeNode->pane) activeNode->pane->write("Cannot detach the last pane.\n");
        debugLog("detach: Cannot detach root");
        return;
    }

    auto parent = activeNode->parent;
    auto grandParent = parent->parent;
    
    debugLog("detach: Detaching pane");
    
    // Save pane to background
    if (activeNode->pane) {
        backgroundPanes.push_back(std::move(activeNode->pane));
    }
    
    // Find sibling to promote
    std::unique_ptr<LayoutNode> sibling;
    if (parent->childA.get() == activeNode) {
        sibling = std::move(parent->childB);
    } else {
        sibling = std::move(parent->childA);
    }
    
    if (grandParent) {
        sibling->parent = grandParent;
        if (grandParent->childA.get() == parent) {
            grandParent->childA = std::move(sibling);
        } else {
            grandParent->childB = std::move(sibling);
        }
    } else {
        // Parent was root, so Sibling becomes new root
        root = std::move(sibling);
        root->parent = nullptr;
    }
    
    // Reset activeNode to a valid leaf
    activeNode = root.get();
    while (activeNode->type != SPLIT_NONE) {
        activeNode = activeNode->childA.get();
    }
    
    updateSize(); // Refresh sizes
    calculateLayout(root.get(), {0, 0, cols, rows});
    
    if (activeNode && activeNode->pane) {
        activeNode->pane->write("Pane detached. Background count: " + std::to_string(backgroundPanes.size()) + "\n");
    }
    debugLog("detach: Done");
}

void Multiplexer::retachPane(int index) {
    debugLog("retachPane: Called with index " + std::to_string(index));
    if (index < 0 || index >= (int)backgroundPanes.size()) {
        if (backgroundPanes.empty()) {
            if (activeNode && activeNode->pane) activeNode->pane->write("No background panes.\n");
        } else {
            if (activeNode && activeNode->pane) activeNode->pane->write("Invalid background pane index.\n");
        }
        debugLog("retach: Invalid index");
        return;
    }
    
    if (!activeNode) return;
    
    auto oldPane = std::move(activeNode->pane);
    auto restoredPane = std::move(backgroundPanes[index]);
    backgroundPanes.erase(backgroundPanes.begin() + index);
    
    // Smart Split Logic (Copied from addPane)
    Rect r = activeNode->cachedRect;
    if (r.w > (r.h * 3)) { 
        activeNode->type = SPLIT_VERTICAL;
        debugLog("retach: SPLIT_VERTICAL");
    } else {
        activeNode->type = SPLIT_HORIZONTAL; 
        debugLog("retach: SPLIT_HORIZONTAL");
    }
    
    activeNode->childA = std::make_unique<LayoutNode>();
    activeNode->childB = std::make_unique<LayoutNode>();
    
    activeNode->childA->parent = activeNode;
    activeNode->childB->parent = activeNode;
    
    activeNode->childA->pane = std::move(oldPane);
    
    updateSize();
    activeNode->childB->pane = std::move(restoredPane);
    
    // Focus new pane
    activeNode = activeNode->childB.get();
    
    if (activeNode->pane) {
         activeNode->pane->write("Pane retached.\n");
    }
    
    calculateLayout(root.get(), {0, 0, cols, rows});
    debugLog("retach: Done");
}

std::vector<Pane*> Multiplexer::getBackgroundPanes() {
    std::vector<Pane*> res;
    for(auto& p : backgroundPanes) res.push_back(p.get());
    return res;
}

std::vector<Pane*> Multiplexer::getAllPanes() {
    std::vector<Pane*> panes;
    flattenPanes(root.get(), panes);
    for(auto& p : backgroundPanes) panes.push_back(p.get());
    return panes;
}
