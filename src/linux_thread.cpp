#include <pthread.h>

Mutex* create_mutex()
{
	pthread_mutex_t *mutex = ALLOC_T(mem_dynamic, pthread_mutex_t);

	int r = pthread_mutex_init(mutex, nullptr);
	PANIC_IF(r != 0, "failed to create mutex, errno: %d", errno);

	return (Mutex*)mutex;
}

void lock_mutex(Mutex *m)
{
	pthread_mutex_t *mutex = (pthread_mutex_t*)m;
	int r = pthread_mutex_lock(mutex);
	PANIC_IF(r != 0, "failed to lock mutex, errno: %d", errno);
}

void unlock_mutex(Mutex *m)
{
	pthread_mutex_t *mutex = (pthread_mutex_t*)m;
	int r = pthread_mutex_unlock(mutex);
	PANIC_IF(r != 0, "failed to lock mutex, errno: %d", errno);
}
