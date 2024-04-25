#include "linux_opengl.h"
#include "mimir.cpp"

#include "linux_process.cpp"

#include "external/MurmurHash/MurmurHash3.cpp"

#include <unistd.h>

Allocator mem_frame;
String exe_path;

int main(int argc, char **argv)
{
	init_default_allocators();
	mem_frame = linear_allocator(100*1024*1024);

	char *p = last_of(argv[0], '/');
	if (argv[0][0] == '/') {
		exe_path = { argv[0], (i32)(p-argv[0]) };
	} else {
		char buffer[PATH_MAX];
		char *wd = getcwd(buffer, sizeof buffer);
		PANIC_IF(wd == nullptr, "current working dir exceeds PATH_MAX");

		exe_path = join_path(
			{ wd, (i32)strlen(wd) },
			{ argv[0], (i32)(p-argv[0]) },
			mem_dynamic);
	}

    Vector2 resolution{ 1024, 720 };
    AppWindow *wnd = create_window({"plumber", i32(resolution.x), i32(resolution.y) });

	Array<String> args{};
	if (argc > 1) {
		array_create(&args, argc-1);
		for (i32 i = 1; i < argc; i++) {
			args[i-1] = String{ argv[i], (i32)strlen(argv[i]) };
		}
	}

    init_app(args);

    u64 last_time, current_time = wall_timestamp();

    DynamicArray<WindowEvent> input_stream{};
    array_reserve(&input_stream, 2);

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
