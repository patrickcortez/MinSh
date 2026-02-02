#ifndef SIGNAL_HPP
#define SIGNAL_HPP

#include <windows.h>
#include <iostream>

namespace Signal {
    inline BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
        if (dwCtrlType == CTRL_C_EVENT) {
            std::cout << "^C\n";
            return TRUE; 
        }
        return FALSE;
    }
    
    inline void init() {
        SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    }
}

#endif // SIGNAL_HPP
