#include "ShellSession.hpp"
#include <iostream>
#include <vector>

ShellSession::ShellSession() 
    : hProcess(NULL), hThread(NULL), 
      hChildOutRead(NULL), hChildOutWrite(NULL), 
      hChildErrWrite(NULL), hChildInRead(NULL), hChildInWrite(NULL) 
{
    // Default CWD to USERPROFILE or Current Directory
    char buffer[MAX_PATH];
    if (GetCurrentDirectoryA(MAX_PATH, buffer)) {
        currentDirectory = std::string(buffer);
    }
}

ShellSession::~ShellSession() {
    cleanupProcess();
    closePipes();
}

void ShellSession::setCwd(const std::string& path) {
    currentDirectory = path;
}

std::string ShellSession::getCwd() const {
    return currentDirectory;
}

void ShellSession::createPipes() {
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process's STDOUT.
    if (!CreatePipe(&hChildOutRead, &hChildOutWrite, &saAttr, 0)) {
        return;
    }
    // Ensure the read handle to the pipe for STDOUT is not inherited.
    SetHandleInformation(hChildOutRead, HANDLE_FLAG_INHERIT, 0);

    // Create a pipe for the child process's STDIN.
    if (!CreatePipe(&hChildInRead, &hChildInWrite, &saAttr, 0)) {
        return;
    }
    // Ensure the write handle to the pipe for STDIN is not inherited.
    SetHandleInformation(hChildInWrite, HANDLE_FLAG_INHERIT, 0);
}

void ShellSession::closePipes() {
    if (hChildOutRead) { CloseHandle(hChildOutRead); hChildOutRead = NULL; }
    if (hChildOutWrite) { CloseHandle(hChildOutWrite); hChildOutWrite = NULL; }
    if (hChildInRead) { CloseHandle(hChildInRead); hChildInRead = NULL; }
    if (hChildInWrite) { CloseHandle(hChildInWrite); hChildInWrite = NULL; }
    
    // Error write is usually a duplicate of out write, but if separate:
    // We will just use same handle for stdout and stderr for simplicity or duplicate it
}

void ShellSession::cleanupProcess() {
    if (hProcess) {
        CloseHandle(hProcess);
        hProcess = NULL;
    }
    if (hThread) {
        CloseHandle(hThread);
        hThread = NULL;
    }
}

bool ShellSession::execute(const std::string& cmd) {
    if (isBusy()) return false; // Already running logic? Or queue? For now, block/ignore.

    createPipes();

    std::string cmdLine = cmd; 
    // Note: In real implementation, might need "cmd /c" or similar if it's a shell command vs executable
    
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    BOOL bSuccess = FALSE;

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = hChildOutWrite;
    siStartInfo.hStdOutput = hChildOutWrite;
    siStartInfo.hStdInput = hChildInRead;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    // Create the child process.
    bSuccess = CreateProcessA(NULL,
        const_cast<char*>(cmdLine.c_str()),     // command line
        NULL,          // process security attributes
        NULL,          // primary thread security attributes
        TRUE,          // handles are inherited
        0,             // creation flags
        NULL,          // use parent's environment
        currentDirectory.c_str(), // use session's CWD
        &siStartInfo,  // STARTUPINFO
        &piProcInfo);  // PROCESS_INFORMATION

    // Close the write end of the output pipe in the parent, 
    // otherwise ReadFile will block forever waiting for EOF.
    // Also close read end of input pipe.
    if (hChildOutWrite) { CloseHandle(hChildOutWrite); hChildOutWrite = NULL; }
    if (hChildInRead) { CloseHandle(hChildInRead); hChildInRead = NULL; }

    if (bSuccess) {
        hProcess = piProcInfo.hProcess;
        hThread = piProcInfo.hThread;
        return true;
    } else {
        // Failed
        closePipes();
        return false;
    }
}

bool ShellSession::isBusy() {
    if (hProcess == NULL) return false;
    
    DWORD dwExitCode = 0;
    if (GetExitCodeProcess(hProcess, &dwExitCode)) {
        if (dwExitCode == STILL_ACTIVE) return true;
    }
    
    // If we're here, it finished.
    // But we might still have data in pipe!
    // We should only say "not busy" if pipe is empty AND process finished.
    // However, typical pattern: poll until empty, then check process.
    
    cleanupProcess();
    return false;
}

std::string ShellSession::pollOutput() {
    if (!hChildOutRead) return "";

    DWORD dwRead, dwAvail, dwLeft;
    BOOL bSuccess = PeekNamedPipe(hChildOutRead, NULL, 0, NULL, &dwAvail, &dwLeft);
    
    if (!bSuccess || dwAvail == 0) return "";
    
    std::string result(dwAvail, 0);
    bSuccess = ReadFile(hChildOutRead, &result[0], dwAvail, &dwRead, NULL);
    
    if (!bSuccess || dwRead == 0) return "";
    
    if (dwRead < dwAvail) {
        result.resize(dwRead);
    }
    return result;
}

void ShellSession::writeInput(const std::string& input) {
    if (!hChildInWrite) return;
    
    DWORD dwWritten;
    WriteFile(hChildInWrite, input.data(), input.size(), &dwWritten, NULL);
}
