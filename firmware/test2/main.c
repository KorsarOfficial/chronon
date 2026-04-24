/* Bubble sort + sum of sorted array. Tests memory + recursion-like loop nesting. */

static int arr[8] = { 7, 3, 1, 9, 2, 8, 4, 6 };

static void bubble_sort(int* a, int n) {
    for (int i = 0; i < n - 1; ++i) {
        for (int j = 0; j < n - 1 - i; ++j) {
            if (a[j] > a[j+1]) {
                int t = a[j]; a[j] = a[j+1]; a[j+1] = t;
            }
        }
    }
}

static int sum(int* a, int n) {
    int s = 0;
    for (int i = 0; i < n; ++i) s += a[i];
    return s;
}

/* Recursive factorial — exercises stack, BL, BX LR. */
static int fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}

int main(void) {
    bubble_sort(arr, 8);
    int s = sum(arr, 8);              /* 1+2+3+4+6+7+8+9 = 40 */
    int f = fact(6);                  /* 720 */
    int result = (s << 16) | f;       /* 0x002802D0 */

    __asm__ volatile (
        "mov r0, %0\n"
        "mov r1, %1\n"
        "mov r2, %2\n"
        ".short 0xDEFE\n"
        :
        : "r"(result), "r"(arr[0]), "r"(arr[7])
        : "r0", "r1", "r2"
    );
    return 0;
}
