// Compile: Use CMake (mkdir build && cd build && cmake .. && cmake --build .)
// Run: bin/minsh
// Execute: bin/minsh

#include "Shell.h"

int main(int argc, char* argv[]) {
    std::string exePath = (argc > 0) ? argv[0] : "";
    Shell shell(exePath);
    shell.run();
    return 0;
}
