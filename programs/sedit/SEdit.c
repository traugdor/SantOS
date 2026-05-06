#include "../../include/stdio.h"
#include "../../include/string.h"
#include "../../include/stdlib.h"

int MAJOR = 1;
int MINOR = 1;
int PATCH = 0;

char VERSION[20] = "";

#define MAX_LINES    1000
#define MAX_LINE_LEN 256
#define SCREEN_COLS  80
#define SCREEN_ROWS  24   // 25 total rows - 1 for status bar

// Editor state
static char lines[MAX_LINES][MAX_LINE_LEN];
static int line_count = 0;
static int cursor_row = 0;
static int cursor_col = 0;
static int scroll_row = 0;
static int scroll_col = 0;
static int modified   = 0;
static char filename[64] = "";

// Shadow buffers — track what is currently rendered on screen.
// draw_line() and update_status() compare against these and only call
// putchar_at() for cells whose content has actually changed.
static char line_shadow[SCREEN_ROWS][SCREEN_COLS];
static char status_shadow[SCREEN_COLS];
static int  shadow_valid = 0;

// ---------------------------------------------------------------------------
// Version string
// ---------------------------------------------------------------------------
static void build_version(char* version) {
    char* dst = version;
    const char* prefix = "SEdit v";
    while (*prefix) *dst++ = *prefix++;
    if (MAJOR >= 10) *dst++ = '0' + (MAJOR / 10);
    *dst++ = '0' + (MAJOR % 10);
    *dst++ = '.';
    if (MINOR >= 10) *dst++ = '0' + (MINOR / 10);
    *dst++ = '0' + (MINOR % 10);
    *dst++ = '.';
    if (PATCH >= 10) *dst++ = '0' + (PATCH / 10);
    *dst++ = '0' + (PATCH % 10);
    *dst = '\0';
}

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------
static int load_file(const char* fname) {
    char buf[32768];
    int bytes = read_file(fname, buf, sizeof(buf) - 1);
    if (bytes < 0) return -1;
    buf[bytes] = '\0';

    line_count = 0;
    int col = 0;
    for (int i = 0; i < bytes && line_count < MAX_LINES; i++) {
        if (buf[i] == '\n') {
            lines[line_count][col] = '\0';
            line_count++;
            col = 0;
        } else if (buf[i] == '\r') {
            // skip
        } else if (col < MAX_LINE_LEN - 1) {
            lines[line_count][col++] = buf[i];
        }
    }
    if (col > 0 || (bytes > 0 && buf[bytes - 1] == '\n')) {
        lines[line_count][col] = '\0';
        line_count++;
    }
    if (line_count == 0) { lines[0][0] = '\0'; line_count = 1; }
    return 0;
}

static int save_file(void) {
    if (filename[0] == '\0') return -1;
    char buf[32768];
    int pos = 0;
    for (int i = 0; i < line_count && pos < (int)sizeof(buf) - 2; i++) {
        int len = strlen(lines[i]);
        for (int j = 0; j < len && pos < (int)sizeof(buf) - 2; j++)
            buf[pos++] = lines[i][j];
        buf[pos++] = '\n';
    }
    int result = write_file(filename, buf, pos);
    if (result > 0) { modified = 0; return 0; }
    return -1;
}

// ---------------------------------------------------------------------------
// Cursor / scroll helpers
// ---------------------------------------------------------------------------
static void ensure_cursor_visible(void) {
    if (cursor_row < scroll_row)
        scroll_row = cursor_row;
    if (cursor_row >= scroll_row + SCREEN_ROWS - 1)
        scroll_row = cursor_row - SCREEN_ROWS + 2;
    if (cursor_col < scroll_col)
        scroll_col = cursor_col;
    if (cursor_col >= scroll_col + SCREEN_COLS - 1)
        scroll_col = cursor_col - SCREEN_COLS + 2;
}

static void clamp_cursor_col(void) {
    int len = strlen(lines[cursor_row]);
    if (cursor_col > len) cursor_col = len;
}

// ---------------------------------------------------------------------------
// Drawing infrastructure
// ---------------------------------------------------------------------------

// Build what one text row should look like into buf[SCREEN_COLS].
static void build_line_buf(int screen_y, char buf[SCREEN_COLS]) {
    int file_row = scroll_row + screen_y;
    int x = 0;
    if (file_row < line_count) {
        int len = strlen(lines[file_row]);
        for (int c = scroll_col; c < len && x < SCREEN_COLS - 1; c++)
            buf[x++] = lines[file_row][c];
        if (len > scroll_col + SCREEN_COLS - 1) {
            while (x < SCREEN_COLS - 1) buf[x++] = ' ';
            buf[x++] = '>';
        }
    }
    while (x < SCREEN_COLS) buf[x++] = ' ';
}

