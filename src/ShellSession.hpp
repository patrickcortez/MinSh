#ifndef SHELL_SESSION_HPP
#define SHELL_SESSION_HPP

#include <string>
#include <vector>
#include <windows.h>

class ShellSession {
public:
    ShellSession();
    ~ShellSession();

    // Environment
    void setCwd(const std::string& path);
    std::string getCwd() const;

    // Execution
    bool execute(const std::string& cmd);
    std::string pollOutput();
    bool isBusy();
    
    // Future input support
    void writeInput(const std::string& input);

    // History
    void initHistory(const std::string& exePath);
    void saveHistory();
    void addHistory(const std::string& cmd);
    std::string historyUp(const std::string& currentContext);
    std::string historyDown();
    void resetHistoryIndex();

private:
    std::string currentDirectory;
    std::string historyFilePath;
    std::vector<std::string> history;
    int historyIndex = -1;
    std::string tempHistoryInput; // Preserve current input when moving up
    
    HANDLE hProcess;
    HANDLE hThread;
    
    HANDLE hChildOutRead;
    HANDLE hChildOutWrite;
    HANDLE hChildErrWrite;
    
    // We might need stdin later, for now we assume non-interactive commands mostly
    // or we implement stdin pipe similarly
    HANDLE hChildInRead;
    HANDLE hChildInWrite;

    void createPipes();
    void closePipes();
    void cleanupProcess();
};

#endif // SHELL_SESSION_HPP
