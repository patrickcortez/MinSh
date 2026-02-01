# MinSh
A custom lightweight Command Dispatcher made in C++, for the educational purpose of learning session management.

## Features
- Session Management
- Simple Core Utility Commands

## Commands
- say <text> - prints text
- goto <path> - goto any directory
- cwd - current directory
- make [-f //file, -d //directory] <filename|dirname> - creates a file or directory
- remove [-f //file, -d //directory] <filename|dirname> - removes a file or directory
- list [-all //list all files and directories,-hidden //list all files and directories including hidden files] <path> - lists all files and directories in the current directory or the specified directory
- exit - exits the shell

## Setup

- Clone the repository
- Make sure you have Cmake
- Run `cmake -S . -B build -G "MinGW Makefiles" && cmake --build build`
- Go to the bin folder and run the executable `Minsh.exe`

## License

This Project is under the GNU General Public License v3.0