// Redraw one text row — only writes cells that differ from the shadow.
static void draw_line(int screen_y) {
    char buf[SCREEN_COLS];
    build_line_buf(screen_y, buf);
    for (int x = 0; x < SCREEN_COLS; x++) {
        if (!shadow_valid || line_shadow[screen_y][x] != buf[x]) {
            putchar_at(x, screen_y, buf[x], COLOR_WHITE, COLOR_BLACK);
            line_shadow[screen_y][x] = buf[x];
        }
    }
}

// Build the full status bar content into buf[SCREEN_COLS-1].
static void build_status_buf(const char* message, char buf[SCREEN_COLS]) {
    int x = 0;
    int max = SCREEN_COLS - 1;

    for (const char* v = VERSION; *v && x < max; ) buf[x++] = *v++;
    if (x < max) buf[x++] = ' ';
    if (x < max) buf[x++] = '|';
    if (x < max) buf[x++] = ' ';

    if (filename[0] == '\0') {
        for (const char* u = "[New File]"; *u && x < max; ) buf[x++] = *u++;
    } else {
        for (const char* f = filename; *f && x < max; ) buf[x++] = *f++;
    }
    if (modified && x < max) buf[x++] = '*';

    if (x < max) buf[x++] = ' ';
    if (x < max) buf[x++] = '|';
    if (x < max) buf[x++] = ' ';

    for (const char* s = "Ln:"; *s && x < max; ) buf[x++] = *s++;
    {
        int val = cursor_row + 1;
        char rev[8]; int ri = 0;
        do { rev[ri++] = '0' + (val % 10); val /= 10; } while (val > 0);
        while (ri > 0 && x < max) buf[x++] = rev[--ri];
    }
    if (x < max) buf[x++] = ' ';

    for (const char* s = "Col:"; *s && x < max; ) buf[x++] = *s++;
    {
        int val = cursor_col + 1;
        char rev[8]; int ri = 0;
        do { rev[ri++] = '0' + (val % 10); val /= 10; } while (val > 0);
        while (ri > 0 && x < max) buf[x++] = rev[--ri];
    }

    const char* hint = (message && message[0]) ? message : "^S=Save ^Q=Quit";
    int hint_len = 0;
    for (const char* h = hint; *h; h++) hint_len++;
    int pad = max - x - hint_len;
    for (int i = 0; i < pad && x < max; i++) buf[x++] = ' ';
    while (*hint && x < max) buf[x++] = *hint++;
    while (x < max) buf[x++] = ' ';
}

// Update status bar row — only writes chars that differ from the shadow.
static void update_status(const char* message) {
    char buf[SCREEN_COLS];
    build_status_buf(message, buf);
    for (int x = 0; x < SCREEN_COLS - 1; x++) {
        if (!shadow_valid || status_shadow[x] != buf[x]) {
            putchar_at(x, SCREEN_ROWS, buf[x], COLOR_BLACK, COLOR_WHITE);
            status_shadow[x] = buf[x];
        }
    }
}

