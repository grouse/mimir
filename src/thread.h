#ifndef THREAD_H
#define THREAD_H

struct Mutex;

Mutex* create_mutex();
void lock_mutex(Mutex*);
void unlock_mutex(Mutex*);

#endif // THREAD_H
