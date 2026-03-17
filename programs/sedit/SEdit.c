#include "../../include/stdio.h"
#include "../../include/string.h"
#include "../../include/stdlib.h"

int MAJOR = 1;
int MINOR = 1;
int PATCH = 0;

char VERSION[20] = "";

// Editor constants
#define MAX_LINES 1000
#define MAX_LINE_LEN 256
#define SCREEN_COLS 80
#define SCREEN_ROWS 24    // 25 total - 1 for status bar
#define CURSOR_LEFT_CH 0x1D  // Non-destructive cursor left (VGA driver)

// Editor state
static char lines[MAX_LINES][MAX_LINE_LEN];
static int line_count = 0;       // Total lines in file
static int cursor_row = 0;       // Cursor row in file (0-indexed)
static int cursor_col = 0;       // Cursor column in line (0-indexed)
static int scroll_row = 0;       // First visible row (viewport top)
static int scroll_col = 0;       // First visible column (viewport left)
static int modified = 0;         // 1 if file has unsaved changes
static char filename[64] = "";   // Current file name

void build_version(char *version)
{
    // Build "SEdit v#.#.#" string manually
    char* dst = version;
    const char* prefix = "SEdit v";
    while (*prefix) *dst++ = *prefix++;

    // Major
    if (MAJOR >= 10) *dst++ = '0' + (MAJOR / 10);
    *dst++ = '0' + (MAJOR % 10);
    *dst++ = '.';

    // Minor
    if (MINOR >= 10) *dst++ = '0' + (MINOR / 10);
    *dst++ = '0' + (MINOR % 10);
    *dst++ = '.';

    // Patch
    if (PATCH >= 10) *dst++ = '0' + (PATCH / 10);
    *dst++ = '0' + (PATCH % 10);

    *dst = '\0';
}

// Draw the status bar on row 24 (last row)
static void draw_status_bar(const char* message) {
    // Position cursor at row 24 (use clear_screen + redraw approach)
    // We'll draw the full status bar as part of the redraw
    set_color(COLOR_BLACK, COLOR_WHITE);  // Inverted colors for status bar

    // Print status: version, filename, line:col, modified indicator, key hints
    // Limit to SCREEN_COLS-1 chars to prevent cursor wrap on last row causing scroll
    int max_cols = SCREEN_COLS - 1;
    int printed = 0;

    // Version
    char* v = VERSION;
    while (*v && printed < max_cols) { putchar(*v++); printed++; }

    // Separator
    if (printed < max_cols) { putchar(' '); printed++; }
    if (printed < max_cols) { putchar('|'); printed++; }
    if (printed < max_cols) { putchar(' '); printed++; }

    // Filename
    char* f = filename;
    if (f[0] == '\0') {
        const char* untitled = "[New File]";
        while (*untitled && printed < max_cols) { putchar(*untitled++); printed++; }
    } else {
        while (*f && printed < max_cols) { putchar(*f++); printed++; }
    }

    // Modified indicator
    if (modified && printed < max_cols) { putchar('*'); printed++; }

    // Separator
    if (printed < max_cols) { putchar(' '); printed++; }
    if (printed < max_cols) { putchar('|'); printed++; }
    if (printed < max_cols) { putchar(' '); printed++; }

    // Line:Col
    const char* lc = "Ln:";
    while (*lc && printed < max_cols) { putchar(*lc++); printed++; }
    // Print cursor_row+1
    int row_num = cursor_row + 1;
    char num_buf[8];
    int ni = 0;
    if (row_num == 0) { num_buf[ni++] = '0'; }
    else {
        int tmp = row_num;
        char rev[8];
        int ri = 0;
        while (tmp > 0) { rev[ri++] = '0' + (tmp % 10); tmp /= 10; }
        while (ri > 0) num_buf[ni++] = rev[--ri];
    }
    for (int i = 0; i < ni && printed < max_cols; i++) { putchar(num_buf[i]); printed++; }

    if (printed < max_cols) { putchar(' '); printed++; }
    const char* cc = "Col:";
    while (*cc && printed < max_cols) { putchar(*cc++); printed++; }
    int col_num = cursor_col + 1;
    ni = 0;
    if (col_num == 0) { num_buf[ni++] = '0'; }
    else {
        int tmp = col_num;
        char rev[8];
        int ri = 0;
        while (tmp > 0) { rev[ri++] = '0' + (tmp % 10); tmp /= 10; }
        while (ri > 0) num_buf[ni++] = rev[--ri];
    }
    for (int i = 0; i < ni && printed < max_cols; i++) { putchar(num_buf[i]); printed++; }

    // Key hints or message on the right side
    // If there's a message, show it; otherwise show key hints
    const char* hint;
    if (message && message[0]) {
        hint = message;
    } else {
        hint = "^S=Save ^Q=Quit";
    }
    int hint_len = 0;
    const char* h = hint;
    while (*h) { hint_len++; h++; }

    // Pad with spaces to push hint to right
    int pad = max_cols - printed - hint_len;
    for (int i = 0; i < pad && printed < max_cols; i++) { putchar(' '); printed++; }

    // Print hint
    h = hint;
    while (*h && printed < max_cols) { putchar(*h++); printed++; }

    // Fill remaining
    while (printed < max_cols) { putchar(' '); printed++; }

    set_color(COLOR_WHITE, COLOR_BLACK);  // Restore normal colors
}

