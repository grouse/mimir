#define WIN32_OPENGL_IMPL
#include "MurmurHash/MurmurHash3.cpp"

#include "core/win32_core.h"
#include "core/win32_opengl.h"

#include "core/array.h"
#include "core/memory.h"
#include "core/string.h"

extern int app_main(Array<String> args);
extern Allocator mem_frame;

int WINAPI wWinMain(
    HINSTANCE /*hInstance*/,
    HINSTANCE /*hPrevInstance*/,
    PWSTR /*pCmdLine*/,
    int /*nCmdShow*/)
{
    init_default_allocators();
    mem_frame = linear_allocator(10*MiB);

    Array<String> args = win32_commandline_args(mem_dynamic);

    return app_main(args);
}
