# SEdit - SantOS Text Editor v1.0.0

## Overview
A full-featured text editor for SantOS that allows users to create, edit, and save text files directly from the operating system. SEdit supports file operations, cursor movement, text editing, vertical and horizontal scrolling, and a comprehensive status bar.

## Usage
```
./SEdit <filename>
```

If the file doesn't exist, SEdit starts with an empty buffer and creates the file on first save.

## Features

### 1. File Operations
- **Open File**: Load existing files from the FAT12 filesystem
  - Automatically loads file specified on command line
  - Starts with empty buffer if file doesn't exist
  
- **Save File**: Write buffer contents to disk
  - **Ctrl+S**: Save current file
  - Creates file if it doesn't exist
  - Displays "Saved!" confirmation or "Save FAILED!" error
  - Modified indicator (*) appears in status bar when file has unsaved changes

- **Exit**: Return to shell
  - **Ctrl+Q**: Quit editor
  - Returns to shell immediately (no unsaved changes warning in v1.0.0)

### 2. Cursor Movement
- **Arrow Keys**: Full directional navigation
  - **↑**: Move to previous line (preserves column position when possible)
  - **↓**: Move to next line (preserves column position when possible)
  - **←**: Move one character left
  - **→**: Move one character right
  
- **Smart Line Wrapping**:
  - **←** at start of line: Jump to end of previous line
  - **→** at end of line: Jump to start of next line
  - Column position automatically clamped when moving to shorter lines
  
- **Hardware Cursor**: VGA hardware cursor tracks editing position in real-time

### 3. Text Editing
- **Character Insert**: Type any printable ASCII character (space through ~)
  - Characters inserted at cursor position
  - Existing text shifts right automatically
  - Supports up to 256 characters per line
  
- **Backspace**: Delete character before cursor
  - Remaining text shifts left
  - At start of line: Merges current line with previous line (if combined length fits)
  
- **Enter**: Insert line break
  - Splits current line at cursor position
  - Text after cursor moves to new line
  - Cursor moves to start of new line
  - Supports up to 1000 lines per file

### 4. Display & Scrolling
- **Screen Layout**:
  - Text area: 80 columns × 24 rows
  - Status bar: 1 row (bottom) with inverted colors
  - Total: 80×25 VGA text mode display

- **Vertical Scrolling**:
  - Automatic scrolling when cursor moves beyond visible rows
  - Smooth viewport adjustment keeps cursor visible
  - Scrolls one line early to prevent cursor reaching status bar

- **Horizontal Scrolling** (NEW in v1.0.0):
  - Automatic scrolling when typing past column 79
  - Viewport shifts to keep cursor visible
  - Smooth left/right scrolling as cursor moves

- **Truncation Indicator** (NEW in v1.0.0):
  - `>` symbol appears at column 79 when line extends beyond visible area
  - Indicates more text exists to the right
  - Disappears when scrolled to end of line

- **Status Bar** (Enhanced in v1.0.0):
  - Format: `SEdit v1.0.0 | filename* | Ln:# Col:# ^S=Save ^Q=Quit`
  - Shows editor version number
  - Displays current filename (or "[New File]")
  - `*` indicator when file has unsaved changes
  - Real-time line and column numbers (1-indexed for user display)
  - Keyboard shortcuts reminder
  - Limited to 79 characters to prevent screen scroll bug

### 5. Buffer Management
- **Line-based Storage**: Static array of lines for reliability
  - 1000 lines maximum
  - 256 characters per line maximum
  - Lines stored as null-terminated strings
  
- **Memory Layout**:
  - Program loads at 5MB (0x500000)
  - Stack at 7MB (0x700000)
  - BSS section automatically zeroed by ELF loader
  - No dynamic allocation required for line buffer

## Technical Implementation

### Kernel Syscalls Used
- `SYSCALL_PUTCHAR` (1): Character output
- `SYSCALL_GETCHAR` (2): Character input with keyboard driver
- `SYSCALL_CLEAR` (4): Clear screen
- `SYSCALL_SET_CURSOR` (5): Hardware cursor positioning
- `SYSCALL_SET_COLOR` (6): VGA color control
- `SYSCALL_READ_FILE` (33): Read file from FAT12 filesystem
- `SYSCALL_CREATE_FILE` (34): Create new file
- `SYSCALL_WRITE_FILE` (35): Write file to FAT12 filesystem

### Keyboard Input
- Arrow keys: `KEY_UP` (0x03), `KEY_DOWN` (0x04), `KEY_LEFT` (0x01), `KEY_RIGHT` (0x02)
- Control keys: Ctrl+S (0x13), Ctrl+Q (0x11)
- Backspace: 0x08 or 127 (DEL)
- Enter: '\n' or '\r'
- Printable ASCII: 32-126

### Display Architecture
- Direct VGA text mode manipulation via syscalls
- Hardware cursor for real-time position feedback
- Full screen redraw after each action for consistency
- Explicit cursor positioning prevents VGA scroll bugs

## Version History

### v1.0.0 (Current)
- ✓ Full file operations (open, edit, save)
- ✓ Complete cursor movement with smart line wrapping
- ✓ Text insertion and deletion
- ✓ Vertical scrolling
- ✓ Horizontal scrolling with truncation indicators
- ✓ Comprehensive status bar with version display
- ✓ Modified file indicator
- ✓ Hardware cursor positioning
- ✓ Ctrl+S save and Ctrl+Q quit shortcuts

### v0.2.0
- Added horizontal scrolling support
- Added `>` truncation indicator for long lines
- Enhanced status bar display

### v0.1.3
- Fixed VGA scroll bug (status bar limited to 79 chars)
- Added explicit cursor positioning per line

### v0.1.2
- Fixed scrolling viewport logic
- Improved cursor visibility

### v0.1.1
- Initial release with basic editing features

## Future Enhancements
- Search and replace
- Copy/paste/cut with clipboard
- Undo/redo stack
- Syntax highlighting
- Multiple file buffers
- Line numbers
- Go to line command
- Word wrap mode
- Unsaved changes warning on quit

## Known Limitations
- Maximum 1000 lines per file
- Maximum 256 characters per line
- No undo/redo in v1.0.0
- No clipboard operations in v1.0.0
- No search functionality in v1.0.0

---
**SEdit v1.0.0** - A complete text editor for SantOS
