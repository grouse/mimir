#include "process.h"

struct Process {
    HANDLE process_handle;

    HANDLE stdout_rd;
    HANDLE stdout_thread;

    StdOutProc stdout_proc;
};

Process* create_process(String exe, String args, StdOutProc stdout_proc)
{
    Process *p = ALLOC_T(mem_dynamic, Process);

    if (extension_of(exe) == ".bat") {
        if (args.length > 0) {
            args = stringf(mem_tmp, "/c %.*s %.*s", STRFMT(exe), STRFMT(args));
        } else {
            args = stringf(mem_tmp, "/c %.*s", STRFMT(exe));
        }
        exe = "c:\\windows\\system32\\cmd.exe";
    }

    wchar_t *wsz_exe = wsz_string(exe);
    wchar_t *wsz_args = wsz_string(args);

    HANDLE stdout_wr = NULL;
    defer { if (stdout_wr) CloseHandle(stdout_wr); };

    if (stdout_proc) {
        SECURITY_ATTRIBUTES sa{
            .nLength = sizeof sa,
            .bInheritHandle = false,
        };

        if (!CreatePipe(&p->stdout_rd, &stdout_wr, &sa, 0)) {
            LOG_ERROR("unable to create stdout pipes: (%d) %s", WIN32_ERR_STR);
            FREE(mem_dynamic, p);
            return nullptr;
        }

        if (!SetHandleInformation(stdout_wr, HANDLE_FLAG_INHERIT, 1)) {
            LOG_ERROR("unable to set inherit flag on stdout_rd handle: (%d) %s", WIN32_ERR_STR);
            CloseHandle(p->stdout_rd);
            FREE(mem_dynamic, p);
            return nullptr;
        }
    }

    STARTUPINFOW si{
        .cb = sizeof si,
        .dwFlags = stdout_proc ? STARTF_USESTDHANDLES : 0u,
        .hStdOutput = stdout_proc ? stdout_wr : NULL,
        .hStdError = stdout_proc ? stdout_wr : NULL,
    };

    DWORD flags = CREATE_NO_WINDOW;

    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(wsz_exe, wsz_args, NULL, NULL, true, flags, NULL, NULL, &si, &pi)) {
        CloseHandle(p->stdout_rd);
        LOG_ERROR("unable to create process: '%S' -- '%S': (%d) %s", wsz_exe, wsz_args, WIN32_ERR_STR);
        FREE(mem_dynamic, p);
        return nullptr;
    }

    p->process_handle = pi.hProcess;
    p->stdout_proc = stdout_proc;

    if (p->stdout_proc) {
        p->stdout_thread = CreateThread(
            NULL, 0,
            [](LPVOID thread_data) -> DWORD
            {
                init_thread_allocators();

                Process *p = (Process*)thread_data;

                char buffer[255];
                while (true) {
                    DWORD bytes_read;
                    if (!ReadFile(p->stdout_rd, buffer, sizeof buffer, &bytes_read, NULL) || bytes_read == 0) {
                        break;
                    }

                    p->stdout_proc(String{ buffer, (i32)bytes_read });
                }

                return 0;
            },
            p, 0, NULL);
    }

    return p;
}