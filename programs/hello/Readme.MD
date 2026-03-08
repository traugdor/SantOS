# Hello - SantOS Example Program

## Overview
A demonstration program for SantOS that showcases userspace program capabilities. The hello program provides a menu-driven interface with two example applications: a calculator and a prototype spell checker.

## Usage
```
./hello
```

The program displays a menu and waits for user input.

## Features

### Main Menu
```
================================
=                              =
=        SantOS Example        =
=                              =
================================
1. Calculator
2. prototype spell checker
3. Exit
```

### 1. Calculator
An interactive calculator supporting basic arithmetic operations:

- **Addition** - Add two numbers
- **Subtraction** - Subtract second number from first
- **Multiplication** - Multiply two numbers
- **Division** - Divide first number by second (with zero-check)

#### Usage
1. Select option 1 from main menu
2. Choose operation (1-5)
3. Enter first number
4. Enter second number
5. View result
6. Repeat or select 5 to exit

#### Example
```
Calculator Program

Select the function:
1. Addition
2. Subtraction
3. Multiplication
4. Division
5. Exit

Enter your choice: 1
Enter first number: 42
Enter second number: 8
Result: 42 + 8 = 50
```

#### Features
- Integer arithmetic
- Division by zero protection
- Input validation
- Continuous operation until exit

### 2. Prototype Spell Checker
A simple dictionary-based spell checker with 50 commonly misspelled words.

#### Dictionary Words
The spell checker includes words like:
- accommodate, achieve, acquire, a lot, apparent, argument
- calendar, cemetery, definitely, discipline, embarrass
- environment, existence, experience, familiar, February
- government, grammar, guarantee, hierarchy, independent
- And 35 more commonly misspelled words

#### Usage
1. Select option 2 from main menu
2. Enter a word to check
3. Program searches dictionary
4. Displays "Word found in dictionary!" or "Word not found - possible spelling error"

#### Example
```
Prototype Spell Checker

Input a word and I will attempt to find spelling errors for you!
Input: definitely
You entered: definitely
Word found in dictionary!
```

## Technical Implementation

### Input Handling
- Uses `scanf()` for formatted input
- Supports integer input for menu choices
- Supports string input for spell checker
- Echo feedback for user confirmation

### Kernel Syscalls Used
- `SYSCALL_PUTCHAR` (1) - Character output
- `SYSCALL_GETCHAR` (2) - Character input
- `SYSCALL_PRINTF` (3) - Formatted output
- `SYSCALL_CLEAR` (4) - Clear screen
- `SYSCALL_STRCMP` (20) - String comparison for spell checker

### Memory Layout
- Program loads at 5MB (0x500000)
- Stack at 7MB (0x700000)
- Dictionary stored in read-only data section
- Input buffers on stack

### String Operations
- Dictionary lookup using linear search
- Case-sensitive string comparison
- Maximum input length: 256 characters

## Code Structure

```c
int main(int argc, char** argv)
    └─> Menu loop
        ├─> Option 1: calculator()
        │   └─> Operation loop with arithmetic
        ├─> Option 2: proto_spell_checker()
        │   └─> Dictionary search
        └─> Option 3: Exit
```

## Educational Purpose

This program demonstrates:
- **Menu-driven interfaces** - User-friendly program flow
- **Input validation** - Checking for invalid choices and division by zero
- **Loops** - Do-while loops for continuous operation
- **Conditionals** - If-else chains for menu handling
- **Arrays** - Dictionary storage using string array
- **String comparison** - Dictionary lookup algorithm
- **Formatted I/O** - Using printf and scanf
- **Program structure** - Multiple functions and modular design

## Known Limitations
- Calculator only supports integers (no floating point)
- Spell checker has limited 50-word dictionary
- Spell checker is case-sensitive
- No suggestions for misspelled words
- No history of calculations
- No ability to chain operations

## Future Enhancements
- Floating-point arithmetic support
- Larger dictionary with more words
- Case-insensitive spell checking
- Spelling suggestions (edit distance algorithm)
- Calculator history
- Expression evaluation (e.g., "2 + 3 * 4")
- Scientific calculator functions
- Memory storage (M+, M-, MR, MC)

## Example Session

```
> ./hello

================================
=                              =
=        SantOS Example        =
=                              =
================================
1. Calculator
2. prototype spell checker
3. Exit

Enter your choice: 1

Calculator Program

Select the function:
1. Addition
2. Subtraction
3. Multiplication
4. Division
5. Exit

Enter your choice: 1
Enter first number: 15
Enter second number: 7
Result: 15 + 7 = 22

Enter your choice: 4
Enter first number: 100
Enter second number: 4
Result: 100 / 4 = 25

Enter your choice: 5

================================
=                              =
=        SantOS Example        =
=                              =
================================
1. Calculator
2. prototype spell checker
3. Exit

Enter your choice: 2

Prototype Spell Checker

Input a word and I will attempt to find spelling errors for you!
Input: wierd
You entered: wierd
Word not found - possible spelling error

Enter your choice: 3
```

---
**Hello v1.0** - A demonstration program for SantOS
