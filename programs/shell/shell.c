// SantOS Shell - Simple Command Prompt
// Uses kernel's printf and getchar functions

#include "../../include/stdio.h"

#define MAX_INPUT 256

void shell_main(void) {
    char input[MAX_INPUT];
    
    printf("\nSantOS Shell v1.0\n");
    printf("Type commands and press Enter\n\n");
    
    while (1) {
        printf("> ");
        
        // Read input
        int i = 0;
        while (i < MAX_INPUT - 1) {
            char c = getchar();
            
            if (c == '\n') {
                input[i] = '\0';
                printf("\n");
                break;
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
        
        // Echo input
        if (input[0] != '\0') {
            printf("You typed: %s\n", input);
        }
    }
}
