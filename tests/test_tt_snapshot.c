#include "test_harness.h"
#include "core/tt.h"
#include "core/cpu.h"
#include "core/bus.h"
#include "core/nvic.h"
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

extern cpu_t*   g_cpu_for_scb;
extern dwt_t*   g_dwt_for_run;
extern nvic_t*  g_nvic_for_run;

/* snap_blob_t ~263KB each: must not be stack-allocated on Windows (1MB default stack). */
static snap_blob_t s_a, s_b, s_loaded;

static void setup(cpu_t* c, bus_t* bus, tt_periph_t* p,
                  systick_t* st, nvic_t* nv, scb_t* scb,
                  mpu_t* mpu, dwt_t* dwt, stm32_t* stm32,
                  eth_t* eth, uart_t* u) {
    bus_init(bus);
    bus_add_flat(bus, "flash", 0x00000000u, 1024u*1024u, false);
    bus_add_flat(bus, "sram",  SRAM_BASE_ADDR, SRAM_SIZE, true);
    memset(u, 0, sizeof *u);
    uart_attach(bus, u);
    systick_attach(bus, st);
    scb_attach(bus, scb);
    g_cpu_for_scb = c;
    mpu_attach(bus, mpu);
    stm32_attach(bus, stm32);
    stm32->quiet = true;
    dwt_attach(bus, dwt);
    g_dwt_for_run = dwt;
    nvic_attach(bus, nv);
    g_nvic_for_run = nv;
    eth_attach(bus, eth);
    cpu_reset(c, CORE_M4);
    p->st = st; p->nv = nv; p->scb = scb;
    p->mpu = mpu; p->dwt = dwt; p->stm32 = stm32;
    p->eth = eth; p->uart = u;
}

/* TT-03: two saves from same state produce byte-equal blobs. */
TEST(tt_snap_double_save_equal) {
    cpu_t c; bus_t bus;
    systick_t st; nvic_t nv; scb_t scb; mpu_t mpu; dwt_t dwt; stm32_t stm32;
    eth_t eth; uart_t u;
    tt_periph_t p;
    setup(&c, &bus, &p, &st, &nv, &scb, &mpu, &dwt, &stm32, &eth, &u);

    c.r[0] = 0xDEADBEEFu;
    c.r[1] = 0xCAFEBABEu;
    region_t* sr = bus_find_flat(&bus, SRAM_BASE_ADDR);
    sr->buf[0] = 0xAAu; sr->buf[100] = 0xBBu;

    ASSERT_TRUE(snap_save(&s_a, &c, &bus, &p));
    ASSERT_TRUE(snap_save(&s_b, &c, &bus, &p));
    ASSERT_TRUE(memcmp(&s_a, &s_b, sizeof s_a) == 0);
}

/* TT-03: restore reproduces cpu_t, SRAM, eth.bus back-pointer. */
TEST(tt_snap_restore_byte_equal) {
    cpu_t c; bus_t bus;
    systick_t st; nvic_t nv; scb_t scb; mpu_t mpu; dwt_t dwt; stm32_t stm32;
    eth_t eth; uart_t u;
    tt_periph_t p;
    setup(&c, &bus, &p, &st, &nv, &scb, &mpu, &dwt, &stm32, &eth, &u);

    c.r[0] = 0xDEADBEEFu;
    c.r[1] = 0xCAFEBABEu;
    region_t* sr = bus_find_flat(&bus, SRAM_BASE_ADDR);
    sr->buf[0] = 0xAAu; sr->buf[100] = 0xBBu;

    ASSERT_TRUE(snap_save(&s_a, &c, &bus, &p));

    c.r[0] = 0xDEADu;
    sr->buf[0] = 0x11u;

    ASSERT_TRUE(snap_restore(&s_a, &c, &bus, &p));
    ASSERT_TRUE(c.r[0] == 0xDEADBEEFu && c.r[1] == 0xCAFEBABEu);
    ASSERT_TRUE(sr->buf[0] == 0xAAu && sr->buf[100] == 0xBBu);
    ASSERT_TRUE(eth.bus == &bus);
}

/* TT-04: 100 consecutive restores complete with mean < 100ms (expected ~26us each). */
TEST(tt_snap_restore_latency) {
    cpu_t c; bus_t bus;
    systick_t st; nvic_t nv; scb_t scb; mpu_t mpu; dwt_t dwt; stm32_t stm32;
    eth_t eth; uart_t u;
    tt_periph_t p;
    setup(&c, &bus, &p, &st, &nv, &scb, &mpu, &dwt, &stm32, &eth, &u);
    ASSERT_TRUE(snap_save(&s_a, &c, &bus, &p));

    clock_t t0 = clock();
    for (int i = 0; i < 100; ++i) snap_restore(&s_a, &c, &bus, &p);
    clock_t t1 = clock();
    double ms = 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC / 100.0;
    /* print latency so CI can confirm sub-ms actual */
    fprintf(stderr, "[tt_snap_restore_latency] mean=%.3f ms/restore\n", ms);
    ASSERT_TRUE(ms < 100.0);
}

/* File round-trip: save_to_file -> load_from_file -> byte-equal. */
TEST(tt_snap_file_roundtrip) {
    cpu_t c; bus_t bus;
    systick_t st; nvic_t nv; scb_t scb; mpu_t mpu; dwt_t dwt; stm32_t stm32;
    eth_t eth; uart_t u;
    tt_periph_t p;
    setup(&c, &bus, &p, &st, &nv, &scb, &mpu, &dwt, &stm32, &eth, &u);
    c.r[2] = 0x12345678u;
    ASSERT_TRUE(snap_save(&s_a, &c, &bus, &p));

    const char* path = "snap_rt_test.bin";
    ASSERT_TRUE(snap_save_to_file(&s_a, path));
    ASSERT_TRUE(snap_load_from_file(&s_loaded, path));
    ASSERT_TRUE(memcmp(&s_a, &s_loaded, sizeof s_a) == 0);
    remove(path);
}

/* Corruption: flip one byte -> load_from_file must return false. */
TEST(tt_snap_corrupt_rejected) {
    cpu_t c; bus_t bus;
    systick_t st; nvic_t nv; scb_t scb; mpu_t mpu; dwt_t dwt; stm32_t stm32;
    eth_t eth; uart_t u;
    tt_periph_t p;
    setup(&c, &bus, &p, &st, &nv, &scb, &mpu, &dwt, &stm32, &eth, &u);
    ASSERT_TRUE(snap_save(&s_a, &c, &bus, &p));

    const char* path = "snap_corrupt_test.bin";
    ASSERT_TRUE(snap_save_to_file(&s_a, path));

    FILE* f = fopen(path, "r+b");
    ASSERT_TRUE(f != NULL);
    long mid = (long)(sizeof(snap_blob_t) / 2);
    fseek(f, mid, SEEK_SET);
    u8 g; fread(&g, 1, 1, f); g ^= 0x01u;
    fseek(f, mid, SEEK_SET);
    fwrite(&g, 1, 1, f); fclose(f);

    ASSERT_TRUE(!snap_load_from_file(&s_loaded, path));
    remove(path);
}

int main(void) {
    RUN(tt_snap_double_save_equal);
    RUN(tt_snap_restore_byte_equal);
    RUN(tt_snap_restore_latency);
    RUN(tt_snap_file_roundtrip);
    RUN(tt_snap_corrupt_rejected);
    TEST_REPORT();
}
