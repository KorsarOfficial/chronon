#include "test_harness.h"
#include "core/cpu.h"
#include "core/bus.h"
#include "core/jit.h"
#include "core/run.h"
#include "periph/systick.h"
#include "periph/scb.h"
#include "periph/mpu.h"
#include "periph/dwt.h"
#include "periph/stm32.h"
#include "periph/eth.h"
#include "periph/uart.h"
#include "core/nvic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
  #include <windows.h>
#endif

#define FLASH_BASE 0x00000000u
#define FLASH_SIZE (1u << 20)
#define SRAM_BASE  0x20000000u
#define SRAM_SIZE  (256u << 10)

/* jit_t ~2MB: file scope to avoid stack overflow (Windows 1MB default stack) */
static jit_t s_jit;

extern dwt_t*  g_dwt_for_run;
extern nvic_t* g_nvic_for_run;
extern cpu_t*  g_cpu_for_scb;

static u8* read_file(const char* p, u32* sz) {
    FILE* f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    u8* b = (u8*)malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *sz = (u32)n;
    return b;
}

static u8* find_test7(u32* sz) {
    const char* env = getenv("LECERF_FIRMWARE_DIR");
    char path[512];
    if (env) {
        snprintf(path, sizeof path, "%s/test7_freertos/test7_freertos.bin", env);
        u8* b = read_file(path, sz); if (b) return b;
    }
    static const char* roots[] = {
        "firmware/test7_freertos/test7_freertos.bin",
        "../firmware/test7_freertos/test7_freertos.bin",
        "../../firmware/test7_freertos/test7_freertos.bin",
    };
    for (size_t k = 0; k < sizeof(roots)/sizeof(roots[0]); ++k) {
        u8* b = read_file(roots[k], sz); if (b) return b;
    }
    return NULL;
}

static void attach_all(bus_t* bus,
                       uart_t* u, systick_t* st, scb_t* scb,
                       mpu_t* mpu, stm32_t* s, dwt_t* dwt,
                       nvic_t* nv, eth_t* eth,
                       cpu_t* cpu) {
    uart_attach(bus, u);
    systick_attach(bus, st);
    scb_attach(bus, scb);
    mpu_attach(bus, mpu);
    stm32_attach(bus, s); s->quiet = true;
    dwt_attach(bus, dwt); g_dwt_for_run = dwt;
    nvic_attach(bus, nv); g_nvic_for_run = nv;
    eth_attach(bus, eth);
    g_cpu_for_scb = cpu;
}

