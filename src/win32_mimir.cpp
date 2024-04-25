#define WIN32_OPENGL_IMPL
#include "win32_opengl.h"

#include "mimir.cpp"
#include "window.h"
#include "gen/window.h"

#include "win32_process.cpp"

#include "external/MurmurHash/MurmurHash3.cpp"

Allocator mem_frame;

i32 mouse_capture_count = 0;
bool has_init = false;

int WINAPI wWinMain(
    HINSTANCE /*hInstance*/,
    HINSTANCE /*hPrevInstance*/,
    LPWSTR pCmdLine,
    int /*nShowCmd*/)
{
    init_default_allocators();
    mem_frame = linear_allocator(100*1024*1024);

    Vector2 resolution{ 1280, 720 };

    AppWindow *wnd = create_window({"mimir", i32(resolution.x), i32(resolution.y), WINDOW_OPENGL });

    extern HWND win32_root_window;
    extern HWND win32_window_handle(AppWindow *wnd);
    win32_root_window = win32_window_handle(wnd);

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

    init_app(args);
    has_init = true;

    u64 last_time, current_time = wall_timestamp();

    while (true) {
        RESET_ALLOC(mem_frame);

        last_time = current_time;
        current_time = wall_timestamp();
        f32 dt = wall_duration_s(last_time, current_time);

        (void)dt;

        app_gather_input(wnd);
        update_and_render();
        present_window(wnd);
    }

    return 0;
}
