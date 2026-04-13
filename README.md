# myshell

A cross-platform command-line shell written in C++ that runs on both Windows and Linux.

## Features

- Command execution
- `cd` with live prompt update showing current directory
- Command history with up/down arrow navigation
- Cursor movement with left/right arrows
- Character insertion at cursor position
- Backspace at any cursor position
- Dynamic piping — chain any number of commands with `|`
- Ctrl+C handling with clean terminal restore
- Cross-platform — builds on Windows and Linux

## Building

**Linux / WSL:**
```bash
g++ main.cpp -o myshell
./myshell
```

**Windows (Visual Studio):**
Open the solution and build in Release mode.

## Usage
/home/user myshell-> ls | grep cpp | sort
/home/user myshell-> cd /home
/home/user myshell-> exit

## What I Learned

- Process creation with `fork/exec` (Linux) and `CreateProcess` (Windows)
- Inter-process communication using pipes
- Terminal raw mode and escape sequences
- Cross-platform development with preprocessor directives
- Signal handling