
i32 get_page_size()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
}

void* virtual_reserve(i64 size)
{
    void *mem = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
    return mem;
}

void* virtual_commit(void *addr, i64 size)
{
	void *ptr = VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE);
	return ptr;
}


