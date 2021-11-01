/* date = April 9th 2021 7:57 pm */

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdint.h>

#define internal static
#define global_var static
#define local_persist static

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;

typedef u32 b32;
typedef u8 b8;

typedef float f32;
typedef double f64;

#define ArrayCount(x) (sizeof(x)/sizeof((x)[0]))
#define Max(a, b) ((a) < (b)) ? b : a;
#define Min(a, b) ((a) < (b)) ? a : b;

#define Kilobyte(x) ((x)*1024LL)
#define Megabyte(x) (Kilobyte(x)*1024LL)
#define Gigabyte(x) (Megabyte(x)*1024LL)
#define Terabyte(x) (Gigabyte(x)*1024LL)

#endif //UTILS_H
