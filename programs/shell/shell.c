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
void execute_block(void);
int is_numeric(const char* str);
int str_to_int(const char* str);
char* int_to_str(int val);
const char* resolve_value(const char* token, int* is_alloc);
int evaluate_condition(const char* val1_raw, const char* op, const char* val2_raw);

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
            printf("%s> ", current_directory);
        }
        
        // Read input
        int i = 0;
        while (i < MAX_INPUT - 1) {
            char c = getchar();
            
            if (c == '\n') {
                input[i] = '\0';
                printf("\n");
                
                // Process command if not empty
                if (input[0] != '\0') {
                    // If collecting a block, handle block lines
                    if (collecting_block) {
                        // Skip comments
                        char* trimmed = input;
                        while (*trimmed == ' ') trimmed++;
                        if (trimmed[0] == '#' && trimmed[1] == '#') {
                            break;  // Ignore comment lines
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
                            // Add line to block
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
                    if (error_code == -100) { //kill and shut down
                        // Exit the shell
                        return;
                    }
                    // Only print error for actual failures
                    // (process_command already prints "Unknown command")
                }
                break;  // Break from input loop to show new prompt
            } else if (c == '\b') {
                if (i > 0) {
                    i--;
                    printf("\b \b");
                }
            } else if (c >= 32 && c <= 126) {
                input[i++] = c;
                printf("%c", c);
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

int process_command(char* command) {
    // Check for program/script execution (./ prefix)
    if (command[0] == '.' && command[1] == '/') {
        char* filename = &command[2];
        
        // Read first 6 bytes to check for script marker ##/sosh
        // For now, just execute as program - script support will come later
        int result = exec_program(filename);
        
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
        printf("  cd       - Change directory (cd ., cd .., cd <dir>)\n");
        printf("  clear    - Clear the screen\n");
        printf("  dir      - List directory contents (alias for ls)\n");
        printf("  echo     - Echo text with variable expansion ($VAR)\n");
        printf("  exit     - Exit the shell\n");
        printf("  help     - Show this help message\n");
        printf("  listvars - List all defined variables\n");
        printf("  ls       - List directory contents\n");
        printf("  sub      - Subtract from numeric variable (sub $VAR value)\n");
        printf("  unset    - Unset a variable (unset $VAR, unset allvars)\n");
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
        // Print everything after the command name, expanding variables
        char* result = substr(command_copy, 6, 0);
        if (result) {
            // Process string for variable expansion
            for (int i = 0; i < strlen(result); i++) {
                if (result[i] == '$') {
                    // Found variable reference, extract variable name
                    int var_start = i + 1;
                    int var_end = var_start;
                    // Find end of variable name (alphanumeric + underscore)
                    while (var_end < strlen(result) && 
                           ((result[var_end] >= 'a' && result[var_end] <= 'z') ||
                            (result[var_end] >= 'A' && result[var_end] <= 'Z') ||
                            (result[var_end] >= '0' && result[var_end] <= '9') ||
                            result[var_end] == '_')) {
                        var_end++;
                    }
                    
                    if (var_end > var_start) {
                        // Extract variable name
                        char* var_name = substr(result, var_start + 1, var_end);
                        if (var_name) {
                            // Look up variable
                            int found = 0;
                            for (int j = 0; j < variable_count; j++) {
                                if (strcmp(variables[j], var_name) == 0) {
                                    printf("%s", values[j]);
                                    found = 1;
                                    break;
                                }
                            }
                            if (!found) {
                                // Variable not found, print nothing (or could print $VARNAME)
                            }
                            free(var_name);
                            i = var_end - 1; // Skip past variable name
                        }
                    } else {
                        // Just a $ with nothing after
                        putchar('$');
                    }
                } else {
                    putchar(result[i]);
                }
            }
            printf("\n");
            free(result);
        } else {
            printf("\n");
        }
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

    // TODO: Handle external program execution
    set_color(COLOR_LIGHT_RED, COLOR_BLACK);
    printf("Unknown command: %s", command_name);
    set_color(COLOR_WHITE, COLOR_BLACK);  // Reset to default
    // Print spaces to clear rest of line (80 chars total, estimate message length)
    int msg_len = 17 + strlen(command_name); // "Unknown command: " + command_name
    for (int i = msg_len; i < 80; i++) {
        putchar(' ');
    }
    printf("\n");
    free(command_copy);
    return 0;  // Command not found
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
