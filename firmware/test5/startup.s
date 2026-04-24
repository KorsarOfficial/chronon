    .syntax unified
    .cpu cortex-m3
    .thumb

    .section .vectors, "a"
    .word   0x20001000          @ [0] MSP
    .word   reset_handler + 1   @ [1] Reset
    .word   default_handler + 1 @ [2] NMI
    .word   default_handler + 1 @ [3] HardFault
    .word   default_handler + 1 @ [4]
    .word   default_handler + 1 @ [5]
    .word   default_handler + 1 @ [6]
    .word   default_handler + 1 @ [7]
    .word   default_handler + 1 @ [8]
    .word   default_handler + 1 @ [9]
    .word   default_handler + 1 @ [10]
    .word   svc_handler + 1     @ [11] SVC
    .word   default_handler + 1 @ [12]
    .word   default_handler + 1 @ [13]
    .word   pendsv_handler + 1  @ [14] PendSV
    .word   default_handler + 1 @ [15] SysTick

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
    .global svc_handler
svc_handler:
    @ SVC #0 = marker that just returns.
    bx      lr

    .thumb_func
    .global pendsv_handler
pendsv_handler:
    @ Simple PendSV: increment counter, return.
    ldr     r0, =pendsv_count
    ldr     r1, [r0]
    adds    r1, #1
    str     r1, [r0]
    bx      lr
