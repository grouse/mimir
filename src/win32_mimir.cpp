#include "platform.h"
#include "core.h"
#include "maths.h"
#include "allocator.h"

#define WIN32_OPENGL_IMPL
#include "win32_opengl.h"

#include "mimir.cpp"


#include "win32_core.cpp"
#include "win32_gui.cpp"
#include "win32_opengl.cpp"
#include "win32_file.cpp"
#include "win32_input.cpp"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x,u)  ((void)(u),malloc(x))
#define STBTT_free(x,u)    ((void)(u),free(x))
#define STBTT_assert(x)    ASSERT(x)
#define STBTT_strlen(x)    strlen(x)
#define STBTT_memcpy       memcpy
#define STBTT_memset       memset
#define STBTT_ifloor(x)   ((int) floor(x))
#define STBTT_iceil(x)    ((int) ceil(x))
#define STBTT_sqrt(x)      sqrt(x)
#define STBTT_fabs(x)     fabs(x)
#include "external/stb/stb_truetype.h"

Allocator mem_frame;
thread_local Allocator mem_tmp;
Allocator mem_dynamic;

struct MouseState {
    i16 x, y;
    i16 dx, dy;
    f32 dwheel;
    bool left_pressed;
    bool left_was_pressed;
    bool middle_pressed;
};

MouseState g_mouse{};

i32 mouse_capture_count = 0;
bool has_init = false;

LRESULT win32_event_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    InputEvent e = win32_input_event(hwnd, message, wparam, lparam);
    if (e.type != 0) app_event(e);
    
    // TODO(jesper): this event loop needs to have a better way to check if the event
    // results in a side effect that requires an update and/or render refresh of the app
    // When app_event is more built it, maybe it should go in there.

    switch (message) {
    case WM_QUIT:
    case WM_CLOSE:
        exit(0);
        break;
    case WM_MOUSEWHEEL:
        g_mouse.dwheel = (i16)((wparam >> 16) & 0xFFFF) / 120;
        app.animating = true;
        break;
    case WM_MOUSEMOVE: 
        g_mouse.x = lparam & 0xFFFF;
        g_mouse.y = (lparam >> 16) & 0xFFFF;
        app.animating = true;
        break;
    case WM_LBUTTONDOWN: 
        g_mouse.left_pressed = true; 
        if (mouse_capture_count++ == 0) SetCapture(hwnd);
        app.animating = true;
        break;
    case WM_LBUTTONUP: 
        g_mouse.left_pressed = false; 
        if (--mouse_capture_count == 0) ReleaseCapture();
        app.animating = true;
        break;
    case WM_MBUTTONDOWN: 
        g_mouse.middle_pressed = true; 
        app.animating = true;
        break;
    case WM_MBUTTONUP: 
        g_mouse.middle_pressed = false; 
        app.animating = true;
        break;
    case WM_SIZING:
    case WM_ENTERSIZEMOVE:
    case WM_EXITSIZEMOVE:
        break;
    case WM_SIZE: {
            // NOTE(jesper): we end up here as soon as the window is created
            if (!has_init) break;
            
            i32 width = lparam & 0xFFFF;
            i32 height = (lparam >> 16) & 0xFFFF;
            
            const char *resize_type = nullptr;
            switch (wparam) {
            case SIZE_MAXHIDE: resize_type = "MAXHIDE"; break;
            case SIZE_MAXIMIZED: resize_type = "MAXIMIZED"; break;
            case SIZE_MAXSHOW: resize_type = "MAXSHOW"; break;
            case SIZE_MINIMIZED: resize_type = "MINIMIZED"; break;
            case SIZE_RESTORED: resize_type = "RESTORED"; break;
            default: resize_type = "unknown";
            }
            
            if (app_change_resolution({ (f32)width, (f32)height })) {
                glViewport(0, 0, width, height);
                
                // TODO(jesper): I don't really understand why I can't just set this to true
                // and let the main loop handle it. It appears as if we get stuck in WM_SIZE message 
                // loop until you stop resizing
                //app.animating = true;

                // TODO(jesper): this feels super iffy. I kind of want some arena push/restore
                // points or something that can be used for local reset of the allocators
                RESET_ALLOC(mem_tmp);
                RESET_ALLOC(mem_frame);
                
                update_and_render(0.0f);
                SwapBuffers(GetDC(hwnd));
            }
        } break;
        
    default:
        return DefWindowProcA(hwnd, message, wparam, lparam);
    }
    
    return 0;
}


