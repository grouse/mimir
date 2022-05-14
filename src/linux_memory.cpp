#include <sys/mman.h>
#include <unistd.h>

i32 get_page_size()
{
	int page_size = getpagesize();
	return page_size;
}

void* virtual_reserve(i64 size)
{
    void *mem = mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
    PANIC_IF(mem == MAP_FAILED, "failed to mmap, errno: %d", errno);
    return mem;
}

void* virtual_commit(void *addr, i64 size)
{
	(void)size;
	return addr;
}
