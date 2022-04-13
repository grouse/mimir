#include "thread.h"

struct Thread {
    HANDLE handle;
    ThreadProc user_proc;
    void *user_data;
};

Mutex* create_mutex()
{
    HANDLE h = CreateMutexA(NULL, false, NULL);
    return (Mutex*)h;
}

void lock_mutex(Mutex *m)
{
    HANDLE h = (HANDLE)m;
    WaitForSingleObject(h, TIMEOUT_INFINITE);
}

void unlock_mutex(Mutex *m)
{
    HANDLE h = (HANDLE)m;
    ReleaseMutex(h);
}

Thread* create_thread(ThreadProc proc, void *user_data)
{
    Thread *t = ALLOC_T(mem_dynamic, Thread);
    t->user_proc = proc;
    t->user_data = user_data;
    
    t->handle = CreateThread(
        NULL, 0,
        [](LPVOID data) -> DWORD
        {
            Thread *t = (Thread*)data;
            init_thread_allocators();
            
            return t->user_proc(t->user_data);
        },
        t, 0, NULL);

    return t;
}
    