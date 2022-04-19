#ifndef PLATFORM_H
#define PLATFORM_H

#if defined(__clang__)

#define PACKED __attribute__((__packed__))

#else
#error "unsupported compiler"
#endif

#if defined(__linux__)
#define NOTHROW __attribute__(( __nothrow__ __LEAF))

typedef unsigned long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8 ;

typedef signed long i64;
typedef signed int i32;
typedef signed short i16;
typedef signed char i8 ;

typedef float f32;
typedef double f64;

#include <stdarg.h>
#include <stdio.h> // TODO(jesper): only needed because vsnprintf is weird

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <dlfcn.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#define DEBUG_BREAK() asm("int $3")

#define atomic_exchange(var, value) __sync_lock_test_and_set(var, value)

extern "C" char *strerror(int errnum) NOTHROW;

#elif defined(_WIN32)
#define NOTHROW

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8 ;

typedef signed long long i64;
typedef signed int i32;
typedef signed short i16;
typedef signed char i8 ;

typedef float f32;
typedef double f64;

#include <stdarg.h>
#include <stdio.h> // TODO(jesper): only needed because vsnprintf is weird

#define DEBUG_BREAK() __debugbreak()

#else
#error "unsupported platform"
#endif

constexpr f64 f64_PI = 3.1415926535897932384626433832795028841971693993751058209749445923078164062;
constexpr f32 f32_PI = (f32)f64_PI;

#define f32_MAX 3.402823466e+38F
#define f32_INF ((f32)(1e+300*1e+300))

#define i32_MAX (i32)0x7FFFFFFF
#define u32_MAX (u32)0xFFFFFFFF

#define i16_MAX (i16)0x7FFF
#define u16_MAX (u16)0xFFFF


static_assert(sizeof(u64) == 8);
static_assert(sizeof(i64) == 8);
static_assert(sizeof(u32) == 4);
static_assert(sizeof(i32) == 4);
static_assert(sizeof(u16) == 2);
static_assert(sizeof(i16) == 2);
static_assert(sizeof(u8) == 1);
static_assert(sizeof(i8) == 1);
static_assert(sizeof(f32) == 4);
static_assert(sizeof(f64) == 8);

#endif // PLATFORM_H
