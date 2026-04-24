#ifndef CORTEX_M_TYPES_H
#define CORTEX_M_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef u32 addr_t;

#if defined(__GNUC__) || defined(__clang__)
  #define FORCE_INLINE static inline __attribute__((always_inline))
  #define LIKELY(x)   __builtin_expect(!!(x), 1)
  #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#elif defined(_MSC_VER)
  #define FORCE_INLINE static __forceinline
  #define LIKELY(x)   (x)
  #define UNLIKELY(x) (x)
#else
  #define FORCE_INLINE static inline
  #define LIKELY(x)   (x)
  #define UNLIKELY(x) (x)
#endif

#endif
