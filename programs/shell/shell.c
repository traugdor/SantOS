// SantOS Shell - Simple Command Prompt
// Uses kernel's printf and getchar functions

#include "../../include/stdio.h"
#include "../../include/string.h"
#include "../../include/stdlib.h"

#define MAX_INPUT 256

char* variables[32];
char* values[32];
int variable_count = 0;

char current_directory[256] = "/";  // Current directory path
unsigned short current_cluster = 0;  // Current directory cluster (0 = root)

// Stream buffer for piping between commands (e.g. cat FILE > read > find "text")
#define STREAM_BUF_SIZE 32768
static char* stream_buffer = NULL;   // Shared stream buffer (malloc'd on first use)
static int stream_length = 0;        // Current length of data in stream
static int piping = 0;               // 1 if currently executing a pipeline
static int pipe_remaining = 0;       // How many pipe segments remain after current one
static char find_term[256] = "";     // Active search term for highlighting (set by find, used by read)

// Command history
#define HISTORY_SIZE 10  // Change this to expand history (e.g. 20, 30)
#define CURSOR_LEFT 0x1D  // Non-destructive cursor left (VGA driver moves cursor without erasing)
static char history[HISTORY_SIZE][MAX_INPUT];
static int history_count = 0;   // Total commands stored (up to HISTORY_SIZE)
static int history_write = 0;   // Next write index (ring buffer)

// VGA page dimensions for the read command
#define PAGE_COLS 80
#define PAGE_ROWS 22  // 25 rows minus 3 for status bar

// Block collection state for if/for/while
#define MAX_BLOCK_LINES 8
#define MAX_LINE_LEN 128

// Use pointers - allocate dynamically to avoid large static arrays
char** block_lines = NULL;  // Will be malloc'd
int block_line_count = 0;
int collecting_block = 0;  // 0=none, 1=if, 2=for, 3=while
char* block_header = NULL;  // Will be malloc'd
int block_nesting = 0;

// Forward declarations
int process_command(char* command);
int process_single_command(char* command);
void process_pipeline(char* command_line);
void execute_block(void);
int is_numeric(const char* str);
int str_to_int(const char* str);
char* int_to_str(int val);
const char* resolve_value(const char* token, int* is_alloc);
int evaluate_condition(const char* val1_raw, const char* op, const char* val2_raw);
int cmd_read(void);
void cmd_find(const char* search_term);

void shell_main(void) {
    // Allocate block storage dynamically
    block_lines = (char**)malloc(MAX_BLOCK_LINES * sizeof(char*));
    if (block_lines) {
        for (int i = 0; i < MAX_BLOCK_LINES; i++) {
            block_lines[i] = (char*)malloc(MAX_LINE_LEN);
            if (block_lines[i]) {
                block_lines[i][0] = '\0';
            }
        }
    }
    block_header = (char*)malloc(MAX_LINE_LEN);
    if (block_header) {
        block_header[0] = '\0';
    }
    block_line_count = 0;
    collecting_block = 0;
    block_nesting = 0;
    
    char input[MAX_INPUT];
    int error_code;
    
    printf("\nSantOS Shell v1.0\n");
    printf("Type commands and press Enter\n\n");
    
    while (1) {
        if (collecting_block) {
            printf(".. ");
        } else {
            printf("%s>", current_directory);
        }
        
        // Read input with history and cursor movement
        int len = 0;       // Current length of input
        int cursor = 0;    // Cursor position within input
        int hist_nav = -1; // History navigation index (-1 = not navigating)
        input[0] = '\0';
        
        while (len < MAX_INPUT - 1) {
            char c = getchar();
            
            if (c == '\n') {
                input[len] = '\0';
                printf("\n");
                
                // Add to history if not empty
                if (len > 0) {
                    strcpy(history[history_write], input);
                    history_write = (history_write + 1) % HISTORY_SIZE;
                    if (history_count < HISTORY_SIZE) history_count++;
                }
                
                // Process command if not empty
                if (input[0] != '\0') {
                    // If collecting a block, handle block lines
                    if (collecting_block) {
                        // Skip comments
                        char* trimmed = input;
                        while (*trimmed == ' ') trimmed++;
                        if (trimmed[0] == '#' && trimmed[1] == '#') {
                            break;
                        }
                        
                        // Check for nested block starts
                        if (strcmp(trimmed, "if") == 0 || strncmp(trimmed, "if ", 3) == 0) {
                            block_nesting++;
                        } else if (strncmp(trimmed, "for ", 4) == 0 || strcmp(trimmed, "for") == 0) {
                            block_nesting++;
                        } else if (strncmp(trimmed, "while ", 6) == 0 || strcmp(trimmed, "while") == 0) {
                            block_nesting++;
                        }
                        
                        // Check for end keywords
                        int is_end = 0;
                        if (collecting_block == 1 && strcmp(trimmed, "endif") == 0) {
                            if (block_nesting > 0) block_nesting--;
                            else is_end = 1;
                        } else if (collecting_block == 2 && strcmp(trimmed, "endfor") == 0) {
                            if (block_nesting > 0) block_nesting--;
                            else is_end = 1;
                        } else if (collecting_block == 3 && strcmp(trimmed, "endwhile") == 0) {
                            if (block_nesting > 0) block_nesting--;
                            else is_end = 1;
                        }
                        
                        if (is_end) {
                            execute_block();
                            collecting_block = 0;
                            block_line_count = 0;
                            block_nesting = 0;
                        } else {
                            if (block_line_count < MAX_BLOCK_LINES) {
                                strcpy(block_lines[block_line_count], input);
                                block_line_count++;
                            } else {
                                set_color(COLOR_LIGHT_RED, COLOR_BLACK);
                                printf("Error: Block too large (max %d lines)\n", MAX_BLOCK_LINES);
                                set_color(COLOR_WHITE, COLOR_BLACK);
                                collecting_block = 0;
                                block_line_count = 0;
                                block_nesting = 0;
                            }
                        }
                        break;
                    }
                    
                    error_code = process_command(input);
                    if (error_code == -100) {
                        return;
                    }
                }
                break;
            } else if (c == '\b' || c == 127) {
                // Backspace: delete character before cursor
                if (cursor > 0) {
                    // Shift everything after cursor left by 1
                    for (int j = cursor - 1; j < len - 1; j++) {
                        input[j] = input[j + 1];
                    }
                    len--;
                    cursor--;
                    input[len] = '\0';
                    
                    // Move cursor back
                    putchar('\b');
                    // Reprint from cursor to end, then add space to erase last char
                    for (int j = cursor; j < len; j++) {
                        putchar(input[j]);
                    }
                    putchar(' ');  // Erase the old last character
                    // Move cursor back to correct position
                    for (int j = 0; j < len - cursor + 1; j++) {
                        putchar(CURSOR_LEFT);
                    }
                }
            } else if (c == KEY_LEFT) {
                // Move cursor left (non-destructive)
                if (cursor > 0) {
                    cursor--;
                    putchar(CURSOR_LEFT);
                }
            } else if (c == KEY_RIGHT) {
                // Move cursor right
                if (cursor < len) {
                    putchar(input[cursor]);
                    cursor++;
                }
            } else if (c == KEY_UP) {
                // Navigate history: previous command
                if (history_count > 0) {
                    int idx;
                    if (hist_nav == -1) {
                        // Start navigating from the most recent
                        hist_nav = 0;
                    } else if (hist_nav < history_count - 1) {
                        hist_nav++;
                    } else {
                        continue;  // Already at oldest entry
                    }
                    
                    // Calculate actual index in ring buffer
                    idx = (history_write - 1 - hist_nav + HISTORY_SIZE) % HISTORY_SIZE;
                    
                    // Erase current input on screen
                    // Move cursor to start of input
                    while (cursor > 0) { putchar(CURSOR_LEFT); cursor--; }
                    // Overwrite with spaces
                    for (int j = 0; j < len; j++) putchar(' ');
                    // Move back to start
                    for (int j = 0; j < len; j++) putchar(CURSOR_LEFT);
                    
                    // Load history entry
                    strcpy(input, history[idx]);
                    len = strlen(input);
                    cursor = len;
                    
                    // Print it
                    printf("%s", input);
                }
            } else if (c == KEY_DOWN) {
                // Navigate history: next (more recent) command
                if (hist_nav >= 0) {
                    // Erase current input on screen
                    while (cursor > 0) { putchar(CURSOR_LEFT); cursor--; }
                    for (int j = 0; j < len; j++) putchar(' ');
                    for (int j = 0; j < len; j++) putchar(CURSOR_LEFT);
                    
                    hist_nav--;
                    
                    if (hist_nav < 0) {
                        // Past most recent — clear input
                        input[0] = '\0';
                        len = 0;
                        cursor = 0;
                    } else {
                        int idx = (history_write - 1 - hist_nav + HISTORY_SIZE) % HISTORY_SIZE;
                        strcpy(input, history[idx]);
                        len = strlen(input);
                        cursor = len;
                        printf("%s", input);
                    }
                }
            } else if (c >= 32 && c <= 126) {
                // Printable character: insert at cursor position
                if (len < MAX_INPUT - 1) {
                    // Shift everything after cursor right by 1
                    for (int j = len; j > cursor; j--) {
                        input[j] = input[j - 1];
                    }
                    input[cursor] = c;
                    len++;
                    input[len] = '\0';
                    
                    // Print from cursor to end
                    for (int j = cursor; j < len; j++) {
                        putchar(input[j]);
                    }
                    cursor++;
                    // Move cursor back to correct position
                    for (int j = 0; j < len - cursor; j++) {
                        putchar(CURSOR_LEFT);
                    }
                }
            }
        }
    }
}

