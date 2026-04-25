#include "test_harness.h"
#include "core/tt.h"
#include "core/cpu.h"
#include "core/bus.h"
#include "core/nvic.h"
#include "core/jit.h"
#include "core/run.h"
#include "periph/systick.h"
#include "periph/scb.h"
#include "periph/mpu.h"
#include "periph/dwt.h"
#include "periph/stm32.h"
#include "periph/eth.h"
#include "periph/uart.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Minimal thumb blob: vector table + NOP x3 + B . */
static const u8 k_blob[] = {
    0x00, 0x00, 0x04, 0x20,  /* [0x00] SP = 0x20040000 */
    0x09, 0x00, 0x00, 0x00,  /* [0x04] reset = 0x8|1 */
    0x00, 0xBF,              /* [0x08] NOP */
    0x00, 0xBF,              /* [0x0A] NOP */
    0x00, 0xBF,              /* [0x0C] NOP */
    0xFE, 0xE7,              /* [0x0E] B . */
};

extern cpu_t*  g_cpu_for_scb;
extern dwt_t*  g_dwt_for_run;
extern nvic_t* g_nvic_for_run;

/* Large statics: jit_t ~2MB, snap_blob_t ~263KB. */
static jit_t s_g;
static snap_blob_t s_a, s_b;

static void setup(cpu_t* c, bus_t* bus, tt_periph_t* p,
                  systick_t* st, nvic_t* nv, scb_t* scb,
                  mpu_t* mpu, dwt_t* dwt, stm32_t* s32,
                  eth_t* eth, uart_t* u) {
    bus_init(bus);
    bus_add_flat(bus, "flash", 0x00000000u, 1024u * 1024u, false);
    bus_add_flat(bus, "sram",  SRAM_BASE_ADDR, SRAM_SIZE, true);
    memset(u, 0, sizeof *u);
    uart_attach(bus, u);
    systick_attach(bus, st);
    scb_attach(bus, scb);
    g_cpu_for_scb = c;
    mpu_attach(bus, mpu);
    stm32_attach(bus, s32);
    s32->quiet = true;
    dwt_attach(bus, dwt);
    g_dwt_for_run = dwt;
    nvic_attach(bus, nv);
    g_nvic_for_run = nv;
    eth_attach(bus, eth);
    bus_load_blob(bus, 0x00000000u, k_blob, (u32)sizeof k_blob);
    cpu_reset(c, CORE_M4);
    c->msp = bus_r32(bus, 0x0u);
    c->r[REG_SP] = c->msp;
    c->r[REG_PC] = bus_r32(bus, 0x4u) & ~1u;
    p->st = st; p->nv = nv; p->scb = scb;
    p->mpu = mpu; p->dwt = dwt; p->stm32 = s32;
    p->eth = eth; p->uart = u;
    memset(&s_g, 0, sizeof s_g);
    tt_attach_jit(&s_g);
}

TEST(tt_rewind_tests) {
    cpu_t c; bus_t bus;
    systick_t st; nvic_t nv; scb_t scb;
    mpu_t mpu; dwt_t dwt; stm32_t s32; eth_t eth; uart_t u;
    tt_periph_t p;

    setup(&c, &bus, &p, &st, &nv, &scb, &mpu, &dwt, &s32, &eth, &u);

    /* stride=10000, capacity=200 -> covers 2M cycles of history */
    tt_t* tt = tt_create(10000u, 200u);
    ASSERT_TRUE(tt != NULL);
    ASSERT_TRUE(g_tt == tt);

    /* Run 1M cycles in batches of 10K; tt_on_cycle snaps at each stride boundary. */
    const u64 TOTAL = 1000000ull;
    while (c.cycles < TOTAL) {
        run_steps_full_g(&c, &bus, 10000u, &st, &scb, &s_g);
        tt_on_cycle(tt, &c, &bus, &p);
    }
    ASSERT_TRUE(tt->n_snaps >= 100u);

    /* TT-06: rewind to 10 random targets across 1M history (all >= stride=10000),
       mean < 100ms */
    u64 targets[10] = {123456ull, 50000ull, 999999ull, 250000ull, 777777ull,
                       10001ull,  500000ull, 850000ull, 333333ull, 666666ull};
    clock_t t0 = clock();
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(tt_rewind(tt, targets[i], &c, &bus, &p, &s_g));
        ASSERT_TRUE(c.cycles >= targets[i]);
    }
    clock_t t1 = clock();
    double ms_mean = 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC / 10.0;
    fprintf(stderr, "tt_rewind mean=%.3fms over 10 random targets in 1M history\n", ms_mean);
    ASSERT_TRUE(ms_mean < 100.0);

    /* TT-07: step_back precision (whole-instruction granularity +-1 ARM cycle).
       Rewind to anchor near snap boundary, then step_back(1) and step_back(100). */
    ASSERT_TRUE(tt_rewind(tt, 99999ull, &c, &bus, &p, &s_g));
    u64 anchor = c.cycles;
    ASSERT_TRUE(tt_step_back(tt, 1ull, &c, &bus, &p, &s_g));
    /* c.cycles in [anchor-1, anchor]: whole-instruction granularity */
    ASSERT_TRUE(c.cycles >= anchor - 1ull && c.cycles <= anchor);
    /* chain: step_back another 100 from new position */
    u64 anchor2 = c.cycles;
    ASSERT_TRUE(tt_step_back(tt, 100ull, &c, &bus, &p, &s_g));
    ASSERT_TRUE(c.cycles >= anchor2 - 101ull && c.cycles <= anchor2 - 100ull + 1ull);

    /* TT-08: tt_diff output contains "R0:" and "SRAM[0x20000100" */
    ASSERT_TRUE(snap_save(&s_a, &c, &bus, &p));
    c.r[0] = 0xDEADBEEFu;
    region_t* sr = bus_find_flat(&bus, SRAM_BASE_ADDR);
    ASSERT_TRUE(sr != NULL);
    sr->buf[0x100] = 0x11u; sr->buf[0x101] = 0x22u;
    sr->buf[0x102] = 0x33u; sr->buf[0x103] = 0x44u;
    ASSERT_TRUE(snap_save(&s_b, &c, &bus, &p));

    FILE* f = fopen("tt_diff_out.txt", "w");
    ASSERT_TRUE(f != NULL);
    tt_diff(&s_a, &s_b, f);
    fclose(f);

    f = fopen("tt_diff_out.txt", "r");
    ASSERT_TRUE(f != NULL);
    char buf[4096];
    size_t n = fread(buf, 1, sizeof buf - 1u, f);
    buf[n] = '\0';
    fclose(f);
    remove("tt_diff_out.txt");

    ASSERT_TRUE(strstr(buf, "R0:") != NULL);
    ASSERT_TRUE(strstr(buf, "SRAM[0x20000100") != NULL);

    tt_destroy(tt);
}

int main(void) {
    RUN(tt_rewind_tests);
    TEST_REPORT();
}
