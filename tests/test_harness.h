#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>

static int _g_fail = 0;
static int _g_tests = 0;

#define TEST(name) static void name(void)

#define ASSERT_EQ_U32(a, b) do { \
    _g_tests++; \
    u32 _a = (u32)(a), _b = (u32)(b); \
    if (_a != _b) { \
        fprintf(stderr, "FAIL %s:%d %s: expected 0x%08x got 0x%08x\n", \
                __FILE__, __LINE__, #a " == " #b, _b, _a); \
        _g_fail++; \
    } \
} while (0)

#define ASSERT_TRUE(x) do { \
    _g_tests++; \
    if (!(x)) { \
        fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x); \
        _g_fail++; \
    } \
} while (0)

#define RUN(fn) do { fn(); } while (0)

#define TEST_REPORT() do { \
    fprintf(stderr, "%d/%d passed\n", _g_tests - _g_fail, _g_tests); \
    return _g_fail ? 1 : 0; \
} while (0)

#endif
