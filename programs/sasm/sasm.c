#include "../../include/stdio.h"
#include "../../include/string.h"
#include "../../include/stdlib.h"

const char* error_messages[] = {
    "",  // errcode 0 (no error)
    "Missing output file flag (-o)",
    "Missing input file flag (-f)"
};

// Register encoding (for ModR/M and opcode)
const int RAX = 0x0;
const int RCX = 0x1;
const int RDX = 0x2;
const int RBX = 0x3;
const int RSP = 0x4;
const int RBP = 0x5;
const int RSI = 0x6;
const int RDI = 0x7;
const int R8  = 0x8;
const int R9  = 0x9;
const int R10 = 0xA;
const int R11 = 0xB;
const int R12 = 0xC;
const int R13 = 0xD;
const int R14 = 0xE;
const int R15 = 0xF;

// REX prefix bits (for 64-bit mode and extended registers)
const int REX_W = 0x48;  // 64-bit operand size
const int REX_R = 0x44;  // Extension of ModR/M reg field
const int REX_X = 0x42;  // Extension of SIB index field
const int REX_B = 0x41;  // Extension of ModR/M r/m field

// Common instruction opcodes
const int OP_MOV_REG_IMM = 0xB8;  // MOV reg, imm32/64 (+ reg in low 3 bits)
const int OP_MOV_RM_REG = 0x89;   // MOV r/m64, r64
const int OP_MOV_REG_RM = 0x8B;   // MOV r64, r/m64
const int OP_ADD_RM_REG = 0x01;   // ADD r/m64, r64
const int OP_SUB_RM_REG = 0x29;   // SUB r/m64, r64
const int OP_XOR_RM_REG = 0x31;   // XOR r/m64, r64
const int OP_PUSH_REG = 0x50;     // PUSH reg (+ reg in low 3 bits)
const int OP_POP_REG = 0x58;      // POP reg (+ reg in low 3 bits)
const int OP_RET = 0xC3;          // RET
const int OP_CALL_REL32 = 0xE8;   // CALL rel32
const int OP_JMP_REL32 = 0xE9;    // JMP rel32

// ModR/M byte encoding modes (2-bit mod field values)
const int MOD_INDIRECT = 0x00;        // [reg] - mod=00
const int MOD_INDIRECT_DISP8 = 0x01;  // [reg + disp8] - mod=01
const int MOD_INDIRECT_DISP32 = 0x02; // [reg + disp32] - mod=10
const int MOD_DIRECT = 0x03;          // reg-to-reg - mod=11

// Helper function to build ModR/M byte
int make_modrm(int mod, int reg, int rm) {
    return (mod << 6) | ((reg & 0x7) << 3) | (rm & 0x7);
}

// Helper to check if register needs REX prefix (R8-R15)
int needs_rex(int reg) {
    return reg >= 8;
}

// Helper to build REX prefix
int make_rex(int w, int r, int x, int b) {
    return 0x40 | (w << 3) | (r << 2) | (x << 1) | b;
}

// Parse register name to register number
int parse_register(const char* name) {
    if (strcmp(name, "rax") == 0) return RAX;
    if (strcmp(name, "rcx") == 0) return RCX;
    if (strcmp(name, "rdx") == 0) return RDX;
    if (strcmp(name, "rbx") == 0) return RBX;
    if (strcmp(name, "rsp") == 0) return RSP;
    if (strcmp(name, "rbp") == 0) return RBP;
    if (strcmp(name, "rsi") == 0) return RSI;
    if (strcmp(name, "rdi") == 0) return RDI;
    if (strcmp(name, "r8") == 0) return R8;
    if (strcmp(name, "r9") == 0) return R9;
    if (strcmp(name, "r10") == 0) return R10;
    if (strcmp(name, "r11") == 0) return R11;
    if (strcmp(name, "r12") == 0) return R12;
    if (strcmp(name, "r13") == 0) return R13;
    if (strcmp(name, "r14") == 0) return R14;
    if (strcmp(name, "r15") == 0) return R15;
    return -1;  // Invalid register
}