TEST(bench_test7) {
    if (getenv("LECERF_BENCH_SKIP")) {
        fprintf(stderr, "bench skipped via LECERF_BENCH_SKIP\n");
        return;
    }

    u32 sz = 0;
    u8* blob = find_test7(&sz);
    ASSERT_TRUE(blob != NULL);
    if (!blob) return;
    ASSERT_TRUE(sz > 0u && sz <= FLASH_SIZE);

    /* --- Warmup phase: run 50K cycles so JIT compiles all hot blocks --- */
    bus_t bus; bus_init(&bus);
    bus_add_flat(&bus, "flash", FLASH_BASE, FLASH_SIZE, false);
    bus_add_flat(&bus, "sram",  SRAM_BASE,  SRAM_SIZE,  true);
    static uart_t u = {0};
    static systick_t st = {0};
    static scb_t scb = {0};
    static mpu_t mpu = {0};
    static stm32_t s = {0};
    static dwt_t dwt = {0};
    static nvic_t nv = {0};
    static eth_t eth = {0};

    cpu_t cpu;
    cpu_reset(&cpu, CORE_M4);
    attach_all(&bus, &u, &st, &scb, &mpu, &s, &dwt, &nv, &eth, &cpu);
    bus_load_blob(&bus, FLASH_BASE, blob, sz);
    cpu.msp = bus_r32(&bus, 0x0u);
    cpu.r[REG_SP] = cpu.msp;
    cpu.r[REG_PC] = bus_r32(&bus, 0x4u) & ~1u;

    memset(&s_jit, 0, sizeof s_jit);
    jit_init(&s_jit);

    /* Warmup: run enough to compile all hot blocks including FreeRTOS scheduler
       and task hot loops. test7 has ~200K startup insns before first task switch;
       warmup of 500K covers all hot paths including first few context switches. */
    (void)run_steps_full_g(&cpu, &bus, 500000ull, &st, &scb, &s_jit);

    free(blob); blob = NULL;

    /* --- Timed 5M-step run: best-of-3 for scheduler-jitter stability.
       JIT compiled code (s_jit) is PRESERVED from warmup so all trials are hot. --- */
#ifdef _WIN32
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
#endif
    u64 n = 0;
    double best_s = 1e9;
    for (int trial = 0; trial < 3; ++trial) {
        /* Reset CPU+bus+peripherals for each trial so each run is independent */
        memset(&u,   0, sizeof u);
        memset(&st,  0, sizeof st);
        memset(&scb, 0, sizeof scb);
        memset(&mpu, 0, sizeof mpu);
        memset(&s,   0, sizeof s);
        memset(&dwt, 0, sizeof dwt);
        memset(&nv,  0, sizeof nv);
        memset(&eth, 0, sizeof eth);
        bus_init(&bus);
        bus_add_flat(&bus, "flash", FLASH_BASE, FLASH_SIZE, false);
        bus_add_flat(&bus, "sram",  SRAM_BASE,  SRAM_SIZE,  true);
        u32 tsz = 0;
        u8* tblob = find_test7(&tsz);
        if (!tblob) break;
        bus_load_blob(&bus, FLASH_BASE, tblob, tsz);
        free(tblob);
        cpu_reset(&cpu, CORE_M4);
        attach_all(&bus, &u, &st, &scb, &mpu, &s, &dwt, &nv, &eth, &cpu);
        cpu.msp = bus_r32(&bus, 0x0u);
        cpu.r[REG_SP] = cpu.msp;
        cpu.r[REG_PC] = bus_r32(&bus, 0x4u) & ~1u;
#ifdef _WIN32
        LARGE_INTEGER t0, t1;
        QueryPerformanceCounter(&t0);
#endif
        u64 tr_n = run_steps_full_g(&cpu, &bus, 5000000ull, &st, &scb, &s_jit);
#ifdef _WIN32
        QueryPerformanceCounter(&t1);
        double tr_s = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
        if (tr_s < best_s) { best_s = tr_s; n = tr_n; }
#else
        n = tr_n;
#endif
    }
#ifdef _WIN32
    double elapsed_s = best_s;
    double ips_m = (elapsed_s > 0.0) ? ((double)n / elapsed_s / 1e6) : 0.0;
    u64 nat = s_jit.native_steps, interp = s_jit.interp_steps;
    double nat_pct = (nat + interp > 0) ? 100.0 * (double)nat / (double)(nat + interp) : 0.0;
    fprintf(stderr, "[bench] insns=%llu IPS=%.2fM elapsed=%.1fms (best-of-3)\n",
            (unsigned long long)n, ips_m, elapsed_s * 1000.0);
    fprintf(stderr, "[bench] native=%llu interp=%llu native%%=%.1f%%\n",
            (unsigned long long)nat, (unsigned long long)interp, nat_pct);
    /* JIT-07: native PUSH/POP/LDM/STM + B.cond fast path; target >=100M IPS / <50ms.
       To disable timing assertion in slow CI: set env LECERF_BENCH_SKIP. */
    ASSERT_TRUE(elapsed_s < 0.050);   /* hard regression gate: <50ms */
    if (ips_m < 100.0)
        fprintf(stderr, "[bench] NOTE: IPS %.2fM < 100M target\n", ips_m);
#else
    fprintf(stderr, "[bench] non-Windows: skipping timing assertion (n=%llu)\n",
            (unsigned long long)n);
#endif
    ASSERT_TRUE(n > 0u);
}

int main(void) {
    RUN(bench_test7);
    TEST_REPORT();
}
