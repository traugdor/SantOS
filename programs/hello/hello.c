//simple hello world program

#include "../../include/stdio.h"
#include "../../include/string.h"
#include "../../include/stdlib.h"

void calculator (void) {
    clear_screen();
    int choice;
    do {
        printf("Calculator Program\n\n");
        printf("Select the function:\n");
        printf("1. Addition\n");
        printf("2. Subtraction\n");
        printf("3. Multiplication\n");
        printf("4. Division\n");
        printf("5. Exit\n");

        printf("Enter your choice: ");
        scanf("%d", &choice);

        int num1, num2;

        printf("Enter first number: ");
        scanf("%d", &num1);
        printf("Enter second number: ");
        scanf("%d", &num2);
        
        //process input
        if (choice == 1) {
            //call addition
            printf("Result: %d + %d = %d\n", num1, num2, num1 + num2);
        } else if (choice == 2) {
            //call subtraction
            printf("Result: %d - %d = %d\n", num1, num2, num1 - num2);
        } else if (choice == 3) {
            //call multiplication
            printf("Result: %d * %d = %d\n", num1, num2, num1 * num2);
        } else if (choice == 4) {
            //call division
            if(num2 != 0) {
                printf("Result: %d / %d = %d\n", num1, num2, num1 / num2);
            } else {
                printf("Error: Division by zero\n");
            }
        } else if (choice > 5){
            printf("Invalid choice\n");
        }
    }while (choice != 5);
    
}

void proto_spell_checker(void)
{
    const char* common_words[] = {
        "accommodate", "achieve", "acquire", "a lot", "apparent", "argument",
        "calendar", "cemetery", "definitely", "discipline", "embarrass",
        "environment", "existence", "experience", "familiar", "February",
        "government", "grammar", "guarantee", "hierarchy", "independent",
        "indispensable", "intelligence", "interrupt", "judgment", "leisure",
        "license", "lightning", "maintenance", "maneuver", "millennium",
        "mischievous", "noticeable", "occasion", "occurrence", "perseverance",
        "possession", "privilege", "questionnaire", "receive", "recommend",
        "rhythm", "separate", "supersede", "surprise", "threshold",
        "tomorrow", "twelfth", "vacuum", "weird"
    };
    int word_count = sizeof(common_words) / sizeof(common_words[0]);
    
    clear_screen();
    printf("Prototype Spell Checker\n\n");
    printf("Input a word and I will attempt to find spelling errors for you!\n");
    printf("Input: ");
    char input[256];
    scanf("%s", input);
    printf("You entered: %s\n", input);
    
    // Check if word is in dictionary
    int found = 0;
    for (int i = 0; i < word_count; i++) {
        if (strcmp(input, common_words[i]) == 0) {
            found = 1;
            break;
        }
    }
    
    if (found) {
        printf("Word found in dictionary!\n");
    } else {
        printf("Word not found - possible spelling error\n");
    }
}

int main(int argc, char** argv) {
    int input;
    do 
    {
        clear_screen();
        printf("================================\n");
        printf("=                              =\n");
        printf("=        SantOS Example        =\n");
        printf("=                              =\n");
        printf("================================\n");

        //print a menu
        //1. Calculator
        //2. prototype spell checker
        //3. Exit
        
        printf("1. Calculator\n");
        printf("2. prototype spell checker\n");
        printf("3. Exit\n");
        
        //get input
        printf("Enter your choice: ");
        scanf("%d", &input);
        
        //process input
        if (input == 1) {
            calculator();
        } else if (input == 2) {
            proto_spell_checker();
        } else if (input == 3) {
            //exit
            return 0;
        } else {
            printf("Invalid choice\n");
        }
    } while(input != 3);
    
    return 0;
}