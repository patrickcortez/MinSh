// Compile: g++ -Wall -std=c++17 src/main.cpp src/Shell.cpp src/Sessions.cpp -o bin/minsh
// Run: bin/minsh
// Execute: bin/minsh

#ifndef SHELL_H
#define SHELL_H

#include <string>
#include <vector>
#include <sstream>
#include "Multiplex.hpp"
#include "Lexer.hpp"

class Shell {
public:
    Shell(const std::string& exePath);
    void run();

private:
    bool isRunning;

    void printPrompt();
    void parseAndExecute(const std::string& input);
    // std::vector<std::string> splitInput(const std::string& input); // Replaced by Lexer

    // Commands
    void executeExternal(const std::string& cmd, const std::vector<std::string>& args);

    // Commands
    void cmdHelp();
    void cmdExit();
    void cmdSay(const std::vector<std::string>& args);
    void cmdCwd();
    void cmdGoto(const std::vector<std::string>& args);
    void cmdMake(const std::vector<std::string>& args);
    void cmdRemove(const std::vector<std::string>& args);
    void cmdList(const std::vector<std::string>& args);
    void cmdSesh(const std::vector<std::string>& args);

    // Logging helper
    void log(const std::string& text);
    void logLn(const std::string& text = "");
    void logError(const std::string& text);
    
    Multiplexer multiplexer;
};

#endif // SHELL_H