// Move the hardware cursor to the editor position.
// This also fires vga_update_cursor(), which causes VirtualBox to snapshot the
// framebuffer (including the already-written status bar) for its cursor-blink cycle.
static void set_edit_cursor(void) {
    set_cursor_pos((unsigned char)(cursor_col - scroll_col),
                   (unsigned char)(cursor_row - scroll_row));
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    build_version(VERSION);

    lines[0][0] = '\0';
    line_count = 1;
    cursor_row = cursor_col = scroll_row = scroll_col = modified = 0;

    if (argc >= 2) {
        int i = 0;
        while (argv[1][i] && i < 63) { filename[i] = argv[1][i]; i++; }
        filename[i] = '\0';
        if (load_file(filename) != 0) { lines[0][0] = '\0'; line_count = 1; }
    }

    // Initial full draw
    for (int y = 0; y < SCREEN_ROWS; y++) draw_line(y);
    update_status("");
    set_edit_cursor();
    shadow_valid = 1;

    char status_msg[80] = "";

    while (1) {
        int c = getchar();
        status_msg[0] = '\0';

        // Capture state before processing so we can detect what changed.
        int old_scroll_row  = scroll_row;
        int old_scroll_col  = scroll_col;
        int old_cursor_row  = cursor_row;  // needed for Enter split point

        // text_change_from: earliest screen row whose text content may have
        // changed.  -1 means no text changed (cursor movement only).
        int text_change_from = -1;

        if (c == 17) {
            // Ctrl+Q — quit
            break;

        } else if (c == 19) {
            // Ctrl+S — save
            if (filename[0] == '\0') {
                const char* m = "No filename! Use: sedit <file>";
                int i = 0; while (m[i]) { status_msg[i] = m[i]; i++; } status_msg[i] = '\0';
            } else {
                const char* m = save_file() == 0 ? "Saved!" : "Save FAILED!";
                int i = 0; while (m[i]) { status_msg[i] = m[i]; i++; } status_msg[i] = '\0';
            }
            // No text_change_from — only the status bar needs updating
            // (modified flag and message may change; shadow handles both).

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
                cursor_row++;
                cursor_col = 0;
                ensure_cursor_visible();
            }

        } else if (c == '\n' || c == '\r') {
            if (line_count < MAX_LINES) {
                // Lines from the split row downward will change.
                text_change_from = old_cursor_row - old_scroll_row;
                for (int i = line_count; i > cursor_row + 1; i--)
                    strcpy(lines[i], lines[i - 1]);
                line_count++;
                strcpy(lines[cursor_row + 1], &lines[cursor_row][cursor_col]);
                lines[cursor_row][cursor_col] = '\0';
                cursor_row++;
                cursor_col = 0;
                modified = 1;
                ensure_cursor_visible();
            }

        } else if (c == '\b' || c == 127) {
            if (cursor_col > 0) {
                // Same-line delete — only the current line changes.
                text_change_from = cursor_row - old_scroll_row;
                int len = strlen(lines[cursor_row]);
                for (int i = cursor_col - 1; i < len; i++)
                    lines[cursor_row][i] = lines[cursor_row][i + 1];
                cursor_col--;
                modified = 1;
                ensure_cursor_visible();
            } else if (cursor_row > 0) {
                // Merge with previous line — that line and all below may change.
                // text_change_from is set before cursor_row is decremented.
                text_change_from = (cursor_row - 1) - old_scroll_row;
                int prev_len = strlen(lines[cursor_row - 1]);
                int cur_len  = strlen(lines[cursor_row]);
                if (prev_len + cur_len < MAX_LINE_LEN) {
                    strcat(lines[cursor_row - 1], lines[cursor_row]);
                    for (int i = cursor_row; i < line_count - 1; i++)
                        strcpy(lines[i], lines[i + 1]);
                    line_count--;
                    lines[line_count][0] = '\0';
                    cursor_row--;
                    cursor_col = prev_len;
                    modified = 1;
                    ensure_cursor_visible();
                }
            }

        } else if (c >= 32 && c <= 126) {
            int len = strlen(lines[cursor_row]);
            if (len < MAX_LINE_LEN - 1) {
                // Only the current line changes.
                text_change_from = cursor_row - old_scroll_row;
                for (int i = len + 1; i > cursor_col; i--)
                    lines[cursor_row][i] = lines[cursor_row][i - 1];
                lines[cursor_row][cursor_col] = (char)c;
                cursor_col++;
                modified = 1;
                ensure_cursor_visible();
            }

        } else {
            continue;  // Unknown key — nothing changed, skip redraw entirely
        }

        // --- Incremental redraw ---
        // Set cursor position first so every vga_update_cursor() call inside
        // putchar_at snapshots the framebuffer with the cursor in the right place.
        set_edit_cursor();

        int scroll_changed = (scroll_row != old_scroll_row ||
                              scroll_col != old_scroll_col);

        if (!shadow_valid || scroll_changed) {
            // Viewport shifted — check every line (shadow skips unchanged ones).
            for (int y = 0; y < SCREEN_ROWS; y++) draw_line(y);
        } else if (text_change_from >= 0) {
            // Redraw from the earliest changed line to the bottom of the screen.
            int start = text_change_from < 0 ? 0 : text_change_from;
            for (int y = start; y < SCREEN_ROWS; y++) draw_line(y);
        }
        // Status bar and cursor are always refreshed (shadow skips unchanged chars).
        update_status(status_msg);
        set_edit_cursor();
        shadow_valid = 1;
    }

    return 0;
}