// Check if string is numeric (optional leading minus)
int is_numeric(const char* str) {
    if (!str || *str == '\0') return 0;
    int i = 0;
    if (str[0] == '-') i = 1;
    if (str[i] == '\0') return 0;
    for (; str[i]; i++) {
        if (str[i] < '0' || str[i] > '9') return 0;
    }
    return 1;
}

// String to integer
int str_to_int(const char* str) {
    int result = 0;
    int neg = 0;
    int i = 0;
    if (str[0] == '-') { neg = 1; i = 1; }
    for (; str[i]; i++) {
        result = result * 10 + (str[i] - '0');
    }
    return neg ? -result : result;
}

// Integer to string (caller must free)
char* int_to_str(int val) {
    char* buf = (char*)malloc(16);
    if (!buf) return NULL;
    int neg = 0;
    if (val < 0) { neg = 1; val = -val; }
    int i = 0;
    if (val == 0) { buf[i++] = '0'; }
    else {
        while (val > 0) {
            buf[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    if (neg) buf[i++] = '-';
    buf[i] = '\0';
    // Reverse
    for (int j = 0; j < i / 2; j++) {
        char t = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = t;
    }
    return buf;
}

// Resolve a value: if starts with $, look up variable; otherwise return as-is
const char* resolve_value(const char* token, int* is_alloc) {
    *is_alloc = 0;
    if (!token) return "";
    if (token[0] == '$') {
        const char* name = token + 1;
        for (int i = 0; i < variable_count; i++) {
            if (strcmp(variables[i], name) == 0) {
                return values[i];
            }
        }
        return "";  // Variable not found
    }
    return token;
}

// Evaluate a condition: val1 op val2
// Operators: <, >, <=, >=, =, <>
int evaluate_condition(const char* val1_raw, const char* op, const char* val2_raw) {
    int a1 = 0, a2 = 0;
    const char* val1 = resolve_value(val1_raw, &a1);
    const char* val2 = resolve_value(val2_raw, &a2);
    
    if (is_numeric(val1) && is_numeric(val2)) {
        int n1 = str_to_int(val1);
        int n2 = str_to_int(val2);
        if (strcmp(op, "<") == 0) return n1 < n2;
        if (strcmp(op, ">") == 0) return n1 > n2;
        if (strcmp(op, "<=") == 0) return n1 <= n2;
        if (strcmp(op, ">=") == 0) return n1 >= n2;
        if (strcmp(op, "=") == 0) return n1 == n2;
        if (strcmp(op, "<>") == 0) return n1 != n2;
    } else {
        int cmp = strcmp(val1, val2);
        if (strcmp(op, "<") == 0) return cmp < 0;
        if (strcmp(op, ">") == 0) return cmp > 0;
        if (strcmp(op, "<=") == 0) return cmp <= 0;
        if (strcmp(op, ">=") == 0) return cmp >= 0;
        if (strcmp(op, "=") == 0) return cmp == 0;
        if (strcmp(op, "<>") == 0) return cmp != 0;
    }
    return 0;
}

int process_single_command(char* command) {
    // Check for program/script execution (./ prefix)
    if (command[0] == '.' && command[1] == '/') {
        // Parse command into filename and arguments
        // Format: ./PROGRAM arg1 arg2 "arg with spaces" ...
        char* cmd_start = &command[2];
        
        // Split into argv[] (max 16 arguments)
        #define MAX_ARGS 16
        char* argv[MAX_ARGS];
        char arg_buf[MAX_INPUT];  // Storage for parsed argument strings
        int argc = 0;
        int buf_pos = 0;
        
        char* p = cmd_start;
        while (*p && argc < MAX_ARGS) {
            // Skip whitespace
            while (*p == ' ') p++;
            if (!*p) break;
            
            // Start of argument
            argv[argc] = &arg_buf[buf_pos];
            
            if (*p == '"') {
                // Quoted argument - include everything until closing quote
                p++;  // Skip opening quote
                while (*p && *p != '"') {
                    arg_buf[buf_pos++] = *p++;
                }
                if (*p == '"') p++;  // Skip closing quote
            } else {
                // Unquoted argument - until next space
                while (*p && *p != ' ') {
                    arg_buf[buf_pos++] = *p++;
                }
            }
            arg_buf[buf_pos++] = '\0';
            argc++;
        }
        
        if (argc == 0) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: No program specified\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            return -1;
        }
        
        // argv[0] is the filename
        char* filename = argv[0];
        
        // Read first 7 bytes to check for script marker "##/sosh"
        // NOTE: buffer must be >= 512 bytes because fat12_read always copies full sectors
        char header[512];
        int bytes = read_file(filename, header, 7);
        if (bytes <= 0) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: Failed to open %s\n", filename);
            set_color(COLOR_WHITE, COLOR_BLACK);
            return -1;
        }
        header[7] = '\0';
        
        // Check if this is a sosh script
        if (bytes >= 7 && strcmp(header, "##/sosh") == 0) {
            // Read entire script file
            char* script_buf = (char*)malloc(4096);
            if (!script_buf) {
                set_color(COLOR_LIGHT_RED, COLOR_BLACK);
                printf("Error: Memory allocation failed\n");
                set_color(COLOR_WHITE, COLOR_BLACK);
                return -1;
            }
            
            int script_len = read_file(filename, script_buf, 4095);
            if (script_len <= 0) {
                free(script_buf);
                set_color(COLOR_LIGHT_RED, COLOR_BLACK);
                printf("Error: Failed to read script %s\n", filename);
                set_color(COLOR_WHITE, COLOR_BLACK);
                return -1;
            }
            script_buf[script_len] = '\0';
            
            // Skip first line (##/sosh header)
            char* line_start = script_buf;
            while (*line_start && *line_start != '\n') {
                line_start++;
            }
            if (*line_start == '\n') line_start++;  // Skip the newline
            
            // Execute each line
            while (*line_start) {
                // Find end of line
                char* line_end = line_start;
                while (*line_end && *line_end != '\n' && *line_end != '\r') {
                    line_end++;
                }
                
                // Extract line into temp buffer
                int line_len = (int)(line_end - line_start);
                if (line_len > 0) {
                    char* line = (char*)malloc(line_len + 1);
                    if (line) {
                        memcpy(line, line_start, line_len);
                        line[line_len] = '\0';
                        
                        // Skip empty lines and comments
                        if (line[0] != '\0') {
                            process_command(line);
                        }
                        free(line);
                    }
                }
                
                // Advance past newline(s)
                line_start = line_end;
                while (*line_start == '\n' || *line_start == '\r') {
                    line_start++;
                }
            }
            
            free(script_buf);
            return 0;
        }
        
        // Not a script - try executing as ELF program with arguments
        int result = exec_program(filename, argc, argv);
        
        if (result != 0) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: Failed to execute %s\n", filename);
            set_color(COLOR_WHITE, COLOR_BLACK);
        }
        
        return result;
    }
    
    char* first_char = substr(command, 1, 1);  // 1-based indexing
    if (first_char && strcmp(first_char, "$") == 0) {
        char* env_var = substr(command, 2, 0);  // Skip the '$'
        
        // Determine action: = or +=
        int is_append = 0;
        for (int i = 0; i < strlen(env_var); i++) {
            if (env_var[i] == '+' && env_var[i+1] == '=') {
                is_append = 1;
                break;
            }
        }
        
        // Find the = sign (works for both = and +=)
        char* equals_pos = NULL;
        for (int i = 0; i < strlen(env_var); i++) {
            if (env_var[i] == '=') {
                equals_pos = &env_var[i];
                break;
            }
        }
        
        if (!equals_pos) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: Invalid variable assignment syntax\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(env_var);
            free(first_char);
            return 0;
        }
        
        // Split at the = sign
        *equals_pos = '\0';
        char* varname = env_var;
        char* varval_raw = equals_pos + 1;
        
        // Remove trailing '+' from varname if append operation
        if (is_append && varname) {
            size_t len = strlen(varname);
            if (len > 0 && varname[len-1] == '+') {
                varname[len-1] = '\0';
            }
        }
        
        // Validate varname
        if (!varname || strlen(varname) == 0) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: Invalid variable assignment syntax\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(env_var);
            free(first_char);
            return 0;
        }
        
        // Process varval: handle quotes and variable expansion
        char* varval = (char*)malloc(1024);  // Allocate buffer for processed value
        if (!varval) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: Memory allocation failed\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(env_var);
            free(first_char);
            return 0;
        }
        varval[0] = '\0';
        
        int i = 0;
        while (varval_raw[i]) {
            if (varval_raw[i] == '"') {
                // Start of quoted string - copy until closing quote
                i++;
                while (varval_raw[i] && varval_raw[i] != '"') {
                    int len = strlen(varval);
                    varval[len] = varval_raw[i];
                    varval[len + 1] = '\0';
                    i++;
                }
                if (varval_raw[i] == '"') i++;  // Skip closing quote
            } else if (varval_raw[i] == '$') {
                // Variable reference - expand it
                i++;
                int var_start = i;
                while (varval_raw[i] && 
                       ((varval_raw[i] >= 'a' && varval_raw[i] <= 'z') ||
                        (varval_raw[i] >= 'A' && varval_raw[i] <= 'Z') ||
                        (varval_raw[i] >= '0' && varval_raw[i] <= '9') ||
                        varval_raw[i] == '_')) {
                    i++;
                }
                
                if (i > var_start) {
                    // Extract variable name
                    char var_name[64];
                    int name_len = i - var_start;
                    if (name_len >= 64) name_len = 63;
                    for (int j = 0; j < name_len; j++) {
                        var_name[j] = varval_raw[var_start + j];
                    }
                    var_name[name_len] = '\0';
                    
                    // Look up variable value
                    for (int j = 0; j < variable_count; j++) {
                        if (strcmp(variables[j], var_name) == 0) {
                            strcat(varval, values[j]);
                            break;
                        }
                    }
                }
            } else {
                // Regular character
                int len = strlen(varval);
                varval[len] = varval_raw[i];
                varval[len + 1] = '\0';
                i++;
            }
        }
        
        // Validate final value
        if (strlen(varval) == 0 && strlen(varval_raw) > 0) {
            // Empty value is ok if explicitly set
        }
        
        // Find variable in array
        int variableindex = -1;
        for (int i = 0; i < variable_count; i++) {
            if (strcmp(variables[i], varname) == 0) {
                variableindex = i;
                break;
            }
        }
        
        if (variableindex == -1) {
            // Variable not found, add new variable
            if (variable_count >= 32) {
                set_color(COLOR_LIGHT_RED, COLOR_BLACK);
                printf("Error: Maximum number of variables reached\n");
                set_color(COLOR_WHITE, COLOR_BLACK);
            } else {
                variables[variable_count] = strdup(varname);
                values[variable_count] = strdup(varval);
                variable_count++;
                //printf("Variable set: %s = %s\n", varname, varval);
            }
        } else {
            // Variable exists
            if (is_append) {
                // Append to existing value
                size_t old_len = strlen(values[variableindex]);
                size_t new_len = strlen(varval);
                char* new_value = (char*)malloc(old_len + new_len + 1);
                if (new_value) {
                    strcpy(new_value, values[variableindex]);
                    strcat(new_value, varval);
                    free(values[variableindex]);
                    values[variableindex] = new_value;
                    //printf("Variable appended: %s = %s\n", varname, new_value);
                }
            } else {
                // Replace value
                free(values[variableindex]);
                values[variableindex] = strdup(varval);
                //printf("Variable set: %s = %s\n", varname, varval);
            }
        }
        
        free(varval);
        free(env_var);
        free(first_char);
        return 0;
    }
    free(first_char);
    
    // Parse command into tokens (strtok modifies the string, so we need a copy)
    char* command_copy = strdup(command);
    char* args[32];  // Max 32 arguments (including command name)
    int argc = 0;
    
    char* token = strtok(command_copy, " ");
    while (token != NULL && argc < 32) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }
    
    if (argc == 0) {
        free(command_copy);
        return 0;  // Empty command
    }
    
    char* command_name = args[0];
    
    // Handle built-in commands
    if (strcmp(command_name, "help") == 0) {
        printf("Available commands:\n");
        printf("  add      - Add to numeric variable (add $VAR value)\n");
        printf("  cat      - Print file contents (cat <file>)\n");
        printf("  cd       - Change directory (cd ., cd .., cd <dir>)\n");
        printf("  clear    - Clear the screen\n");
        printf("  dir      - List directory contents (alias for ls)\n");
        printf("  echo     - Echo text with variable expansion ($VAR)\n");
        printf("  exit     - Exit the shell\n");
        printf("  find     - Highlight text in stream (> find \"text\")\n");
        printf("  help     - Show this help message\n");
        printf("  listvars - List all defined variables\n");
        printf("  ls       - List directory contents\n");
        printf("  read     - Paginated text viewer (> read)\n");
        printf("  sub      - Subtract from numeric variable (sub $VAR value)\n");
        printf("  touch    - Create an empty file (touch <file>)\n");
        printf("  unset    - Unset a variable (unset $VAR, unset allvars)\n");
        printf("\nPiping: cmd1 > cmd2 > cmd3\n");
        printf("  Example: cat FILE > read > find \"text\"\n");
        free(command_copy);
        return 0;
    }
    
    if (strcmp(command_name, "clear") == 0) {
        clear_screen();
        free(command_copy);
        return 0;
    }

    if (strcmp(command_name, "exit") == 0) {
        printf("Exiting shell...\n");
        free(command_copy);
        return -100;  // Signal to exit
    }

    //cd - change directory
    if (strcmp(command_name, "cd") == 0) {
        if (argc < 2) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Usage: cd <directory>\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        
        char* target = args[1];
        
        if (strcmp(target, ".") == 0) {
            // Stay in current directory
            printf("%s\n", current_directory);
        } else if (strcmp(target, "..") == 0) {
            // Go up one directory
            if (strcmp(current_directory, "/") == 0) {
                set_color(COLOR_LIGHT_RED, COLOR_BLACK);
                printf("Already at root directory\n");
                set_color(COLOR_WHITE, COLOR_BLACK);
            } else {
                // Use FAT12's ".." entry to find parent cluster
                int is_dir = 0;
                unsigned short parent_cluster = find_entry(current_cluster, "..", &is_dir);
                
                // Find last slash and truncate path
                int len = strlen(current_directory);
                int last_slash = -1;
                for (int i = len - 1; i >= 0; i--) {
                    if (current_directory[i] == '/') {
                        last_slash = i;
                        break;
                    }
                }
                
                if (last_slash == 0) {
                    current_directory[1] = '\0';  // Back to root "/"
                    current_cluster = 0;  // Root cluster
                } else if (last_slash > 0) {
                    current_directory[last_slash] = '\0';
                    current_cluster = parent_cluster;
                }
                printf("%s\n", current_directory);
            }
        } else {
            // Enter directory - validate it exists
            int is_dir = 0;
            unsigned short new_cluster = find_entry(current_cluster, target, &is_dir);
            
            if (new_cluster == 0) {
                set_color(COLOR_LIGHT_RED, COLOR_BLACK);
                printf("Directory not found: %s\n", target);
                set_color(COLOR_WHITE, COLOR_BLACK);
            } else if (!is_dir) {
                set_color(COLOR_LIGHT_RED, COLOR_BLACK);
                printf("Not a directory: %s\n", target);
                set_color(COLOR_WHITE, COLOR_BLACK);
            } else {
                // Valid directory, update path and cluster
                if (strcmp(current_directory, "/") == 0) {
                    strcpy(current_directory, "/");
                    strcat(current_directory, target);
                } else {
                    strcat(current_directory, "/");
                    strcat(current_directory, target);
                }
                current_cluster = new_cluster;
                printf("%s\n", current_directory);
            }
        }
        
        free(command_copy);
        return 0;
    }
    
    //listvars - list all variables
    if (strcmp(command_name, "listvars") == 0) {
        if (variable_count == 0) {
            printf("No variables defined\n");
        } else {
            // Find longest variable name
            int max_len = 0;
            for (int i = 0; i < variable_count; i++) {
                int len = strlen(variables[i]);
                if (len > max_len) {
                    max_len = len;
                }
            }
            
            // Print variables with aligned values
            for (int i = 0; i < variable_count; i++) {
                int padding = max_len - strlen(variables[i]);
                printf("%s", variables[i]);
                for (int j = 0; j < padding; j++) {
                    printf(" ");
                }
                printf(" = %s\n", values[i]);
            }
        }
        free(command_copy);
        return 0;
    }
    
    //ls or dir - list directory contents
    if (strcmp(command_name, "ls") == 0 || strcmp(command_name, "dir") == 0) {
        list_dir_cluster(current_cluster);
        free(command_copy);
        return 0;
    }

    //echo - print literally what was typed after white space and all, even if white space is long
    if (strcmp(command_name, "echo") == 0) {
        // Build output into a temporary buffer (for piping support)
        char* out_buf = (char*)malloc(4096);
        int out_len = 0;
        if (!out_buf) {
            printf("\n");
            free(command_copy);
            return 0;
        }
        out_buf[0] = '\0';
        
        char* result = substr(command, 6, 0);
        if (result) {
            // Process string for variable expansion
            for (int i = 0; i < strlen(result); i++) {
                if (result[i] == '$') {
                    // Found variable reference, extract variable name
                    int var_start = i + 1;
                    int var_end = var_start;
                    while (var_end < strlen(result) && 
                           ((result[var_end] >= 'a' && result[var_end] <= 'z') ||
                            (result[var_end] >= 'A' && result[var_end] <= 'Z') ||
                            (result[var_end] >= '0' && result[var_end] <= '9') ||
                            result[var_end] == '_')) {
                        var_end++;
                    }
                    
                    if (var_end > var_start) {
                        char* var_name = substr(result, var_start + 1, var_end);
                        if (var_name) {
                            int found = 0;
                            for (int j = 0; j < variable_count; j++) {
                                if (strcmp(variables[j], var_name) == 0) {
                                    int vlen = strlen(values[j]);
                                    if (out_len + vlen < 4095) {
                                        memcpy(out_buf + out_len, values[j], vlen);
                                        out_len += vlen;
                                    }
                                    found = 1;
                                    break;
                                }
                            }
                            free(var_name);
                            i = var_end - 1;
                        }
                    } else {
                        if (out_len < 4095) out_buf[out_len++] = '$';
                    }
                } else {
                    if (out_len < 4095) out_buf[out_len++] = result[i];
                }
            }
            free(result);
        }
        out_buf[out_len] = '\0';
        
        if (piping) {
            // Store in stream buffer for next command in pipeline
            if (!stream_buffer) {
                stream_buffer = (char*)malloc(STREAM_BUF_SIZE);
            }
            if (stream_buffer) {
                int copy_len = out_len < STREAM_BUF_SIZE - 1 ? out_len : STREAM_BUF_SIZE - 1;
                memcpy(stream_buffer, out_buf, copy_len);
                stream_buffer[copy_len] = '\0';
                stream_length = copy_len;
            }
        } else {
            printf("%s\n", out_buf);
        }
        
        free(out_buf);
        free(command_copy);
        return 0;
    }

    //unset - unsets variables
    //  allvars -- all variables are unset
    //  $VARNAME -- only unsets VARNAME
    //              this means erase the value of it and shift everything over to fill the gap
    if (strcmp(command_name, "unset") == 0) {
        if (argc < 2) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Usage: unset <varname|allvars>\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        
        // Check if "allvars" is specified
        if (strcmp(args[1], "allvars") == 0) {
            // Clear all variables
            for (int i = 0; i < variable_count; i++) {
                free(variables[i]);
                free(values[i]);
            }
            variable_count = 0;
            printf("All variables cleared\n");
        } else {
            // Clear specific variable
            char* var_name = args[1];
            // Remove $ prefix if present
            if (var_name[0] == '$') {
                var_name++;
            }
            // Find and remove the variable
            int found = 0;
            for (int i = 0; i < variable_count; i++) {
                if (strcmp(variables[i], var_name) == 0) {
                    // Free the variable and its value
                    free(variables[i]);
                    free(values[i]);
                    // Shift everything down
                    for (int j = i; j < variable_count - 1; j++) {
                        variables[j] = variables[j + 1];
                        values[j] = values[j + 1];
                    }
                    variable_count--;
                    printf("Variable %s cleared\n", var_name);
                    found = 1;
                    break;
                }
            }
            if (!found) {
                set_color(COLOR_LIGHT_RED, COLOR_BLACK);
                printf("Variable %s not found\n", var_name);
                set_color(COLOR_WHITE, COLOR_BLACK);
            }
        }
        free(command_copy);
        return 0;
    }
    
    // ## comment - ignore
    if (command_name[0] == '#' && command_name[1] == '#') {
        free(command_copy);
        return 0;
    }

    //add - add a numeric value to a variable
    // Syntax: add $VAR value  OR  add $VAR $VAR2
    if (strcmp(command_name, "add") == 0) {
        if (argc < 3) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Usage: add $VAR value\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        
        char* var_token = args[1];
        char* val_token = args[2];
        
        // Variable must start with $
        if (var_token[0] != '$') {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: First argument must be a variable ($VAR)\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        
        char* var_name = var_token + 1;
        
        // Find the variable
        int idx = -1;
        for (int i = 0; i < variable_count; i++) {
            if (strcmp(variables[i], var_name) == 0) {
                idx = i;
                break;
            }
        }
        
        if (idx == -1) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: Variable %s not found\n", var_name);
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        
        // Check variable is numeric
        if (!is_numeric(values[idx])) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: Variable %s is not numeric (%s)\n", var_name, values[idx]);
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        
        // Resolve the value to add
        int dummy = 0;
        const char* add_str = resolve_value(val_token, &dummy);
        if (!is_numeric(add_str)) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: Value is not numeric (%s)\n", add_str);
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        
        int result = str_to_int(values[idx]) + str_to_int(add_str);
        char* result_str = int_to_str(result);
        free(values[idx]);
        values[idx] = result_str;
        //printf("%s = %s\n", var_name, result_str);
        
        free(command_copy);
        return 0;
    }

    //sub - subtract a numeric value from a variable
    // Syntax: sub $VAR value  OR  sub $VAR $VAR2
    if (strcmp(command_name, "sub") == 0) {
        if (argc < 3) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Usage: sub $VAR value\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        
        char* var_token = args[1];
        char* val_token = args[2];
        
        // Variable must start with $
        if (var_token[0] != '$') {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: First argument must be a variable ($VAR)\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        
        char* var_name = var_token + 1;
        
        // Find the variable
        int idx = -1;
        for (int i = 0; i < variable_count; i++) {
            if (strcmp(variables[i], var_name) == 0) {
                idx = i;
                break;
            }
        }
        
        if (idx == -1) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: Variable %s not found\n", var_name);
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        
        // Check variable is numeric
        if (!is_numeric(values[idx])) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: Variable %s is not numeric (%s)\n", var_name, values[idx]);
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        
        // Resolve the value to subtract
        int dummy = 0;
        const char* sub_str = resolve_value(val_token, &dummy);
        if (!is_numeric(sub_str)) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: Value is not numeric (%s)\n", sub_str);
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        
        int result = str_to_int(values[idx]) - str_to_int(sub_str);
        char* result_str = int_to_str(result);
        free(values[idx]);
        values[idx] = result_str;
        //printf("%s = %s\n", var_name, result_str);
        
        free(command_copy);
        return 0;
    }

    //if - start collecting an if block
    // Syntax: if val1 op val2
    if (strcmp(command_name, "if") == 0) {
        if (argc < 4) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Usage: if <val1> <op> <val2>\n");
            printf("Operators: <, >, <=, >=, =, <>\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        strcpy(block_header, command_copy);
        collecting_block = 1;
        block_line_count = 0;
        block_nesting = 0;
        free(command_copy);
        return 0;
    }

    //for - start collecting a for block
    // Syntax: for <start> to <end>
    if (strcmp(command_name, "for") == 0) {
        if (argc < 4) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Usage: for <start> to <end>\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        if (strcmp(args[2], "to") != 0) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Usage: for <start> to <end>\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        strcpy(block_header, command_copy);
        collecting_block = 2;
        block_line_count = 0;
        block_nesting = 0;
        free(command_copy);
        return 0;
    }

    //while - start collecting a while block
    // Syntax: while val1 op val2
    if (strcmp(command_name, "while") == 0) {
        if (argc < 4) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Usage: while <val1> <op> <val2>\n");
            printf("Operators: <, >, <=, >=, =, <>\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        strcpy(block_header, command_copy);
        collecting_block = 3;
        block_line_count = 0;
        block_nesting = 0;
        free(command_copy);
        return 0;
    }

    // touch - create an empty file
    if (strcmp(command_name, "touch") == 0) {
        if (argc < 2) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Usage: touch <filename>\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        int result = create_file(args[1]);
        if (result != 0) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: Failed to create file %s\n", args[1]);
            set_color(COLOR_WHITE, COLOR_BLACK);
        }
        free(command_copy);
        return 0;
    }

    // cat - read file and print to screen, or put into stream buffer if piped
    if (strcmp(command_name, "cat") == 0) {
        if (argc < 2) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Usage: cat <filename>\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        
        // Allocate stream buffer if not already
        if (!stream_buffer) {
            stream_buffer = (char*)malloc(STREAM_BUF_SIZE);
            if (!stream_buffer) {
                set_color(COLOR_LIGHT_RED, COLOR_BLACK);
                printf("Error: Memory allocation failed\n");
                set_color(COLOR_WHITE, COLOR_BLACK);
                free(command_copy);
                return 0;
            }
        }
        
        int bytes = read_file(args[1], stream_buffer, STREAM_BUF_SIZE - 1);
        if (bytes < 0) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: Failed to read file %s\n", args[1]);
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        stream_buffer[bytes] = '\0';
        stream_length = bytes;
        
        // Only print to screen if not in a pipeline
        if (!piping) {
            printf("%s", stream_buffer);
            if (bytes > 0 && stream_buffer[bytes - 1] != '\n') {
                printf("\n");
            }
        }
        
        free(command_copy);
        return 0;
    }

    // read - paginated text viewer using stream buffer
    if (strcmp(command_name, "read") == 0) {
        int rr = cmd_read();
        free(command_copy);
        return rr;
    }

    // find - highlight matching text in stream buffer
    if (strcmp(command_name, "find") == 0) {
        if (argc < 2) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Usage: find \"text to find\"\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            free(command_copy);
            return 0;
        }
        
        // Extract search term - handle quoted strings
        // Find the first quote after "find "
        char* search_start = command + 5;  // Skip "find "
        while (*search_start == ' ') search_start++;
        
        char search_term[256];
        if (*search_start == '"') {
            search_start++;
            int si = 0;
            while (*search_start && *search_start != '"' && si < 255) {
                search_term[si++] = *search_start++;
            }
            search_term[si] = '\0';
        } else {
            // No quotes - use first token
            int si = 0;
            while (*search_start && *search_start != ' ' && si < 255) {
                search_term[si++] = *search_start++;
            }
            search_term[si] = '\0';
        }
        
        cmd_find(search_term);
        free(command_copy);
        return 0;
    }

    // Try to execute as a program on disk (e.g., "sedit" -> "SEDIT" or "SEDIT.ELF")
    {
        // Build uppercased command name
        char elf_name[256];
        int ni = 0;
        while (command_name[ni] && ni < 248) {
            char c = command_name[ni];
            if (c >= 'a' && c <= 'z') c -= 32;  // uppercase
            elf_name[ni] = c;
            ni++;
        }
        elf_name[ni] = '\0';

        // Check if the file exists by trying to read its header
        // Try bare name first (e.g., "SEDIT"), then with ".ELF" appended
        // NOTE: probe must be >= 512 bytes because fat12_read/fdc_read_sectors
        // always copies full sectors even for small reads
        char probe[512];
        int found = 0;

        if (read_file(elf_name, probe, 4) > 0) {
            found = 1;
        } else {
            strcat(elf_name, ".ELF");
            if (read_file(elf_name, probe, 4) > 0) {
                found = 1;
            }
        }

        if (found) {
            // Set args[0] to the resolved filename (e.g. "SEDIT" instead of "sedit")
            args[0] = elf_name;
            int result = exec_program(elf_name, argc, args);
            if (result != 0) {
                set_color(COLOR_LIGHT_RED, COLOR_BLACK);
                printf("Error: Failed to execute %s\n", elf_name);
                set_color(COLOR_WHITE, COLOR_BLACK);
            }
            free(command_copy);
            return result;
        }
    }

    set_color(COLOR_LIGHT_RED, COLOR_BLACK);
    printf("Unknown command: %s", command_name);
    set_color(COLOR_WHITE, COLOR_BLACK);
    int msg_len = 17 + strlen(command_name);
    for (int i = msg_len; i < 80; i++) {
        putchar(' ');
    }
    printf("\n");
    free(command_copy);
    return 0;
}

// Execute a collected block (if/for/while)
void execute_block(void) {
    // Parse the block header
    char header_copy[MAX_LINE_LEN];
    strcpy(header_copy, block_header);
    
    char* h_args[8];
    int h_argc = 0;
    char* tok = strtok(header_copy, " ");
    while (tok && h_argc < 8) {
        h_args[h_argc++] = tok;
        tok = strtok(NULL, " ");
    }
    
    if (h_argc < 4) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        printf("Error: Malformed block header\n");
        set_color(COLOR_WHITE, COLOR_BLACK);
        return;
    }
    
    char* block_type = h_args[0];
    
    if (strcmp(block_type, "if") == 0) {
        // if val1 op val2
        char* val1 = h_args[1];
        char* op = h_args[2];
        char* val2 = h_args[3];
        
        if (evaluate_condition(val1, op, val2)) {
            // Condition true - execute all lines in block
            for (int i = 0; i < block_line_count; i++) {
                // Skip comments
                char* line = block_lines[i];
                while (*line == ' ') line++;
                if (line[0] == '#' && line[1] == '#') continue;
                
                char* cmd_copy = strdup(block_lines[i]);
                if (cmd_copy) {
                    process_command(cmd_copy);
                    free(cmd_copy);
                }
            }
        }
        
    } else if (strcmp(block_type, "for") == 0) {
        // for start to end
        char* start_token = h_args[1];
        // h_args[2] is "to"
        char* end_token = h_args[3];
        
        // Resolve start and end values
        int dummy = 0;
        const char* start_str = resolve_value(start_token, &dummy);
        const char* end_str = resolve_value(end_token, &dummy);
        
        if (!is_numeric(start_str) || !is_numeric(end_str)) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: for loop requires numeric start and end values\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            return;
        }
        
        int start_val = str_to_int(start_str);
        int end_val = str_to_int(end_str);
        
        // Execute the block for each iteration
        if (start_val <= end_val) {
            for (int iter = start_val; iter <= end_val; iter++) {
                for (int i = 0; i < block_line_count; i++) {
                    // Skip comments
                    char* line = block_lines[i];
                    while (*line == ' ') line++;
                    if (line[0] == '#' && line[1] == '#') continue;
                    
                    char* cmd_copy = strdup(block_lines[i]);
                    if (cmd_copy) {
                        process_command(cmd_copy);
                        free(cmd_copy);
                    }
                }
            }
        } else {
            // Count down
            for (int iter = start_val; iter >= end_val; iter--) {
                for (int i = 0; i < block_line_count; i++) {
                    // Skip comments
                    char* line = block_lines[i];
                    while (*line == ' ') line++;
                    if (line[0] == '#' && line[1] == '#') continue;
                    
                    char* cmd_copy = strdup(block_lines[i]);
                    if (cmd_copy) {
                        process_command(cmd_copy);
                        free(cmd_copy);
                    }
                }
            }
        }
        
    } else if (strcmp(block_type, "while") == 0) {
        // while val1 op val2
        // Re-evaluate each iteration since variables may change
        int max_iterations = 10000;  // Safety limit
        int iterations = 0;
        
        while (iterations < max_iterations) {
            // Re-parse header each time to pick up variable changes
            char h_copy[MAX_LINE_LEN];
            strcpy(h_copy, block_header);
            
            char* w_args[8];
            int w_argc = 0;
            char* w_tok = strtok(h_copy, " ");
            while (w_tok && w_argc < 8) {
                w_args[w_argc++] = w_tok;
                w_tok = strtok(NULL, " ");
            }
            
            if (w_argc < 4) break;
            
            char* val1 = w_args[1];
            char* op = w_args[2];
            char* val2 = w_args[3];
            
            if (!evaluate_condition(val1, op, val2)) break;
            
            // Execute all lines in block
            for (int i = 0; i < block_line_count; i++) {
                // Skip comments
                char* line = block_lines[i];
                while (*line == ' ') line++;
                if (line[0] == '#' && line[1] == '#') continue;
                
                char* cmd_copy = strdup(block_lines[i]);
                if (cmd_copy) {
                    process_command(cmd_copy);
                    free(cmd_copy);
                }
            }
            
            iterations++;
        }
        
        if (iterations >= max_iterations) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: while loop exceeded maximum iterations (%d)\n", max_iterations);
            set_color(COLOR_WHITE, COLOR_BLACK);
        }
    }
}

