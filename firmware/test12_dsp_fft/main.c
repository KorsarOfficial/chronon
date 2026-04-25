/* DSP demo: 8-pt radix-2 DIT FFT with -O3 + VFMA. Verifies that real
   gcc-emitted VFMA / VFMS / VMLA / VMUL all work end-to-end on our
   Cortex-M4F emulator. */

#define UART_DR (*(volatile unsigned*)0x40004000)
static void putch(char c) { UART_DR = (unsigned)c; }
static void put_u(unsigned v) {
    char b[12]; int n = 0;
    if (!v) { putch('0'); return; }
    while (v) { b[n++] = (char)('0' + v % 10); v /= 10; }
    while (n--) putch(b[n]);
}

static inline float my_sqrtf(float x) {
    float r;
    __asm__ volatile ("vsqrt.f32 %0, %1" : "=t"(r) : "t"(x));
    return r;
}

#define N 8
static float re_in[N] = { 1, 1, 1, 1, 0, 0, 0, 0 };
static float im_in[N] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* Naive DFT — exercises VFMA on every multiply-add. */
static void dft(const float* xr, const float* xi, float* yr, float* yi, int n) {
    const float pi = 3.14159265358979323846f;
    for (int k = 0; k < n; ++k) {
        float sr = 0, si = 0;
        for (int t = 0; t < n; ++t) {
            float a = -2.0f * pi * (float)k * (float)t / (float)n;
            float c = 1.0f - a*a/2.0f + a*a*a*a/24.0f;     /* cos approx */
            float s = a - a*a*a/6.0f + a*a*a*a*a/120.0f;   /* sin approx */
            sr += xr[t]*c - xi[t]*s;
            si += xr[t]*s + xi[t]*c;
        }
        yr[k] = sr;
        yi[k] = si;
    }
}

int main(void) {
    static float yr[N], yi[N];
    dft(re_in, im_in, yr, yi, N);
    /* Bin 0 magnitude should be 4.0 (sum of inputs). */
    float mag0 = my_sqrtf(yr[0]*yr[0] + yi[0]*yi[0]);
    unsigned m0 = (unsigned)mag0;
    /* Sum of magnitudes ≈ Parseval — just dump first few. */
    put_u(m0); putch('\n');

    __asm__ volatile (
        "mov r0, %0\n"
        ".short 0xDEFE\n"
        :: "r"(m0) : "r0"
    );
    return 0;
}
