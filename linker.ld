OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    headers PT_LOAD FLAGS((1 << 0) | (1 << 1) | (1 << 2));  /* RWX */
}

SECTIONS
{
    /* Kernel loaded at 2MB */
    . = 0x200000;
    
    .text : ALIGN(4K) {
        KEEP(*(.text.startup))
        *(.text*)
    } :headers
    
    .rodata : ALIGN(4K) {
        *(.rodata*)
    } :headers
    
    .data : ALIGN(4K) {
        *(.data*)
    } :headers
    
    .bss : ALIGN(4K) {
        *(COMMON)
        *(.bss*)
        *(.stack)
    } :headers
    
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}