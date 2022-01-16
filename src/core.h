#ifndef CORE_H
#define CORE_H

#include "string.h"

extern "C" void* malloc(size_t size);
extern "C" void free(void* ptr);
extern "C" void* realloc(void* ptr, size_t size);

extern "C" void exit(int status);

extern "C" size_t strlen(const char * str);
extern "C" size_t wcslen(const wchar_t* wcs);
extern "C" int strcmp(const char * str1, const char * str2);
extern "C" int strncmp(const char * str1, const char * str2, size_t num);

extern "C" void* memcpy(void *destination, const void *source, size_t num);
extern "C" void* memset(void *ptr, int value, size_t num);
extern "C" int memcmp(const void *ptr1, const void *ptr2, size_t num);
extern "C" void* memmove(void *destination, const void *source, size_t num);

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MAX3(a, b, c) MAX(MAX(a, b), c)
#define MAX4(a, b, c, d) MAX(MAX(a, b), MAX(c, d))

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MIN4(a, b, c, d) MIN(MIN(a, b), MIN(c, d))

#define CLAMP(v, min, max) ((v) < (min) ? (min) : (v) > (max) ? (max) : (v));
#define SWAP(a, b) do { auto tmp = a; a = b; b = tmp; } while(0)
#define ARRAY_COUNT(arr) (i32)(sizeof(arr) / sizeof ((arr)[0]))

#define ARGS(...) , ##__VA_ARGS__

#define RMOV(...) static_cast<std::remove_reference_t<decltype(__VA_ARGS__)>&&>(__VA_ARGS__)
#define RFWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

template <typename F>
struct Defer {
    Defer(F f) : f(f) {}
    ~Defer() { f(); }
    F f;
};

template <typename F>
Defer<F> defer_create( F f ) {
    return Defer<F>( f );
}

#define defer__(line) defer_ ## line
#define defer_(line) defer__( line )

struct DeferDummy { };
template<typename F>
Defer<F> operator+ (DeferDummy, F&& f)
{
    return defer_create<F>(RFWD(f));
}

#define defer auto defer_( __LINE__ ) = DeferDummy( ) + [&]( )


#define ASSERT(cond)\
    do {\
        if (!(cond)) {\
            log(String{ (char*)__FILE__, (sizeof __FILE__)-1 }, __LINE__, __FUNCTION__, LOG_TYPE_ASSERT, "%s", #cond);\
            DEBUG_BREAK();\
        }\
    } while(0)

#define PANIC(...)\
    do {\
        log(String{ (char*)__FILE__, (sizeof __FILE__)-1 }, __LINE__, __FUNCTION__, LOG_TYPE_PANIC, __VA_ARGS__);\
        DEBUG_BREAK();\
    } while(0)

#define PANIC_IF(cond, ...)\
    if (cond) PANIC(__VA_ARGS__)

#define LOG_ERROR(...)\
    do {\
        log(String{ (char*)__FILE__, (sizeof __FILE__)-1 }, __LINE__, __FUNCTION__, LOG_TYPE_ERROR, __VA_ARGS__);\
        if (debugger_attached()) DEBUG_BREAK();\
    } while(0)

#define LOG_INFO(...) log(String{ (char*)__FILE__, (sizeof __FILE__)-1 }, __LINE__, __FUNCTION__, LOG_TYPE_INFO, __VA_ARGS__)
#define LOG_RAW(...) log(__VA_ARGS__)

enum LogType {
    LOG_TYPE_PANIC,
    LOG_TYPE_ERROR,
    LOG_TYPE_INFO,
    LOG_TYPE_ASSERT
};

bool debugger_attached();

void set_clipboard_data(String str);
String read_clipboard_str(Allocator mem = mem_tmp);

String get_exe_folder(Allocator mem = mem_tmp);

void log(String path, u32 line, String func, LogType type, const char *fmt, ...);
void log(const char *fmt, ...);

#endif // CORE_H