#ifndef INPUT_HPP
#define INPUT_HPP

#include <windows.h>
#include <string>
#include "Panes.hpp"
#include <algorithm>

namespace Input {

    inline void handleClipboardCopy(Pane& pane) {
        if (!OpenClipboard(NULL)) return;
        EmptyClipboard();
        
        std::string text;
        if (pane.hasSelection) {
             text = pane.currentInput;
        }
        
        if (text.empty()) {
            CloseClipboard();
            return;
        }
        
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hGlob) {
            memcpy(GlobalLock(hGlob), text.c_str(), text.size() + 1);
            GlobalUnlock(hGlob);
            SetClipboardData(CF_TEXT, hGlob);
        }
        CloseClipboard();
    }

    inline void handleClipboardPaste(Pane& pane) {
        if (!OpenClipboard(NULL)) return;
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* pszText = static_cast<char*>(GlobalLock(hData));
            if (pszText) {
                std::string text(pszText);
                GlobalUnlock(hData);
                
                for (char c : text) {
                    if (c >= 32) { 
                        pane.insertChar(c);
                    }
                }
            }
        }
        CloseClipboard();
    }

    inline void handleSelectAll(Pane& pane) {
        pane.hasSelection = true;
        pane.selectionStart = 0;
        pane.selectionEnd = pane.currentInput.length();
        pane.inputCursor = pane.currentInput.length();
    }
}

#endif // INPUT_HPP