// Redraw the entire screen
static void redraw_screen(const char* status_message) {
    // Do NOT use clear_screen() - it causes visible flashing in VirtualBox
    // because the VGA refresh catches the blank frame. Instead, overwrite
    // each line with content padded with spaces.

    // Draw visible lines
    for (int screen_y = 0; screen_y < SCREEN_ROWS; screen_y++) {
        set_cursor_pos(0, screen_y);
        int file_row = scroll_row + screen_y;
        int visible = 0;
        if (file_row < line_count) {
            int len = strlen(lines[file_row]);
            // Print visible portion starting from scroll_col
            for (int x = scroll_col; x < len && visible < SCREEN_COLS - 1; x++) {
                putchar(lines[file_row][x]);
                visible++;
            }
            // Show '>' indicator if line extends beyond visible area
            if (len > scroll_col + SCREEN_COLS - 1) {
                // Pad up to the last column, then show '>'
                while (visible < SCREEN_COLS - 1) { putchar(' '); visible++; }
                putchar('>');
                visible++;
            }
        }
        // Pad remainder of line with spaces to clear old content
        while (visible < SCREEN_COLS) { putchar(' '); visible++; }
    }

    // Position cursor at status bar row and draw it
    set_cursor_pos(0, SCREEN_ROWS);
    draw_status_bar(status_message);

    // Position VGA cursor at the editor cursor position (adjusted for scroll)
    set_cursor_pos((unsigned char)(cursor_col - scroll_col), (unsigned char)(cursor_row - scroll_row));
}

// Load a file into the editor buffer
static int load_file(const char* fname) {
    char buf[32768];  // 32KB buffer
    int bytes = read_file(fname, buf, sizeof(buf) - 1);
    if (bytes < 0) {
        return -1;
    }
    buf[bytes] = '\0';

    // Parse into lines
    line_count = 0;
    int col = 0;
    for (int i = 0; i < bytes && line_count < MAX_LINES; i++) {
        if (buf[i] == '\n') {
            lines[line_count][col] = '\0';
            line_count++;
            col = 0;
        } else if (buf[i] == '\r') {
            // Skip carriage returns
        } else if (col < MAX_LINE_LEN - 1) {
            lines[line_count][col++] = buf[i];
        }
    }
    // Handle last line (if no trailing newline)
    if (col > 0 || (bytes > 0 && buf[bytes-1] == '\n')) {
        lines[line_count][col] = '\0';
        line_count++;
    }

    // Ensure at least one line
    if (line_count == 0) {
        lines[0][0] = '\0';
        line_count = 1;
    }

    return 0;
}

// Save the editor buffer to file
static int save_file(void) {
    if (filename[0] == '\0') return -1;

    // Flatten lines into a buffer
    char buf[32768];
    int pos = 0;
    for (int i = 0; i < line_count && pos < (int)sizeof(buf) - 2; i++) {
        int len = strlen(lines[i]);
        for (int j = 0; j < len && pos < (int)sizeof(buf) - 2; j++) {
            buf[pos++] = lines[i][j];
        }
        buf[pos++] = '\n';
    }

    int result = write_file(filename, buf, pos);
    if (result > 0) {
        modified = 0;
        return 0;
    }
    return -1;
}

// Ensure cursor is within visible viewport
static void ensure_cursor_visible(void) {
    // Vertical scrolling
    if (cursor_row < scroll_row) {
        scroll_row = cursor_row;
    }
    if (cursor_row >= scroll_row + SCREEN_ROWS - 1) {
        scroll_row = cursor_row - SCREEN_ROWS + 2;
    }
    // Horizontal scrolling
    if (cursor_col < scroll_col) {
        scroll_col = cursor_col;
    }
    if (cursor_col >= scroll_col + SCREEN_COLS - 1) {
        scroll_col = cursor_col - SCREEN_COLS + 2;
    }
}

// Clamp cursor_col to valid range for current line
static void clamp_cursor_col(void) {
    int len = strlen(lines[cursor_row]);
    if (cursor_col > len) {
        cursor_col = len;
    }
}

