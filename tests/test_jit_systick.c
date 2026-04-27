/* Regression: jit_run_chained must not suppress periodic SysTick IRQ delivery.
   With irq_safe_budget(), the chain budget is capped to the current SysTick CVR
   so each tick boundary causes a chain return -> systick_tick -> IRQ check cycle.
   Without the cap, the entire max_steps budget runs in one chain call, all but
   the last SysTick reload are silently discarded, and the counter stays near 0.

   Setup:
     flash @ 0x00000000:
       0x00 (reset handler): set up SP, jump to main
       0x3C (SysTick vector): handler address
     main loop: busy-waits on tick_count < TARGET
     SysTick handler: increments tick_count, BX LR

   SysTick reload = 100 cycles.  After N_TICKS firings the handler sets a flag and
   main falls through to the BKPT halt.  We verify tick_count == N_TICKS at halt. */

#include "test_harness.h"
#include "core/cpu.h"
#include "core/bus.h"
#include "core/decoder.h"
#include "core/jit.h"
#include "core/run.h"
#include "periph/systick.h"
#include "periph/scb.h"
#include <string.h>
#include <stdio.h>

/* Large static to avoid stack overflow (jit_t ~ 2 MB). */
static jit_t s_jit;

/* tick_count lives at SRAM base + 0. */
#define SRAM_BASE    0x20000000u
#define TICK_ADDR    SRAM_BASE
#define STACK_TOP    (SRAM_BASE + 0x2000u)

/* Flash layout (Thumb, 2-byte instructions where noted):
   0x00: vector[0] = initial SP  = STACK_TOP         (4 bytes, data)
   0x04: vector[1] = reset addr  = 0x09 | 1          (4 bytes, Thumb bit)
   ...
   0x3C: vector[15]= SysTick     = handler_addr | 1
   0x08..0x3B: unused (NOP or zero, handler and main share later space)

   Reset handler @ 0x08:
     LDR  r0, =main_entry_addr   -- but easier: B 0x40 (to main)
     That is B.UNCOND with target 0x40.
     From 0x08: pc+4 = 0x0C, target = 0x40, imm = 0x40 - 0x0C = 0x34 = 52 halfwords / 2...
     Actually imm for B.T2 (Thumb-1 B) is in halfwords. offset_bytes = 0x34, halfwords = 0x1A.
     imm11 = 0x1A - 2 = 0x18 (B adds +4 already accounted, imm11 in halfword units, pc+4+imm*2)
     Let's just place code at 0x40 for main and 0x80 for handler.

   Simpler: place reset at 0x09 (Thumb, so code at 0x08), main at 0x40, handler at 0x80. */

/* All hand-encoded as little-endian u8 pairs. */

/* tick_count offset in SRAM, as an absolute address for LDR literal. */

/*
   main @ 0x40:
     LDR  r1, [pc, #20]    ; load &tick_count = TICK_ADDR  (literal pool at 0x58)
     MOV  r2, #N_TICKS
   .loop:
     LDR  r0, [r1, #0]     ; load tick_count
     CMP  r0, r2
     BLT  .loop            ; branch back while < N_TICKS
     BKPT #0               ; halt

   SysTick handler @ 0x80:
     LDR  r1, [pc, #8]     ; load &tick_count (literal pool at 0x8C)
     LDR  r0, [r1, #0]
     ADD  r0, r0, #1
     STR  r0, [r1, #0]
     BX   LR

   Reset handler @ 0x08:
     B    main (0x40)       ; B.UNCOND from 0x08: imm = (0x40 - 0x0C)/2 = 0x1A, imm11 = 0x18
                            ; encoding: 11100 00000011000 -> hi = 0b11100000 = 0xE0,
                            ;                                lo = 0b00011000 = 0x18 (LE: 0x18 0xE0)

   Vector table:
     0x00: STACK_TOP = 0x20002000
     0x04: reset_addr = 0x09 (0x08 | Thumb bit)
     ...
     0x3C: systick_addr = 0x81 (0x80 | Thumb bit)
*/

