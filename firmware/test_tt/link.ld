MEMORY {
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 64K
    SRAM  (rw) : ORIGIN = 0x20000000, LENGTH = 16K
}

ENTRY(reset_handler)

SECTIONS {
    .vectors : { KEEP(*(.vectors)) } > FLASH
    .text    : { *(.text*) *(.rodata*) } > FLASH
    .data    : { *(.data*) } > SRAM AT > FLASH
    .bss     : { *(.bss*)  *(COMMON) } > SRAM
}