int main(int argc, char** argv) 
{
    if(argc < 2) {
        printf("Usage: sasm [options] [filename]\n");
        printf("Options:\n");
        printf("  -f <filename>  Specify the input file\n");
        printf("  -o <output>    Specify the output file\n");
        return 1;
    }
    char* flag;
    char* filename;
    char* output;
    int errcode = 0;

    flag = argv[1];
    if(strcmp(flag, "-f") == 0)
    {
        filename = argv[2];
        flag = argv[3];
        if(strcmp(flag, "-o") == 0)
        {
            output = argv[4];
        }
        else
        {
            errcode = 1; // 1 = missing output file flag
        }
    }
    else if(strcmp(flag, "-o") == 0)
    {
        output = argv[2];
        flag = argv[3];
        if(strcmp(flag, "-f") == 0)
        {
            filename = argv[4];
        }
        else
        {
            errcode = 2; // 2 = missing input file flag
        }
    }

    if(errcode != 0)
    {
        char errorMessage[256];
        sprintf(errorMessage, "Error: %s\n", error_messages[errcode]);
        printf("%s", errorMessage);
        return 1;
    }

    char* input_buffer = (char*)malloc(65536);
    if(input_buffer == NULL) {
        printf("Error: Failed to allocate memory for input buffer\n");
        return 1;
    }

    int bytes_read = read_file(filename, input_buffer, 65536);
    if(bytes_read < 1) {
        printf("Error: Failed to read input file, or it was blank\n");
        return 1;
    }

    input_buffer[bytes_read] = '\0';

    // Allocate output buffer for machine code
    unsigned char* output_buffer = (unsigned char*)malloc(65536);
    if(output_buffer == NULL) {
        printf("Error: Failed to allocate memory for output buffer\n");
        free(input_buffer);
        return 1;
    }
    int code_pos = 0;  // Current position in output buffer

    char* line_start = input_buffer;
    int line_num = 1;
    
    while (*line_start) {
        // Find end of line
        char* line_end = line_start;
        while (*line_end && *line_end != '\n' && *line_end != '\r') {
            line_end++;
        }
        
        // Extract line into a temporary buffer
        int line_len = line_end - line_start;
        char line[256];  // Max line length
        if (line_len > 255) line_len = 255;
        
        // Copy line
        for (int i = 0; i < line_len; i++) {
            line[i] = line_start[i];
        }
        line[line_len] = '\0';
        
        // Process this line
        printf("Line %d: %s\n", line_num, line);
        // Skip empty lines and comments
        if (line_len == 0 || line[0] == ';' || line[0] == '#') {
            continue;
        }

        // Parse instruction - split into tokens
        char* tokens[4];  // instruction + up to 3 operands
        int token_count = 0;
        char* p = line;

        // Get instruction mnemonic
        while (*p == ' ' || *p == '\t') p++;  // Skip leading whitespace
        tokens[token_count++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != ',') p++;
        if (*p) *p++ = '\0';

        // Get operands (comma-separated)
        while (*p && token_count < 4) {
            while (*p == ' ' || *p == '\t' || *p == ',') p++;
            if (!*p) break;
            tokens[token_count++] = p;
            while (*p && *p != ' ' && *p != '\t' && *p != ',') p++;
            if (*p) *p++ = '\0';
        }
        
        // Encode instruction
        char* instr = tokens[0];
        
        if (strcmp(instr, "ret") == 0) {
            // RET - single byte instruction
            output_buffer[code_pos++] = OP_RET;
        }
        else if (strcmp(instr, "push") == 0 && token_count == 2) {
            // PUSH reg
            int reg = parse_register(tokens[1]);
            if (reg < 0) {
                printf("Error line %d: Invalid register '%s'\n", line_num, tokens[1]);
                continue;
            }
            if (needs_rex(reg)) {
                output_buffer[code_pos++] = REX_B;
            }
            output_buffer[code_pos++] = OP_PUSH_REG + (reg & 0x7);
        }
        else if (strcmp(instr, "pop") == 0 && token_count == 2) {
            // POP reg
            int reg = parse_register(tokens[1]);
            if (reg < 0) {
                printf("Error line %d: Invalid register '%s'\n", line_num, tokens[1]);
                continue;
            }
            if (needs_rex(reg)) {
                output_buffer[code_pos++] = REX_B;
            }
            output_buffer[code_pos++] = OP_POP_REG + (reg & 0x7);
        }
        else if (strcmp(instr, "mov") == 0 && token_count == 3) {
            int dst = parse_register(tokens[1]);
            int src = parse_register(tokens[2]);
            
            if (dst >= 0 && src >= 0) {
                // MOV reg, reg
                int rex = REX_W;
                if (needs_rex(src)) rex |= 0x04;  // REX.R
                if (needs_rex(dst)) rex |= 0x01;  // REX.B
                output_buffer[code_pos++] = rex;
                output_buffer[code_pos++] = OP_MOV_RM_REG;
                output_buffer[code_pos++] = make_modrm(MOD_DIRECT, src, dst);
            }
            else {
                printf("Error line %d: Invalid MOV operands\n", line_num);
            }
        }
        else {
            printf("Error line %d: Unknown or malformed instruction '%s'\n", line_num, instr);
        }
        
        // Move to next line
        line_start = line_end;
        if (*line_start == '\r') line_start++;  // Skip \r
        if (*line_start == '\n') line_start++;  // Skip \n
        line_num++;
    }

    // Write output file
    printf("\nAssembled %d bytes of machine code\n", code_pos);
    
    int bytes_written = write_file(output, output_buffer, code_pos);
    if (bytes_written != code_pos) {
        printf("Error: Failed to write output file\n");
        free(input_buffer);
        free(output_buffer);
        return 1;
    }
    
    printf("Successfully wrote %d bytes to %s\n", bytes_written, output);
    
    // Clean up
    free(input_buffer);
    free(output_buffer);
    
    return 0;
}