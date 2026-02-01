// Compile: g++ -Wall -std=c++17 src/main.cpp src/Shell.cpp src/Sessions.cpp -o bin/minsh
// Run: bin/minsh
// Execute: bin/minsh

#ifndef SESSIONS_HPP
#define SESSIONS_HPP

#include <string>
#include <vector>
#include <filesystem>

struct SessionData {
    std::string cwd;
    std::string content;
};

class SessionManager {
public:
    static void ensureSessionDirectory();
    static bool saveSession(const std::string& name, const std::string& content, const std::string& cwd);
    static SessionData loadSession(const std::string& name);
    static bool removeSession(const std::string& name);
    static std::vector<std::string> listSessions();
    
    static void init(const std::string& exePath);
    
private:
    static std::filesystem::path sessionRoot;
    static std::filesystem::path getSessionDir();
};

#endif // SESSIONS_HPP
