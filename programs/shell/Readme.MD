# Shell - SantOS Command Interpreter

## Overview
The SantOS shell is a powerful command-line interpreter that provides an interactive interface to the operating system. It supports built-in commands, program execution, scripting, environment variables, and command piping.

## Usage
The shell starts automatically when SantOS boots. It displays a prompt and waits for user commands:
```
SantOS Shell v1.0 - Type 'help' for commands
> 
```

## Features

### 1. Built-in Commands

#### System Information
- **`help`** - Display list of available commands
- **`clear`** - Clear the screen
- **`echo <text>`** - Print text to the screen
  - Example: `echo Hello, World!`
  - Supports environment variable expansion: `echo $VAR`

#### File Operations
- **`ls`** - List files in the root directory
  - Shows filename, size, and type (file/directory)
- **`cat <filename>`** - Display file contents
  - Example: `cat story.txt`
- **`read <filename>`** - Interactive file viewer with paging
  - Press any key to continue, 'q' to quit
  - Example: `read story.txt`

#### Program Execution
- **`./program [args]`** - Execute ELF programs
  - Example: `./SEdit readme.txt`
  - Supports command-line arguments (up to 16 arguments)
  - Arguments can be quoted: `./program "arg with spaces"`
  - Programs load at 5MB with dedicated stack at 7MB

#### Script Execution
- **`./script.sh`** - Execute sosh scripts
  - Scripts must start with `##/sosh` marker
  - Supports all shell commands
  - Environment variables persist across script lines
  - Example: `./test.sh`

### 2. Environment Variables

#### Variable Operations
- **`$VAR="value"`** - Set variable to string value
  - Example: `$NAME="Alice"`
- **`$VAR=123`** - Set variable to numeric value
  - Example: `$COUNT=5`
- **`$VAR+="text"`** - Append string to variable
  - Example: `$PATH+="/bin"`
- **`$VAR+=5`** - Add number to variable
  - Example: `$COUNT+=10`

#### Variable Usage
- **`echo $VAR`** - Display variable value
- Variables expand in echo commands
- Variables persist across commands in the same session
- Up to 100 variables supported
- Variable names and values up to 64 characters

#### Built-in Variable Operations
- **`add $VAR <number>`** - Add number to variable
  - Example: `add $COUNT 5`
- **`sub $VAR <number>`** - Subtract number from variable
  - Example: `sub $COUNT 3`

### 3. Command Piping

Chain commands together using the pipe operator `|`:

```
cat file.txt | read
```

Supported pipe chains:
- **`cat | read`** - Display file with paging
- **`ls | read`** - List files with paging
- **`echo | read`** - Echo text with paging

The pipe operator passes output from the left command to the right command as input.

### 4. Command History

- **Up Arrow (↑)** - Recall previous command
- **Down Arrow (↓)** - Recall next command in history
- Stores last 10 commands
- History persists during shell session
- Navigate through history with arrow keys

### 5. Inline Editing

- **Left Arrow (←)** - Move cursor left in command line
- **Right Arrow (→)** - Move cursor right in command line
- **Backspace** - Delete character before cursor
- **Home** - Move cursor to start of line
- **End** - Move cursor to end of line
- Edit anywhere in the command line before pressing Enter

## Technical Implementation

### Architecture
- Loads at 1MB (0x100000) to avoid conflicts
- Entry point: `shell_main` instead of `main`
- Dedicated memory space separate from other programs
- Cannot execute itself (prevented by kernel)

### Command Processing
1. Read line from keyboard with inline editing support
2. Parse command and arguments (supports quoted strings)
3. Check for built-in commands
4. Check for script marker (`##/sosh`)
5. Attempt ELF program execution
6. Display error if command not found

### Kernel Syscalls Used
- `SYSCALL_PUTCHAR` (1) - Character output
- `SYSCALL_GETCHAR` (2) - Character input
- `SYSCALL_PRINTF` (3) - Formatted output
- `SYSCALL_CLEAR` (4) - Clear screen
- `SYSCALL_SET_COLOR` (6) - VGA color control
- `SYSCALL_LIST_DIR` (30) - List root directory
- `SYSCALL_READ_FILE` (33) - Read file contents
- `SYSCALL_EXEC_PROGRAM` (40) - Execute ELF programs

### Memory Management
- Static buffers for command line and variables
- No dynamic allocation required
- Command buffer: 256 characters
- Variable storage: 100 variables × 64 chars each
- History buffer: 10 commands × 256 chars each

## Scripting

### Script Format
Scripts must begin with the sosh marker:
```bash
##/sosh
echo Starting script...
$VAR=10
echo Value: $VAR
add $VAR 5
echo New value: $VAR
```

### Script Features
- All shell commands supported
- Environment variables persist across lines
- Comments not supported in v1.0
- Sequential execution (no conditionals or loops in v1.0)

### Example Script
```bash
##/sosh
echo This is a test of the scripting system.
$VAR1="Test Value"
echo $VAR1
$VAR2=5
echo VAR2: $VAR2
add $VAR2 5
echo Added 5 to VAR2: $VAR2
```

## Examples

### Basic Commands
```
> help
> clear
> echo Hello, SantOS!
> ls
```

### File Operations
```
> cat story.txt
> read story.txt
> ls | read
```

### Variables
```
> $NAME="Alice"
> echo Hello, $NAME
> $COUNT=0
> add $COUNT 1
> echo Count: $COUNT
```

### Program Execution
```
> ./SEdit readme.txt
> ./hello
```

### Piping
```
> cat story.txt | read
> echo "Long text here" | read
```

## Known Limitations
- Maximum 256 characters per command line
- Maximum 100 environment variables
- Maximum 10 commands in history
- No conditional statements (if/else)
- No loops (for/while)
- No functions or procedures
- No comments in scripts
- No background processes
- No command substitution
- Pipe operator limited to specific command pairs

## Future Enhancements
- Conditional statements (if/then/else)
- Loop constructs (for/while)
- Functions and procedures
- Script comments
- Command substitution
- More flexible piping
- Background processes (&)
- Job control
- Tab completion
- Wildcard expansion (*.txt)
- Redirection (>, <, >>)
- Command aliases

---
**Shell v1.0** - The SantOS command interpreter
