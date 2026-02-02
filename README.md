# MinSh
A custom lightweight Hybrid Terminal Multiplexer,Shell and Workspace Manager made in C++, for the educational purpose of learning session management and multiplexing/MultiPaning.

## Features
- MultiPlexing and MultiPaning
- Independent Sessions per Pane and usage of mouse wheel to scroll individual panes
- Session Management and Pane management
- Background Sessions
- history tracking for each session
- Simple Core Utility Commands

## Commands
- say <text> - prints text
- goto <path> - goto any directory
- cwd - current directory
- make [-f //file, -d //directory] <filename|dirname> - creates a file or directory
- remove [-f //file, -d //directory] <filename|dirname> - removes a file or directory
- list [-all //list all files and directories,-hidden //list all files and directories including hidden files] <path> - lists all files and directories in the current directory or the specified directory
- sesh <subcommand> - session management:
    - save <name> - saves current session
    - load <name> - loads a session
    - update - updates loaded session
    - remove <name> - removes a session
    - list - lists all sessions
    - add - splits screen with new sessions/Adds a Pane in the screen.
    - switch <number> - switches focus to session <N>
    - detach - moves active session to background
    - retach <index> - brings background session to foreground
- exit - exits the shell

## Setup

- Clone the repository
- Make sure you have CMake and a C++ compiler (like MinGW) installed
- Run `cmake -S . -B build -G "MinGW Makefiles" && cmake --build build`
- Go to the `bin` folder and run the executable `minsh.exe`

## License

This Project is under the GNU General Public License v3.0