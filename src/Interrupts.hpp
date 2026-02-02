#ifndef INTERRUPTS_HPP
#define INTERRUPTS_HPP

#include <windows.h>
#include "Panes.hpp"
#include "Input.hpp"

namespace Interrupts {

    inline void processKey(Pane& pane, KEY_EVENT_RECORD& ker) {
        if (!ker.bKeyDown) return;

        bool ctrl = (ker.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        bool shift = (ker.dwControlKeyState & (SHIFT_PRESSED)) != 0;
        char c = ker.uChar.AsciiChar;
        WORD vk = ker.wVirtualKeyCode;

        if (ctrl) {
            if (vk == 'V') {
                Input::handleClipboardPaste(pane);
            } else if (vk == 'A') {
                Input::handleSelectAll(pane);
            } else if (vk == 'L') {
                 pane.repaint();
            }
            return; 
        }

        if (vk == VK_LEFT) {
            pane.moveCursor(-1);
            if (!shift) pane.hasSelection = false;
        } else if (vk == VK_RIGHT) {
             pane.moveCursor(1);
             if (!shift) pane.hasSelection = false;
        } else if (vk == VK_HOME) {
             pane.inputCursor = 0;
             if (!shift) pane.hasSelection = false;
        } else if (vk == VK_END) {
             pane.inputCursor = pane.currentInput.length();
             if (!shift) pane.hasSelection = false;
        } else if (vk == VK_UP) {
             std::string prev = pane.session->historyUp(pane.currentInput);
             if (!prev.empty()) {
                 while(pane.currentInput.length() > 0) {
                    pane.inputCursor = pane.currentInput.length();
                    pane.deleteChar();
                 }
                 pane.currentInput = prev;
                 pane.write(prev);
                 pane.inputCursor = prev.length();
             }
        } else if (vk == VK_DOWN) {
             std::string next = pane.session->historyDown();
             while(pane.currentInput.length() > 0) {
                 pane.inputCursor = pane.currentInput.length();
                 pane.deleteChar();
             }
             pane.currentInput = next;
             pane.write(next);
             pane.inputCursor = next.length();
        } else if (vk == VK_BACK) {
            pane.deleteChar();
            pane.hasSelection = false;
        } else if (vk == VK_DELETE) {
             pane.deleteCharForward();
             pane.hasSelection = false;
        } else if (c >= 32) {
             pane.insertChar(c);
             pane.hasSelection = false;
        }
    }
}

#endif // INTERRUPTS_HPP