int main(int argc, char** argv) {
    build_version(VERSION);

    // Initialize empty buffer
    lines[0][0] = '\0';
    line_count = 1;
    cursor_row = 0;
    cursor_col = 0;
    scroll_row = 0;
    modified = 0;

    // Load file if specified
    if (argc >= 2) {
        // Copy filename
        char* src = argv[1];
        int i = 0;
        while (src[i] && i < 63) {
            filename[i] = src[i];
            i++;
        }
        filename[i] = '\0';

        if (load_file(filename) != 0) {
            // File doesn't exist - start with empty buffer (will create on save)
            lines[0][0] = '\0';
            line_count = 1;
        }
    }

    char status_msg[80] = "";
    redraw_screen(status_msg);

    // Main editor loop
    while (1) {
        int c = getchar();

        status_msg[0] = '\0';  // Clear status message

        if (c == 17) {
            // Ctrl+Q = Quit (ASCII 17)
            break;
        } else if (c == 19) {
            // Ctrl+S = Save (ASCII 19)
            if (filename[0] == '\0') {
                const char* msg = "No filename! Use: sedit <file>";
                int mi = 0;
                while (msg[mi]) { status_msg[mi] = msg[mi]; mi++; }
                status_msg[mi] = '\0';
            } else {
                if (save_file() == 0) {
                    const char* msg = "Saved!";
                    int mi = 0;
                    while (msg[mi]) { status_msg[mi] = msg[mi]; mi++; }
                    status_msg[mi] = '\0';
                } else {
                    const char* msg = "Save FAILED!";
                    int mi = 0;
                    while (msg[mi]) { status_msg[mi] = msg[mi]; mi++; }
                    status_msg[mi] = '\0';
                }
            }
        } else if (c == KEY_UP) {
            if (cursor_row > 0) {
                cursor_row--;
                clamp_cursor_col();
                ensure_cursor_visible();
            }
        } else if (c == KEY_DOWN) {
            if (cursor_row < line_count - 1) {
                cursor_row++;
                clamp_cursor_col();
                ensure_cursor_visible();
            }
        } else if (c == KEY_LEFT) {
            if (cursor_col > 0) {
                cursor_col--;
                ensure_cursor_visible();
            } else if (cursor_row > 0) {
                // Wrap to end of previous line
                cursor_row--;
                cursor_col = strlen(lines[cursor_row]);
                ensure_cursor_visible();
            }
        } else if (c == KEY_RIGHT) {
            int len = strlen(lines[cursor_row]);
            if (cursor_col < len) {
                cursor_col++;
                ensure_cursor_visible();
            } else if (cursor_row < line_count - 1) {
                // Wrap to start of next line
                cursor_row++;
                cursor_col = 0;
                ensure_cursor_visible();
            }
        } else if (c == '\n' || c == '\r') {
            // Enter: split line at cursor
            if (line_count < MAX_LINES) {
                // Shift lines down
                for (int i = line_count; i > cursor_row + 1; i--) {
                    strcpy(lines[i], lines[i - 1]);
                }
                line_count++;

                // Split current line
                int len = strlen(lines[cursor_row]);
                // Copy text after cursor to new line
                strcpy(lines[cursor_row + 1], &lines[cursor_row][cursor_col]);
                // Truncate current line at cursor
                lines[cursor_row][cursor_col] = '\0';

                // Move cursor to start of new line
                cursor_row++;
                cursor_col = 0;
                modified = 1;
                ensure_cursor_visible();
            }
        } else if (c == '\b' || c == 127) {
            // Backspace
            if (cursor_col > 0) {
                // Delete character before cursor in current line
                int len = strlen(lines[cursor_row]);
                for (int i = cursor_col - 1; i < len; i++) {
                    lines[cursor_row][i] = lines[cursor_row][i + 1];
                }
                cursor_col--;
                modified = 1;
                ensure_cursor_visible();
            } else if (cursor_row > 0) {
                // Merge with previous line
                int prev_len = strlen(lines[cursor_row - 1]);
                int cur_len = strlen(lines[cursor_row]);
                if (prev_len + cur_len < MAX_LINE_LEN) {
                    // Append current line to previous
                    strcat(lines[cursor_row - 1], lines[cursor_row]);

                    // Shift lines up
                    for (int i = cursor_row; i < line_count - 1; i++) {
                        strcpy(lines[i], lines[i + 1]);
                    }
                    line_count--;
                    lines[line_count][0] = '\0';

                    cursor_row--;
                    cursor_col = prev_len;
                    modified = 1;
                    ensure_cursor_visible();
                }
            }
        } else if (c >= 32 && c <= 126) {
            // Printable character: insert at cursor position
            int len = strlen(lines[cursor_row]);
            if (len < MAX_LINE_LEN - 1) {
                // Shift text right
                for (int i = len + 1; i > cursor_col; i--) {
                    lines[cursor_row][i] = lines[cursor_row][i - 1];
                }
                lines[cursor_row][cursor_col] = (char)c;
                cursor_col++;
                modified = 1;
                ensure_cursor_visible();
            }
        } else {
            // Unknown key - ignore
            continue;
        }

        // Redraw after every action
        redraw_screen(status_msg);
    }
    
    return 0;
}