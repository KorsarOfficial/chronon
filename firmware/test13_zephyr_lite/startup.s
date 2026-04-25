    .syntax unified
    .cpu cortex-m3
    .thumb

    .extern k_current
    .extern k_next
    .extern systick_tick_c

    .section .vectors, "a"
    .word   0x20002000          @ MSP
    .word   reset_handler + 1
    .word   default_handler + 1 @ NMI
    .word   default_handler + 1 @ HardFault
    .word   default_handler + 1
    .word   default_handler + 1
    .word   default_handler + 1
    .word   default_handler + 1
    .word   default_handler + 1
    .word   default_handler + 1
    .word   default_handler + 1
    .word   svc_handler + 1     @ SVC
    .word   default_handler + 1
    .word   default_handler + 1
    .word   pendsv_handler + 1  @ PendSV
    .word   systick_handler + 1 @ SysTick

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
    ldr     r3, =k_current
    ldr     r3, [r3]
    ldr     r0, [r3]            @ psp = k_current->sp
    ldmia   r0!, {r4-r11}
    msr     psp, r0
    movs    r0, #2
    msr     control, r0
    isb
    ldr     lr, =0xFFFFFFFD
    bx      lr

    .thumb_func
    .global systick_handler
systick_handler:
    push    {lr}
    bl      systick_tick_c
    pop     {pc}

    .thumb_func
    .global pendsv_handler
pendsv_handler:
    mrs     r0, psp
    stmdb   r0!, {r4-r11}
    ldr     r3, =k_current
    ldr     r1, [r3]
    str     r0, [r1]            @ save sp to k_current->sp

    ldr     r2, =k_next
    ldr     r12, [r2]
    str     r12, [r3]           @ k_current = k_next

    ldr     r0, [r12]           @ new psp
    ldmia   r0!, {r4-r11}
    msr     psp, r0

    ldr     lr, =0xFFFFFFFD
    bx      lr
