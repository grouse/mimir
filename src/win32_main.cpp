#define WIN32_OPENGL_IMPL
#include "win32_opengl.h"
#include "MurmurHash/MurmurHash3.cpp"

#include "core/win32_core.h"

#include "core/array.h"
#include "core/memory.h"
#include "core/string.h"

extern int app_main(Array<String> args);
extern Allocator mem_frame;

int WINAPI WinMain(
    HINSTANCE /*hInstance*/,
    HINSTANCE /*hPrevInstance*/,
    PWSTR pCmdLine,
    int /*nCmdShow*/)
{
    init_default_allocators();
    mem_frame = linear_allocator(10*MiB);

    Array<String> args = win32_commandline_args(pCmdLine, mem_dynamic);

    return app_main(args);
}