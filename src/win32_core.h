#ifndef WIN32_CORE_H
#define WIN32_CORE_H

#include "win32_lite.h"
#include "array.h"

#define WIN32_ERR_STR GetLastError(), win32_system_error_message(GetLastError())

char* win32_system_error_message(DWORD error);
void win32_client_rect(HWND hwnd, f32 *x, f32 *y);

const char* string_from_file_attribute(DWORD dwFileAttribute);
Array<String> win32_commandline_args(LPWSTR pCmdLine, Allocator mem = mem_dynamic);

#endif // WIN32_CORE_H
