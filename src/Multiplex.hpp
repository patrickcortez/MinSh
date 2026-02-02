#ifndef MULTIPLEX_HPP
#define MULTIPLEX_HPP

#include "Panes.hpp"
#include <vector>
#include <memory>
#include <windows.h>

struct Rect {
    int x, y, w, h;
};

enum SplitType {
    SPLIT_NONE,
    SPLIT_VERTICAL,
    SPLIT_HORIZONTAL
};

struct LayoutNode {
    SplitType type;
    std::unique_ptr<LayoutNode> childA;
    std::unique_ptr<LayoutNode> childB;
    std::unique_ptr<Pane> pane;     
    float splitRatio;
    Rect cachedRect;
    LayoutNode* parent;

    LayoutNode() : type(SPLIT_NONE), splitRatio(0.5f), parent(nullptr) {}
};

class Multiplexer {
public:
    Multiplexer();
    void init();
    
    void addPane(); 
    bool switchToPane(int index); 
    bool detachActivePane();
    bool retachPane(int index);
    
    Pane& getActivePane();
    std::vector<Pane*> getBackgroundPanes();
    std::vector<Pane*> getAllPanes(); // Added
    int getActivePaneIndex() const; 

    void render();
    void logToActive(const std::string& text);
    
    void enterGuiMode();
    void exitGuiMode();
    
    int cols, rows;
    void updateSize();
    
    void handleMouse(int x, int y, int button);

private:
    std::unique_ptr<LayoutNode> root;
    LayoutNode* activeNode;
    
    std::vector<std::unique_ptr<Pane>> backgroundPanes;
    
    void calculateLayout(LayoutNode* node, Rect r);
    void renderNode(LayoutNode* node);
    void setCursor(int x, int y);
    
    HANDLE hOut;
    std::vector<CHAR_INFO> renderBuffer;
    
    void flattenPanes(LayoutNode* node, std::vector<Pane*>& out);
};

#endif // MULTIPLEX_HPP
