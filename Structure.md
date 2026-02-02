# MiniShell

- A minishell with its own commands just to test out session management and multiplexingmade in c++

## Commands[Built-ins]:

- say <text> - prints text
- goto <path>- goto any directory
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
    - add - splits screen with new session
    - switch <number> - switches focus to session N
    - detach - moves active session to background
    - retach <index> - brings background session to foreground

## Prompt:

MinSh@<current_directory>: //Minsh is cyan in color and the directory is in light green color

## Welcome Message:

Welcome to Minsh! 
- Type 'help' to view all commands

## Paness:
Each pane has its own session which the user can switch into using sesh switch
has their own scroll bar as well, and it uses rectangle partition of divide the panes in an even manner. user can detach the said pane and bring it to the foreground using sesh retach <index>.

## Error Messages:

- Minsh: <command>: command not found
- Minsh: <command>: invalid arguments
- Minsh: <filename>: file not found
- Minsh: <directory>: directory not found
- Minsh: <command>: permission denied

## Exit:

- exit - exits the shell