int WINAPI wWinMain(
    HINSTANCE hInstance, 
    HINSTANCE hPrevInstance, 
    LPWSTR pCmdLine, 
    int nShowCmd)
{
    init_default_allocators();
    mem_frame = linear_allocator(100*1024*1024);
    
    Vector2 resolution{ 1280, 720 };
    
    Win32Window wnd = create_opengl_window(
        "mimir",
        resolution,
        hInstance,
        &win32_event_proc,
        WS_OVERLAPPEDWINDOW);
    win32_client_rect(wnd.hwnd, &resolution.x, &resolution.y);
    
    init_gfx(resolution);
    init_app();
    
    has_init = true;
    
    Array<String> args{};
    
    // NOTE(jesper): CommandLineToArgvW sets the first argument string to the executable
    // path if and only if pCmdLine is empty
    if (pCmdLine && *pCmdLine != '\0') {
        i32 argv = 0;
        LPWSTR* argc = CommandLineToArgvW(pCmdLine, &argv);


        if (argv > 0) {
            i32 total_utf8_length = 0;
            for (i32 i = 0; i < argv; i++) {
                total_utf8_length += utf8_length(argc[i], wcslen(argc[i]));
            }

            args.data = ALLOC_ARR(mem_dynamic, String, argv);
            args.count = argv;

            char *args_mem = ALLOC_ARR(mem_dynamic, char, total_utf8_length);

            char *p = args_mem;
            for (i32 i = 0; i < argv; i++) {
                i32 l = wcslen(argc[i]);

                args[i].data = p;
                args[i].length = utf8_from_utf16((u8*)p, l, argc[i], l);
                p += l;

                LOG_INFO("arg[%d] = '%.*s'", i, STRFMT(args[i]));
            }
        }
    }
    
    if (args.count > 0) {
        app_open_file(args[0]);
    }
    
    
    LARGE_INTEGER frequency, last_time;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&last_time);
    
    while (true) {
        LARGE_INTEGER current_time;
        QueryPerformanceCounter(&current_time);
        i64 elapsed = current_time.QuadPart - last_time.QuadPart;
        last_time = current_time;

        f32 dt = (f32)elapsed / frequency.QuadPart;
        if (debugger_attached()) dt = MIN(dt, 0.1f);
        
        RESET_ALLOC(mem_tmp);
        RESET_ALLOC(mem_frame);
        
        u16 old_x = g_mouse.x;
        u16 old_y = g_mouse.y;
        g_mouse.left_was_pressed = g_mouse.left_pressed;
        g_mouse.dwheel = 0;
        
        MSG msg;
        while (!app_needs_render() && GetMessageA(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        g_mouse.dx = (i32)g_mouse.x - (i32)old_x;
        g_mouse.dy = (i32)g_mouse.y - (i32)old_y;

        gui.mouse.x = g_mouse.x;
        gui.mouse.y = g_mouse.y;
        gui.mouse.dx = g_mouse.dx;
        gui.mouse.dy = g_mouse.dy;
        gui.mouse.left_pressed = g_mouse.left_pressed;
        gui.mouse.left_was_pressed = g_mouse.left_was_pressed;
        gui.mouse.middle_pressed = g_mouse.middle_pressed;
        
        update_and_render(dt);
        SwapBuffers(wnd.hdc);
    }

    return 0;
}