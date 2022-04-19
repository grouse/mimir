static u16 key_state[256] = {0};


bool debugger_attached()
{
    return true;
}

void log_key_event(XKeyEvent event)
{
    LOG_INFO(
        "keyrelease: 0x%x, shift: %d, ctrl: %d, lock: %d, mod1: %d, mod2: %d, mod3: %d, mod4: %d, mod5: %d",
        event.keycode,
        event.state & ShiftMask ? 1 : 0,
        event.state & ControlMask ? 1 : 0,
        event.state & LockMask ? 1 : 0,
        event.state & Mod1Mask ? 1 : 0, // alt
        event.state & Mod2Mask ? 1 : 0, // no idea
        event.state & Mod3Mask ? 1 : 0, // no idea
        event.state & Mod4Mask ? 1 : 0, // no idea
        event.state & Mod5Mask ? 1 : 0); // no idea
}

void log(String path, u32 line, String proc, LogType type, const char *fmt, ...)
{
    char buffer0[1024];
    char *msg = buffer0;

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

    constexpr const char* LOG_FMT = "%.*s:%d in %.*s: %s %s\n";
    String filename = filename_of(path);
    printf(LOG_FMT, STRFMT(filename), line, STRFMT(proc), log_type_str, msg);
    fflush(stdout);
}

void log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    vprintf(fmt, args);

    va_end(args);
    fflush(stdout);

}

timespec monotonic_time()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return ts;
}

i64 time_difference(timespec start, timespec end)
{
	return (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
}


u64 file_modified_timestamp(String path)
{
    char *sz_path = sz_string(path);

    struct stat st;
    i32 result = stat(sz_path, &st);

    if (result != 0) {
        LOG_ERROR("couldn't stat file: %s", sz_path);
        return -1;
    }

    return st.st_mtim.tv_sec;
}

u64 wall_timestamp()
{
    timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_sec;
}

f32 app_time_s()
{
    static u64 start_time = wall_timestamp();
    u64 current = wall_timestamp();
    return (f32)(current-start_time);
}

struct {
	String exe_path;
} core;

void init_core(int argc, char **argv)
{
	ASSERT(argc > 0);

	char *p = last_of(argv[0], '/');
	if (argv[0][0] == '/') {
		core.exe_path = { argv[0], (i32)(p-argv[0]) };
	} else {
		char buffer[PATH_MAX];
		char *wd = getcwd(buffer, sizeof buffer);
		PANIC_IF(wd == nullptr, "current working dir exceeds PATH_MAX");

		core.exe_path = join_path(
			{ wd, (i32)strlen(wd) },
			{ argv[0], (i32)(p-argv[0]) },
			mem_dynamic);
	}
}
