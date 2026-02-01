// Compile: g++ -Wall -std=c++17 src/main.cpp src/Shell.cpp src/Sessions.cpp -o bin/minsh
// Run: bin/minsh
// Execute: bin/minsh

#include "Sessions.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>

namespace fs = std::filesystem;

std::filesystem::path SessionManager::sessionRoot;

void SessionManager::init(const std::string& exePath) {
    fs::path exeDir = fs::absolute(exePath).parent_path();
    
    fs::path potentialPath = exeDir / "sessions";
    if (fs::exists(potentialPath)) {
        sessionRoot = potentialPath;
        return;
    }
    
    if (exeDir.filename() == "bin") {
        potentialPath = exeDir.parent_path() / "sessions";
        if (fs::exists(potentialPath)) {
            sessionRoot = potentialPath;
            return;
        }
    }
    
    if (exeDir.filename() == "bin") {
        sessionRoot = exeDir.parent_path() / "sessions";
    } else {
        sessionRoot = exeDir / "sessions";
    }
}

std::filesystem::path SessionManager::getSessionDir() {
    if (sessionRoot.empty()) {
        return "sessions";
    }
    return sessionRoot;
}

void SessionManager::ensureSessionDirectory() {
    fs::path dir = getSessionDir();
    if (!fs::exists(dir)) {
        fs::create_directory(dir);
    }
}

bool SessionManager::saveSession(const std::string& name, const std::string& content, const std::string& cwd) {
    ensureSessionDirectory();
    fs::path filename = getSessionDir() / (name + ".sesh");
    std::ofstream outfile(filename);
    if (!outfile) return false;
    outfile << cwd << "\n";
    outfile << content;
    outfile.close();
    return true;
}

SessionData SessionManager::loadSession(const std::string& name) {
    ensureSessionDirectory();
    fs::path filename = getSessionDir() / (name + ".sesh");
    std::ifstream infile(filename);
    SessionData data;
    if (!infile) return data;
    
    if (std::getline(infile, data.cwd)) {
        // Remove \r if present (Windows)
        if (!data.cwd.empty() && data.cwd.back() == '\r') {
            data.cwd.pop_back();
        }
    } else {
        data.cwd = "";
    }
    
    std::string content((std::istreambuf_iterator<char>(infile)), 
                        std::istreambuf_iterator<char>());
    data.content = content;
    return data;
}

bool SessionManager::removeSession(const std::string& name) {
    ensureSessionDirectory();
    fs::path filename = getSessionDir() / (name + ".sesh");
    if (fs::exists(filename)) {
        return fs::remove(filename);
    }
    return false;
}

std::vector<std::string> SessionManager::listSessions() {
    ensureSessionDirectory();
    std::vector<std::string> sessions;
    for (const auto& entry : fs::directory_iterator(getSessionDir())) {
        if (entry.path().extension() == ".sesh") {
             sessions.push_back(entry.path().filename().string());
        }
    }
    return sessions;
}
