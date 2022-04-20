#include "linux_opengl.h"
#include "mimir.cpp"

#include "linux_core.cpp"
#include "linux_opengl.cpp"
#include "linux_clipboard.cpp"
#include "linux_input.cpp"
#include "linux_process.cpp"
#include "linux_thread.cpp"
#include "linux_file.cpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x) ASSERT(x)
#include "external/stb/stb_image.h"

#include "external/MurmurHash/MurmurHash3.cpp"

Allocator mem_frame;

int main(int argc, char **argv)
{
	init_default_allocators();
	mem_frame = linear_allocator(100*1024*1024);
	init_core(argc, argv);


    Vector2 resolution{ 1024, 720 };

    LinuxWindow wnd = create_opengl_window("plumber", resolution);
    load_opengl_procs();
    init_clipboard(wnd.dsp, wnd.handle);

	Array<String> args{};
	if (argc > 1) {
		array_create(&args, argc-1);
		for (i32 i = 1; i < argc; i++) {
			args[i-1] = String{ argv[i], (i32)strlen(argv[i]) };
		}
	}

    init_gfx(resolution);
    init_app(args);

    timespec last_time = monotonic_time();
    
    DynamicArray<InputEvent> input_stream{};
    array_reserve(&input_stream, 2);

    while (true) {
        timespec current_time = monotonic_time();
        i64 elapsed = time_difference(last_time, current_time);
        last_time = current_time;

        f32 dt = (i32)elapsed / 1000000000.0f;

        RESET_ALLOC(mem_tmp);
        RESET_ALLOC(mem_frame);

        g_mouse.left_was_pressed = g_mouse.left_pressed;
        g_mouse.dwheel = 0;

        XEvent event;
        while (XPending(wnd.dsp)) {
            XNextEvent(wnd.dsp, &event);

			input_stream.count = 0;
			linux_input_event(&input_stream, &wnd, event);
			for (InputEvent e : input_stream) app_event(e);

            switch (event.type) {
            case PropertyNotify: {
                    char *name = XGetAtomName(wnd.dsp, event.xproperty.atom);
                    if (name) {
                        LOG_INFO(
                            "window %p: PropertyNotify: %s %s time=%lu",
                            wnd, name,
                            (event.xproperty.state == PropertyDelete) ? "deleted" : "changed",
                            event.xproperty.time);
                        XFree(name);
                    }
                } break;
            case MotionNotify: {
                    g_mouse.dx = event.xmotion.x - g_mouse.x;
                    g_mouse.dy = event.xmotion.y - g_mouse.y;
                    g_mouse.x = event.xmotion.x;
                    g_mouse.y = event.xmotion.y;
                } break;
            case ButtonPress:
                if (event.xbutton.button == 1) {
                    g_mouse.left_pressed = true;
                }
                break;
            case ButtonRelease:
                if (event.xbutton.button == 1) {
                    g_mouse.left_pressed = false;
                }
                break;
            }
        }

        gui.mouse.x = g_mouse.x;
        gui.mouse.y = g_mouse.y;
        gui.mouse.dx = g_mouse.dx;
        gui.mouse.dy = g_mouse.dy;
        gui.mouse.left_pressed = g_mouse.left_pressed;
        gui.mouse.left_was_pressed = g_mouse.left_was_pressed;
        gui.mouse.middle_pressed = g_mouse.middle_pressed;

        update_and_render(dt);
        glXSwapBuffers(wnd.dsp, wnd.handle);

    }

    return 0;
}


