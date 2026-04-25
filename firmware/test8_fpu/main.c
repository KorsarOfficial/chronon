/* FPU test: float arithmetic. */
static inline float my_sqrtf(float x) {
    float r;
    __asm__ volatile ("vsqrt.f32 %0, %1" : "=t"(r) : "t"(x));
    return r;
}
#define __builtin_sqrtf(x) my_sqrtf(x)

#define UART_DR (*(volatile unsigned*)0x40004000)
static void putch(char c) { UART_DR = (unsigned)c; }
static void put_u(unsigned v) {
    char b[12]; int n = 0;
    if (!v) { putch('0'); return; }
    while (v) { b[n++] = (char)('0' + v % 10); v /= 10; }
    while (n--) putch(b[n]);
}

static volatile float a = 3.0f;
static volatile float b = 4.0f;

int main(void) {
    float x = a;
    float y = b;
    float hyp = __builtin_sqrtf(x*x + y*y);  /* VSQRT, VMUL, VADD */
    float area = (x * y) / 2.0f;             /* VMUL, VDIV */
    float diff = y - x;                       /* VSUB */
    float neg = -diff;                        /* VNEG */
    float abs_neg = neg < 0 ? -neg : neg;     /* VABS via branch */

    /* Cast hyp to int via VCVT, but skip for simplicity — multiply and store. */
    unsigned hyp_int  = (unsigned)hyp;        /* expect 5 */
    unsigned area_int = (unsigned)area;       /* expect 6 */
    unsigned diff_int = (unsigned)abs_neg;    /* expect 1 */

    put_u(hyp_int);  putch(' ');
    put_u(area_int); putch(' ');
    put_u(diff_int); putch('\n');

    __asm__ volatile (
        "mov r0, %0\n"
        "mov r1, %1\n"
        "mov r2, %2\n"
        ".short 0xDEFE\n"
        :: "r"(hyp_int), "r"(area_int), "r"(diff_int)
        : "r0","r1","r2"
    );
    return 0;
}
