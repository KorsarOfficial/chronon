    .syntax unified
    .cpu cortex-m0
    .thumb

    .section .vectors, "a"
    .word   0x20001000          @ initial MSP (top of 4 KB SRAM)
    .word   reset_handler + 1   @ reset vector (Thumb bit set)
    .word   default_handler + 1 @ NMI
    .word   default_handler + 1 @ HardFault

    .text
    .thumb_func
    .global reset_handler
reset_handler:
    bl      main
1:  b       1b                  @ trap if main returns

    .thumb_func
    .global default_handler
default_handler:
1:  b       1b
