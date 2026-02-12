OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    text    PT_LOAD FLAGS((1 << 0) | (1 << 2));  /* Execute + Read */
    rodata  PT_LOAD FLAGS((1 << 2));             /* Read only */
    data    PT_LOAD FLAGS((1 << 1) | (1 << 2));  /* Write + Read */
    bss     PT_LOAD FLAGS((1 << 1) | (1 << 2));  /* Write + Read */
}

SECTIONS
{
    /* Kernel loaded at 2MB */
    . = 0x200000;
    
    .text : ALIGN(4K) {
        *(.text*)
    } :text
    
    .rodata : ALIGN(4K) {
        *(.rodata*)
    } :rodata
    
    .data : ALIGN(4K) {
        *(.data*)
    } :data
    
    .bss : ALIGN(4K) {
        *(COMMON)
        *(.bss*)
        *(.stack)
    } :bss
    
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}