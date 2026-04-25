    .syntax unified
    .cpu cortex-m3
    .thumb

    .section .vectors, "a"
    .word   0x20001000          @ [0]  MSP
    .word   reset_handler + 1   @ [1]  Reset
    .rept 14                    @ [2..15] system exceptions
    .word   default_handler + 1
    .endr
    @ External IRQ vectors (16+)
    .word   irq0_handler + 1    @ [16] IRQ0
    .word   irq1_handler + 1    @ [17] IRQ1

    .text
    .thumb_func
    .global reset_handler
reset_handler:
    ldr     r0, =__data_start
    ldr     r1, =__data_end
    ldr     r2, =__data_load
1:  cmp     r0, r1
    beq     2f
    ldr     r3, [r2]
    str     r3, [r0]
    adds    r0, #4
    adds    r2, #4
    b       1b
2:
    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    movs    r3, #0
3:  cmp     r0, r1
    beq     4f
    str     r3, [r0]
    adds    r0, #4
    b       3b
4:
    bl      main
5:  b       5b

    .thumb_func
    .global default_handler
default_handler:
1:  b       1b

    .thumb_func
    .global irq0_handler
irq0_handler:
    ldr     r0, =irq0_count
    ldr     r1, [r0]
    adds    r1, #1
    str     r1, [r0]
    bx      lr

    .thumb_func
    .global irq1_handler
irq1_handler:
    ldr     r0, =irq1_count
    ldr     r1, [r0]
    adds    r1, #10
    str     r1, [r0]
    bx      lr