// Pipeline-aware process_command
// Splits command line on " > " and chains commands via stream_buffer
// cat FILE > read > find "text"
int process_command(char* command) {
    // Check if there's a pipe in the command
    char* pipe_pos = NULL;
    char* scan = command;
    while (*scan) {
        if (*scan == ' ' && *(scan+1) == '>' && *(scan+2) == ' ') {
            pipe_pos = scan;
            break;
        }
        scan++;
    }
    
    if (!pipe_pos) {
        // No pipe — just run the single command
        return process_single_command(command);
    }
    
    // Allocate stream buffer if needed
    if (!stream_buffer) {
        stream_buffer = (char*)malloc(STREAM_BUF_SIZE);
        if (!stream_buffer) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            printf("Error: Memory allocation failed for stream\n");
            set_color(COLOR_WHITE, COLOR_BLACK);
            return -1;
        }
    }
    stream_length = 0;
    stream_buffer[0] = '\0';
    find_term[0] = '\0';
    piping = 1;
    
    // Make a working copy of the full command line
    char* pipeline = strdup(command);
    if (!pipeline) { piping = 0; return -1; }
    
    // Split into segments on " > "
    char* segments[16];
    int seg_count = 0;
    char* seg_start = pipeline;
    char* p = pipeline;
    
    while (*p && seg_count < 16) {
        if (*p == ' ' && *(p+1) == '>' && *(p+2) == ' ') {
            *p = '\0';
            segments[seg_count++] = seg_start;
            seg_start = p + 3;
            p = seg_start;
        } else {
            p++;
        }
    }
    if (*seg_start && seg_count < 16) {
        segments[seg_count++] = seg_start;
    }
    
    // Pre-scan: if any segment is a "find" command, extract the search term now
    // so that read can highlight matches regardless of command order
    int find_seg_index = -1;
    for (int i = 0; i < seg_count; i++) {
        char* seg = segments[i];
        while (*seg == ' ') seg++;
        if (strncmp(seg, "find ", 5) == 0) {
            find_seg_index = i;
            // Extract search term from this segment
            char* fs = seg + 5;
            while (*fs == ' ') fs++;
            int fi = 0;
            if (*fs == '"') {
                fs++;
                while (*fs && *fs != '"' && fi < 255) find_term[fi++] = *fs++;
            } else {
                while (*fs && *fs != ' ' && fi < 255) find_term[fi++] = *fs++;
            }
            find_term[fi] = '\0';
            break;
        }
    }
    
    // Execute each segment in order
    int result = 0;
    
    for (int i = 0; i < seg_count; i++) {
        // Trim leading/trailing spaces
        char* seg = segments[i];
        while (*seg == ' ') seg++;
        int len = strlen(seg);
        while (len > 0 && seg[len-1] == ' ') seg[--len] = '\0';
        if (strlen(seg) == 0) continue;
        
        // Tell commands how many segments remain after this one
        pipe_remaining = seg_count - i - 1;
        
        // Skip find if it was pre-processed AND read is also in the pipeline
        // (read will handle highlighting via find_term)
        if (i == find_seg_index && find_seg_index != seg_count - 1) {
            continue;  // find was pre-scanned, read will use find_term
        }
        
        // Run the segment as a normal command
        result = process_single_command(seg);
        if (result == -100 || result == -200) break;
    }
    
    piping = 0;
    free(pipeline);
    return result;
}

