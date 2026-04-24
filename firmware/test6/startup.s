    .syntax unified
    .cpu cortex-m3
    .thumb

    .section .vectors, "a"
    .word   0x20001000
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
    .word   svc_handler + 1     @ [11] SVC
    .word   default_handler + 1
    .word   default_handler + 1
    .word   pendsv_handler + 1  @ [14] PendSV
    .word   systick_handler + 1 @ [15] SysTick

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

    @ SVC handler: load first task's PSP from current_task, switch to thread+PSP.
    @ Stack frame already constructed in task's stack by kernel_init.
    .thumb_func
    .global svc_handler
svc_handler:
    ldr     r3, =current_task
    ldr     r3, [r3]
    ldr     r0, [r3]            @ psp = current_task->sp (R4-R11 at top)
    ldmia   r0!, {r4-r11}       @ pop R4-R11, advance to exc frame
    msr     psp, r0
    movs    r0, #2
    msr     control, r0         @ switch to PSP
    isb
    ldr     lr, =0xFFFFFFFD     @ EXC_RETURN: thread + PSP
    bx      lr

    @ SysTick handler: pend PendSV, after N ticks halt.
    .thumb_func
    .global systick_handler
systick_handler:
    push    {r4, lr}
    ldr     r4, =systick_count
    ldr     r0, [r4]
    adds    r0, #1
    str     r0, [r4]
    cmp     r0, #20
    bge     systick_done
    ldr     r0, =0xE000ED04     @ SCB->ICSR
    ldr     r1, =0x10000000     @ PENDSVSET
    str     r1, [r0]
    pop     {r4, pc}
systick_done:
    ldr     r0, =counter0
    ldr     r0, [r0]
    ldr     r1, =counter1
    ldr     r1, [r1]
    .short  0xDEFE

    @ PendSV handler: save R4-R11 on current PSP, swap task pointer,
    @ restore R4-R11 from new PSP.
    .thumb_func
    .global pendsv_handler
pendsv_handler:
    mrs     r0, psp
    stmdb   r0!, {r4-r11}

    ldr     r3, =current_task
    ldr     r1, [r3]            @ r1 = old current
    str     r0, [r1]            @ save sp to old current->sp

    ldr     r2, =next_task
    ldr     r12, [r2]           @ r12 = old next
    str     r12, [r3]           @ current = old next
    str     r1, [r2]            @ next = old current

    ldr     r0, [r12]           @ new psp
    ldmia   r0!, {r4-r11}
    msr     psp, r0

    ldr     lr, =0xFFFFFFFD
    bx      lr
