#include "win32_core.h"

#include "core.h"


bool debugger_attached()
{
    return IsDebuggerPresent();
}

void log(String path, u32 line, String func, LogType type, const char *fmt, ...)
{
    char buffer0[1024];
    char buffer1[1024];

    char *msg = buffer0;
    char *final = buffer1;

    va_list args;
    va_start(args, fmt);

    i32 length = vsnprintf(buffer0, sizeof buffer0-1, fmt, args);
    if (length >= (i32)sizeof buffer0) {
        msg = (char*)ALLOC(mem_tmp, length);
        vsnprintf(msg, length, fmt, args);
    }

    va_end(args);

    const char *log_type_str = nullptr;
    switch (type) {
    case LOG_TYPE_ASSERT:
        log_type_str = "assertion failed:";
        break;
    case LOG_TYPE_ERROR:
        log_type_str = "error:";
        break;
    case LOG_TYPE_PANIC:
        log_type_str = "panic!";
        break;
    case LOG_TYPE_INFO:
        log_type_str = "info:";
        break;
    }

    // NOTE(jesper): this is basically just to appease the jump buffer parsing in 4coder, but it was possible to grab this 
    // number from the source caller somehow
    i32 column = 0;

    constexpr const char* LOG_FMT = "%.*s:%d:%d in %.*s: %s %s\n";
    String filename = filename_of(path);
    length = snprintf(buffer1, sizeof buffer1-1, LOG_FMT, STRFMT(filename), line, column, STRFMT(func), log_type_str, msg);
    if (length >= (i32)sizeof buffer1) {
        final = (char*)ALLOC(mem_tmp, length);
        snprintf(final, length, LOG_FMT, STRFMT(filename), line, column, STRFMT(func), log_type_str, msg);
    }

    if (debugger_attached()) {
        OutputDebugStringA(final);
    } else { 
        printf("%s", final);
    }
}

void log(const char *fmt, ...)
{
    char buffer[1024];
    char *msg = buffer;

    va_list args;
    va_start(args, fmt);

    i32 length = vsnprintf(buffer, sizeof buffer-1, fmt, args);
    if (length >= (i32)sizeof buffer) {
        msg = (char*)ALLOC(mem_tmp, length);
        vsnprintf(msg, length, fmt, args);
    }

    va_end(args);
    if (IsDebuggerPresent()) {
        OutputDebugStringA(msg);
    } else { 
        printf("%s", msg);
    }
}

u64 time_counter_frequency()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

u64 wall_time_counter()
{
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time.QuadPart;
}

f32 app_time_s()
{
    static u64 start_time = wall_time_counter();
    static u64 frequency = time_counter_frequency();

    u64 time = wall_time_counter();
    return (f64)(time - start_time) / frequency;
}

u64 time_counter_from_s(f32 s)
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return (u64)(s*frequency.QuadPart);
}

f32 time_counter_duration_s(u64 start, u64 current)
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return (f32)(current-start) / frequency.QuadPart;
}

void set_clipboard_data(String str)
{
    if (!OpenClipboard(NULL)) return;
    defer { CloseClipboard(); };

    if (!EmptyClipboard()) return;

    i32 utf16_len = utf16_length(str);
    HANDLE clip_mem = GlobalAlloc(GMEM_MOVEABLE, (utf16_len+1) * sizeof(u16));
    defer { GlobalUnlock(clip_mem); };

    u16 *clip_str = (u16*)GlobalLock(clip_mem);
    utf16_from_string(clip_str, utf16_len, str);
    clip_str[utf16_len] = '\0';

    if (!SetClipboardData(CF_UNICODETEXT, clip_mem)) return;
}

const char* string_from_file_attribute(DWORD dwFileAttribute)
{
    switch (dwFileAttribute) {
    case FILE_ATTRIBUTE_NORMAL: return "FILE_ATTRIBUTE_NORMAL";
    case FILE_ATTRIBUTE_DIRECTORY: return "FILE_ATTRIBUTE_DIRECTORY";
    case FILE_ATTRIBUTE_ARCHIVE: return "FILE_ATTRIBUTE_ARCHIVE";
    default: 
        LOG_ERROR("unknown file attribute: 0x%x", dwFileAttribute);
        return "unknown";
    }
}

char* win32_system_error_message(DWORD error)
{
    static char buffer[2048];
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        error,
        0,
        buffer,
        sizeof buffer,
        NULL);

    return buffer;
}

void win32_client_rect(HWND hwnd, f32 *x, f32 *y)
{
    ASSERT(x != nullptr);
    ASSERT(y != nullptr);

    RECT client_rect;
    if (GetClientRect(hwnd, &client_rect)) {
        *x = (f32)(client_rect.right - client_rect.left);
        *y = (f32)(client_rect.bottom - client_rect.top);
    }
}