// cmd_read - Paginated text viewer
// Reads from stream_buffer and displays one page at a time
// LEFT arrow = next page, RIGHT arrow = previous page, ESC/Q = quit
// If find_term is set (by a preceding find command), highlights matches
int cmd_read(void) {
    if (!stream_buffer || stream_length == 0) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        printf("Error: No data in stream. Use cat <file> > read\n");
        set_color(COLOR_WHITE, COLOR_BLACK);
        return 0;
    }
    
    // Calculate page boundaries
    // Each page = PAGE_ROWS lines of up to PAGE_COLS characters
    int page_starts[1024];  // Max 1024 pages
    int total_pages = 0;
    
    page_starts[0] = 0;
    total_pages = 1;
    
    int col = 0;
    int row = 0;
    
    for (int i = 0; i < stream_length && total_pages < 1024; i++) {
        if (stream_buffer[i] == '\n') {
            col = 0;
            row++;
        } else {
            col++;
            if (col >= PAGE_COLS) {
                col = 0;
                row++;
            }
        }
        
        if (row >= PAGE_ROWS) {
            page_starts[total_pages++] = i + 1;
            row = 0;
            col = 0;
        }
    }
    
    // Display pages interactively
    int current_page = 0;
    
    while (1) {
        clear_screen();
        
        // Print current page content
        int start = page_starts[current_page];
        int end;
        if (current_page + 1 < total_pages) {
            end = page_starts[current_page + 1];
        } else {
            end = stream_length;
        }
        
        // Print page content (with optional highlighting from find)
        int ft_len = strlen(find_term);
        for (int i = start; i < end; i++) {
            if (ft_len > 0 && i + ft_len <= stream_length) {
                int match = 1;
                for (int j = 0; j < ft_len; j++) {
                    if (stream_buffer[i + j] != find_term[j]) {
                        match = 0;
                        break;
                    }
                }
                if (match) {
                    set_color(COLOR_BLACK, COLOR_YELLOW);
                    for (int j = 0; j < ft_len && (i + j) < end; j++) {
                        putchar(stream_buffer[i + j]);
                    }
                    set_color(COLOR_WHITE, COLOR_BLACK);
                    i += ft_len - 1;
                    continue;
                }
            }
            putchar(stream_buffer[i]);
        }
        
        // Print status bar on last row
        set_color(COLOR_BLACK, COLOR_LIGHT_GRAY);
        printf("\n");
        char status[81];
        int slen = 0;
        
        // Left side: navigation info
        char* nav_left = " [LEFT] Prev";
        char* nav_right = " [RIGHT] Next";
        char* nav_quit = " [Q] Quit";
        
        for (int i = 0; nav_left[i] && slen < 80; i++) status[slen++] = nav_left[i];
        for (int i = 0; nav_right[i] && slen < 80; i++) status[slen++] = nav_right[i];
        for (int i = 0; nav_quit[i] && slen < 80; i++) status[slen++] = nav_quit[i];
        
        // Right side: page number
        char* pg = " Page ";
        for (int i = 0; pg[i] && slen < 70; i++) status[slen++] = pg[i];
        
        // Convert page numbers to string
        char num1[8], num2[8];
        int n = current_page + 1;
        int ni = 0;
        if (n == 0) { num1[ni++] = '0'; }
        else {
            char tmp[8]; int ti = 0;
            while (n > 0) { tmp[ti++] = '0' + (n % 10); n /= 10; }
            for (int i = ti - 1; i >= 0; i--) num1[ni++] = tmp[i];
        }
        num1[ni] = '\0';
        
        n = total_pages;
        ni = 0;
        if (n == 0) { num2[ni++] = '0'; }
        else {
            char tmp[8]; int ti = 0;
            while (n > 0) { tmp[ti++] = '0' + (n % 10); n /= 10; }
            for (int i = ti - 1; i >= 0; i--) num2[ni++] = tmp[i];
        }
        num2[ni] = '\0';
        
        for (int i = 0; num1[i] && slen < 78; i++) status[slen++] = num1[i];
        status[slen++] = '/';
        for (int i = 0; num2[i] && slen < 80; i++) status[slen++] = num2[i];
        
        // Show find term if active
        if (ft_len > 0) {
            char* fi = "  Find:\"";
            for (int i = 0; fi[i] && slen < 76; i++) status[slen++] = fi[i];
            for (int i = 0; find_term[i] && slen < 78; i++) status[slen++] = find_term[i];
            if (slen < 79) status[slen++] = '"';
        }
        
        // Pad with spaces
        while (slen < 80) status[slen++] = ' ';
        status[80] = '\0';
        
        printf("%s", status);
        set_color(COLOR_WHITE, COLOR_BLACK);
        
        // Wait for key
        char key = getchar();
        
        if (key == KEY_RIGHT) {
            if (current_page < total_pages - 1) {
                current_page++;
            }
        } else if (key == KEY_LEFT) {
            if (current_page > 0) {
                current_page--;
            }
        } else if (key == 'q' || key == 'Q' || key == 27) {
            break;
        }
    }
    
    // Clear find term after display
    find_term[0] = '\0';
    stream_length = 0;  // Clear stream so nothing leaks to later commands
    clear_screen();
    return -200;  // Signal pipeline to stop
}