#define N_TICKS 10

static void build_image(bus_t* bus) {
    region_t* fl = bus_find_flat(bus, 0x00000000u);
    if (!fl) return;
    fl->writable = true;

    u8* m = fl->buf;

    /* --- Vector table --- */
    /* 0x00: initial SP (MSP) */
    m[0]=0x00; m[1]=0x20; m[2]=0x00; m[3]=0x20;  /* STACK_TOP = 0x20002000 */
    /* 0x04: reset vector */
    m[4]=0x09; m[5]=0x00; m[6]=0x00; m[7]=0x00;  /* 0x08 | 1 */
    /* 0x08..0x3B: zero (unused vectors) */
    /* 0x3C: SysTick vector = 0x80 | 1 */
    m[0x3C]=0x81; m[0x3D]=0x00; m[0x3E]=0x00; m[0x3F]=0x00;

    /* --- Reset handler @ 0x08: B to main (0x40) ---
       Thumb B.UNCOND T1: PC_new = PC + 4 + SignExtend(imm11 << 1, 12)
       PC = 0x08, target = 0x40, delta = 0x40 - (0x08 + 4) = 0x34, imm11 = 0x34/2 = 0x1A
       Encoding: 11100 00000011010 -> 0xE01A (LE: 0x1A, 0xE0) */
    m[0x08]=0x1Au; m[0x09]=0xE0u;

    /* --- main @ 0x40 ---
       We need: LDR r1, literal (&tick_count=0x20000000)
       LDR Rd, [PC, #imm8*4]:  encoding 01001 ddd iiiiiiii
         dd=01 (r1), pc_eff = (0x40+4) & ~3 = 0x44, imm8 = offset/4
         literal at 0x58: offset from 0x44 = 0x14 = 20 bytes, imm8 = 5
         encoding: 01001 001 00000101 = 0x4905 (LE: 0x05, 0x49)
    */
    m[0x40]=0x05u; m[0x41]=0x49u;  /* LDR r1, [pc, #20]  -> loads from 0x44+0x14=0x58 */

    /* MOV r2, #N_TICKS  (MOVS r2, imm8): 00100 010 iiiiiiii = 0x220A (N_TICKS=10)
       encoding: 0010 0 010 0000 1010 = 0x220A (LE: 0x0A, 0x22) */
    m[0x42]=0x0Au; m[0x43]=0x22u;  /* MOVS r2, #10 */

    /* .loop @ 0x44:
       LDR r0, [r1, #0]:  0110 0 00000 001 000 = 0x6808 (LE: 0x08, 0x68) */
    m[0x44]=0x08u; m[0x45]=0x68u;  /* LDR r0, [r1] */

    /* CMP r0, r2:  0100 0010 10 010 000 = 0x4290 (LE: 0x90, 0x42) */
    m[0x46]=0x90u; m[0x47]=0x42u;  /* CMP r0, r2 */

    /* BLT .loop (0x44): cond=LT=0b1011, pc_eff=0x48+4=0x4C, target=0x44
       delta = 0x44 - 0x4C = -8 bytes, imm8 = -8/2 = -4 = 0xFC
       encoding: 1101 1011 1111 1100 = 0xDBFC (LE: 0xFC, 0xDB) */
    m[0x48]=0xFCu; m[0x49]=0xDBu;  /* BLT .loop */

    /* BKPT #0: 1011 1110 0000 0000 = 0xBE00 (LE: 0x00, 0xBE) */
    m[0x4A]=0x00u; m[0x4B]=0xBEu;  /* BKPT -- halt */

    /* padding to reach literal pool at 0x58 (2 bytes used for alignment) */
    m[0x4C]=0x00u; m[0x4D]=0x00u;  /* NOP (or just zero) */
    m[0x4E]=0x00u; m[0x4F]=0x00u;
    m[0x50]=0x00u; m[0x51]=0x00u;
    m[0x52]=0x00u; m[0x53]=0x00u;
    m[0x54]=0x00u; m[0x55]=0x00u;
    m[0x56]=0x00u; m[0x57]=0x00u;

    /* Literal pool @ 0x58: &tick_count = 0x20000000 */
    m[0x58]=0x00u; m[0x59]=0x00u; m[0x5A]=0x00u; m[0x5B]=0x20u;

    /* --- SysTick handler @ 0x80 ---
       LDR r1, [pc, #8]:  pc_eff = (0x80+4)&~3 = 0x84, offset=8, imm8=2
         encoding: 0100 1 001 0000 0010 = 0x4902 (LE: 0x02, 0x49) */
    m[0x80]=0x02u; m[0x81]=0x49u;  /* LDR r1, [pc, #8] -> from 0x8C */

    /* LDR r0, [r1, #0] */
    m[0x82]=0x08u; m[0x83]=0x68u;

    /* ADDS r0, r0, #1 (ADDS rd,rd,imm3 T1... or ADDS rd,imm8 T2):
       ADDS r0, #1 T2: 0011 0 000 0000 0001 = 0x3001 (LE: 0x01, 0x30) */
    m[0x84]=0x01u; m[0x85]=0x30u;

    /* STR r0, [r1, #0]: 0110 0 00000 001 000 = 0x6008 (LE: 0x08, 0x60) */
    m[0x86]=0x08u; m[0x87]=0x60u;

    /* BX LR: 0100 0111 0 1110 000 = 0x4770 (LE: 0x70, 0x47) */
    m[0x88]=0x70u; m[0x89]=0x47u;

    /* padding */
    m[0x8A]=0x00u; m[0x8B]=0x00u;

    /* Literal pool @ 0x8C: &tick_count = 0x20000000 */
    m[0x8C]=0x00u; m[0x8D]=0x00u; m[0x8E]=0x00u; m[0x8F]=0x20u;

    fl->writable = false;
}

