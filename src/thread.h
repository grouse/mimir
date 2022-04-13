#ifndef THREAD_H
#define THREAD_H

struct Mutex;
struct Thread;

typedef i32 (*ThreadProc)(void *user_data);

Mutex* create_mutex();
void lock_mutex(Mutex*);
void unlock_mutex(Mutex*);

Thread* create_thread(ThreadProc proc, void *user_data = nullptr);

#endif // THREAD_H