// cmd_find - Highlight matching text in stream buffer
// Sets the find_term so read can highlight during pagination.
// If find is the last command in the pipeline, prints highlighted text directly.
// Stream buffer is NOT modified — highlighting is done at display time.
void cmd_find(const char* search_term) {
    if (!stream_buffer || stream_length == 0) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        printf("Error: No data in stream. Use cat <file> > find \"text\"\n");
        set_color(COLOR_WHITE, COLOR_BLACK);
        return;
    }
    
    if (!search_term || search_term[0] == '\0') {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        printf("Error: No search term provided\n");
        set_color(COLOR_WHITE, COLOR_BLACK);
        return;
    }
    
    // Store the search term globally so read can use it for highlighting
    int tlen = strlen(search_term);
    if (tlen > 255) tlen = 255;
    memcpy(find_term, search_term, tlen);
    find_term[tlen] = '\0';
    
    // If more commands follow in pipeline, just pass through
    // (e.g. find "text" > read — let read handle display with highlighting)
    if (piping && pipe_remaining > 0) {
        return;
    }
    
    // find is the last command — print highlighted text directly (no pagination)
    int term_len = strlen(find_term);
    for (int i = 0; i < stream_length; i++) {
        int match = 0;
        if (i + term_len <= stream_length) {
            match = 1;
            for (int j = 0; j < term_len; j++) {
                if (stream_buffer[i + j] != find_term[j]) {
                    match = 0;
                    break;
                }
            }
        }
        
        if (match) {
            set_color(COLOR_BLACK, COLOR_YELLOW);
            for (int j = 0; j < term_len; j++) {
                putchar(stream_buffer[i + j]);
            }
            set_color(COLOR_WHITE, COLOR_BLACK);
            i += term_len - 1;
        } else {
            putchar(stream_buffer[i]);
        }
    }
    if (stream_length > 0 && stream_buffer[stream_length - 1] != '\n') {
        printf("\n");
    }
    
    // Clear the find term after use
    find_term[0] = '\0';
}