/* --- test_systick_irq_periodic ---
   Run a busy-wait loop with SysTick reload=100, verify it gets N_TICKS interrupts
   and halts normally (not via max_steps timeout). */
TEST(test_systick_irq_periodic) {
    bus_t bus; bus_init(&bus);
    bus_add_flat(&bus, "flash", 0x00000000u, 0x1000u, false);
    bus_add_flat(&bus, "sram",  SRAM_BASE,   0x2000u, true);

    systick_t st; systick_attach(&bus, &st);
    scb_t scb;    scb_attach(&bus, &scb);

    build_image(&bus);

    cpu_t c; memset(&c, 0, sizeof c);
    cpu_reset(&c, CORE_M4);
    /* ARM reset: read MSP from vector[0], PC from vector[1]. */
    u32 msp = 0, entry = 0;
    bus_read(&bus, 0x00u, 4, &msp);
    bus_read(&bus, 0x04u, 4, &entry);
    c.r[REG_SP] = msp;
    c.r[REG_PC] = entry & ~1u;

    /* Configure SysTick: reload=100, ENABLE|TICKINT. */
    st.rvr = 100u;
    st.cvr = 0u;
    st.csr = 0x3u;  /* ENABLE | TICKINT */

    memset(&s_jit, 0, sizeof s_jit);
    jit_init(&s_jit);

    /* Run up to 100000 steps.  Should halt after N_TICKS * ~100 = ~1000 cycles
       plus handler overhead.  If bug present: runs all 100000 with tick_count ~ 1. */
    u64 steps = run_steps_full_g(&c, &bus, 100000ull, &st, &scb, &s_jit);

    u32 tick_count = 0;
    bus_read(&bus, TICK_ADDR, 4, &tick_count);

    ASSERT_TRUE(c.halted);
    ASSERT_TRUE(tick_count == (u32)N_TICKS);
    (void)steps;
}

int main(void) {
    RUN(test_systick_irq_periodic);
    TEST_REPORT();
}
