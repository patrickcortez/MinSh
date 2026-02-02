#ifndef DEBUG_LOG_HPP
#define DEBUG_LOG_HPP
#include <fstream>
#include <string>

inline void debugLog(const std::string& msg) {
    std::ofstream ofs("debug.log", std::ios::app);
    ofs << msg << std::endl;
}
#endif
