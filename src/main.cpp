// Compile: g++ -Wall -std=c++17 src/main.cpp src/Shell.cpp src/Sessions.cpp -o bin/minsh
// Run: bin/minsh
// Execute: bin/minsh

#include "Shell.h"

int main(int argc, char* argv[]) {
    std::string exePath = (argc > 0) ? argv[0] : "";
    Shell shell(exePath);
    shell.run();
    return 0;
}
