#ifndef DEBUG_LOG_HPP
#define DEBUG_LOG_HPP
#include <fstream>
#include <string>

inline void debugLog(const std::string& msg) { //debug any errors
    std::ofstream ofs("debug.log", std::ios::app); //store log into a file
    ofs << msg << std::endl;
}
#endif
