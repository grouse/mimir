#include "thread.h"

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

