#include "file.h"

String absolute_path(String relative, Allocator mem)
{
    const char *sz_relative = sz_string(relative);

    // NOTE(jesper): size is total size required including null terminator
    DWORD size = GetFullPathNameA(sz_relative, 0, NULL, NULL);
    char *pstr = ALLOC_ARR(mem, char, size);
    
    // NOTE(jesper): length is size excluding null terminator
    DWORD length = GetFullPathNameA(sz_relative, size, pstr, NULL);
    return String{ pstr, (i32)length };
}

FileInfo read_file(String path, Allocator mem, i32 retry_count)
{
    FileInfo fi{};
    
    char *sz_path = sz_string(path);

    // TODO(jesper): when is this actually needed? read up on win32 file path docs
    char *ptr = sz_path;
    char *ptr_end = ptr + path.length;
    while (ptr < ptr_end) {
        if (*ptr == '/') {
            *ptr = '\\';
        }
        ptr++;
    }

    HANDLE file = CreateFileA(
        sz_path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    
    if (file == INVALID_HANDLE_VALUE && GetLastError() == ERROR_SHARING_VIOLATION) {
        for (i32 i = 0; file == INVALID_HANDLE_VALUE && i < retry_count; i++) {
            LOG_INFO("failed opening file '%s' due to sharing violation, sleeping and retrying", sz_path);
            
            Sleep(5);
            file = CreateFileA(
                sz_path,
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
        }
    }
    
    if (file == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND) {
        LOG_INFO("file not found: %s", sz_path);
        return {};
    }

    if (file == INVALID_HANDLE_VALUE) {
        LOG_ERROR("failed to open file '%s': (%d) %s", sz_path, WIN32_ERR_STR);
        return {};
    }
    defer { CloseHandle(file); };

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size)) {
        LOG_ERROR("failed getting file size for file '%s': (%d) %s", sz_path, WIN32_ERR_STR);
        return {};
    }

    PANIC_IF(
        file_size.QuadPart >= 0x7FFFFFFF, 
        "error opening file '%s': file size (%lld bytes) exceeds maximum supported %d", sz_path, file_size.QuadPart, 0x7FFFFFFF);

    fi.size = file_size.QuadPart;
    fi.data = (u8*)ALLOC(mem, file_size.QuadPart);

    DWORD bytes_read;
    if (!ReadFile(file, fi.data, (DWORD)file_size.QuadPart, &bytes_read, 0)) {
        LOG_ERROR("failed reading file '%s': (%d) %s", sz_path, WIN32_ERR_STR);
        FREE(mem, fi.data);
        return {};
    }

    return fi;
}

HANDLE win32_open_file(char *sz_path, u32 creation_mode, u32 access_mode)
{
    HANDLE file = CreateFileA(
        sz_path,
        access_mode,
        0,
        nullptr,
        creation_mode,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file == INVALID_HANDLE_VALUE && 
        (creation_mode == CREATE_ALWAYS ||
         creation_mode == CREATE_NEW))
    {
        DWORD code = GetLastError();
        if (code == 3) {
            char *ptr = sz_path;
            char *end = ptr + strlen(ptr);

            while (ptr < end) {
                if (*ptr == '\\' || *ptr == '/') {
                    char c = *ptr;
                    *ptr = '\0';
                    defer{ *ptr = c; };

                    if (CreateDirectoryA(sz_path, NULL) == 0) {
                        DWORD create_dir_error = GetLastError();
                        if (create_dir_error != ERROR_ALREADY_EXISTS) {
                            LOG_ERROR("failed creating folder: %s, code: %d, msg: '%s'", 
                                      sz_path, 
                                      create_dir_error, 
                                      win32_system_error_message(create_dir_error));
                            return INVALID_HANDLE_VALUE;
                        }
                    }
                }
                ptr++;
            }
        } else {
            LOG_ERROR("failed creating file: '%s', code: %d, msg: '%s'", sz_path, code, win32_system_error_message(code));
            return INVALID_HANDLE_VALUE;
        }
    }

    return file;
}

bool is_directory(String path)
{
    char *sz_path = sz_string(path);
    DWORD attribs = GetFileAttributesA(sz_path);
    return attribs == FILE_ATTRIBUTE_DIRECTORY;
}
                   
Array<String> win32_list_files(String dir, Allocator mem = mem_tmp)
{
    DynamicArray<String> files{ .alloc = mem };
    
    String dir2 = join_path(dir, "/*");
    char *sz_path = sz_string(dir2);
    
    WIN32_FIND_DATAA ffd{};
    HANDLE ff = FindFirstFileA(sz_path, &ffd);
    
    do {
        if (ffd.cFileName[0] == '.') continue;
        
        if (ffd.dwFileAttributes & (FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE)) {
            String filename{ ffd.cFileName, (i32)strlen(ffd.cFileName) };
            String path = join_path(dir, filename, mem);
            array_add(&files, path);
        } else {
            LOG_ERROR("unsupported file attribute for file '%s': %s", 
                      ffd.cFileName, string_from_file_attribute(ffd.dwFileAttributes));
        }
    } while (FindNextFileA(ff, &ffd));
    
    
    return files;
}

void write_file(String path, StringBuilder *sb)
{
    char *sz_path = sz_string(path);
    
    HANDLE file = win32_open_file(sz_path, CREATE_ALWAYS, GENERIC_WRITE);
    defer{ CloseHandle(file); };
    
    StringBuilder::Block *block = &sb->head;
    while (block && block->written > 0) {
        WriteFile(file, block->data, block->written, nullptr, nullptr);
        block = block->next;
    }
}

enum FileChangeType {
    FILE_CHANGE_MODIFY,
    FILE_CHANGE_ADD,
    FILE_CHANGE_REMOVE,
};

struct FileChangeInfo {
    String path;
    FileChangeType type;
};

void win32_add_filewatch(String directory, DynamicArray<FileChangeInfo> *changes, HANDLE mutex)
{
    struct FileWatchThreadData {
        String directory;
        DynamicArray<FileChangeInfo> *changes;
        HANDLE mutex;
    };
    
    auto thread_proc = [](LPVOID lpParameter) -> DWORD
    {
        FileWatchThreadData *ftd = (FileWatchThreadData *)lpParameter;

        mem_tmp = linear_allocator(10*1024*1024);
        char *sz_dir = sz_string(ftd->directory, mem_dynamic);

        HANDLE h = CreateFileA(
            sz_dir,
            GENERIC_READ | FILE_LIST_DIRECTORY,
            FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
            NULL);

        if (h == INVALID_HANDLE_VALUE) {
            LOG_ERROR("unable to open file handle for directory '%s': (%d) %s", sz_dir, WIN32_ERR_STR);
            return 1;
        }

        DWORD buffer[sizeof(FILE_NOTIFY_INFORMATION)*2];
        while (true) {
            RESET_ALLOC(mem_tmp);

            DWORD num_bytes;
            BOOL result = ReadDirectoryChangesW(
                h,
                &buffer, sizeof buffer,
                true,
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
                &num_bytes,
                NULL,
                NULL);

            if (!result) {
                LOG_ERROR("ReadDirectoryChangesW for dir '%s' failed: (%d) %s", sz_dir, WIN32_ERR_STR);
                CloseHandle(h);
                return 1;
            }

            FILE_NOTIFY_INFORMATION *fni = (FILE_NOTIFY_INFORMATION*)buffer;
            String sub_path = string_from_utf16((u16*)fni->FileName, fni->FileNameLength/2);
            String path = join_path(ftd->directory, sub_path, mem_dynamic);

            if (is_directory(path)) goto next_fni;

handle_fni:
            if (fni->Action == FILE_ACTION_MODIFIED || fni->Action == FILE_ACTION_ADDED) {
                WaitForSingleObject(ftd->mutex, TIMEOUT_INFINITE);

                FileChangeInfo add{};
                for (i32 i = 0; i < ftd->changes->count; i++) {
                    FileChangeInfo fci = ftd->changes->data[i];

                    if (fci.path == path) {
                        if (fci.type == FILE_CHANGE_REMOVE) array_remove_unsorted(ftd->changes, i);
                        goto skip_add;
                    }
                }

                LOG_INFO("detected file %s: %.*s", fni->Action == FILE_ACTION_MODIFIED ? "modified" : "add", STRFMT(path));

                add.path = path;
                add.type = fni->Action == FILE_ACTION_ADDED ? FILE_CHANGE_ADD : FILE_CHANGE_MODIFY;
                array_add(ftd->changes, add);
skip_add:
                ReleaseMutex(ftd->mutex);
            } else if (fni->Action == FILE_ACTION_REMOVED) {
                LOG_INFO("detected file removal: %.*s", STRFMT(path));

                FileChangeInfo removal{};

                WaitForSingleObject(ftd->mutex, TIMEOUT_INFINITE);
                for (i32 i = 0; i < ftd->changes->count; i++) {
                    FileChangeInfo fci = ftd->changes->data[i];
                    if (fci.path == path) {
                        if (fci.type != FILE_CHANGE_REMOVE) array_remove_unsorted(ftd->changes, i);
                        goto skip_add_remove;
                    }
                }

                removal.path = path;
                removal.type = FILE_CHANGE_REMOVE;

                array_add(ftd->changes, removal);
skip_add_remove:
                ReleaseMutex(ftd->mutex);

            }

next_fni:
            if (fni->NextEntryOffset > 0) {
                fni = (FILE_NOTIFY_INFORMATION*)(((u8*)fni) + fni->NextEntryOffset);
                goto handle_fni;
            }
        }

        CloseHandle(h);
        return 0;
    };

    FileWatchThreadData *ftd = ALLOC_T(mem_dynamic, FileWatchThreadData);
    *ftd = {
        .directory = duplicate_string(directory, mem_dynamic),
        .changes = changes,
        .mutex = mutex,
    };
    
    HANDLE thread = CreateThread(NULL, 0, thread_proc, ftd, 0, NULL);
    if (thread == NULL) LOG_ERROR("error creating file watch thread: (%d) %s", WIN32_ERR_STR);

}

u64 file_modified_timestamp(String path)
{
    char *sz_path = sz_string(path);
    HANDLE file = win32_open_file(sz_path, OPEN_EXISTING, GENERIC_READ);
    
    if (file == INVALID_HANDLE_VALUE) { 
        LOG_ERROR("unable to open file to get modified timestamp: '%.*s' - (%d) '%s'", STRFMT(path), WIN32_ERR_STR);
        return -1;
    }
    
    defer { CloseHandle(file); };
    
    FILETIME last_write_time;
    if (GetFileTime(file, nullptr, nullptr, &last_write_time)) {
        return last_write_time.dwLowDateTime | ((u64)last_write_time.dwHighDateTime << 32);
    }
    
    return -1;
}

void remove_file(String path)
{
    char *sz_path = sz_string(path);
    DeleteFileA(sz_path);
}

bool file_exists_sz(const char *path)
{
    return PathFileExistsA(path);
}

bool file_exists(String path)
{
    const char *sz_path = sz_string(path);
    return file_exists_sz(sz_path);
}

FileHandle open_file(String path, FileOpenMode mode)
{
    char *sz_path = sz_string(path);
    
    u32 creation_mode = 0;
    switch (mode) {
    case FILE_OPEN_CREATE:
        creation_mode = CREATE_NEW;
        break;
    case FILE_OPEN_TRUNCATE:
        creation_mode = CREATE_ALWAYS;
        break;
    }
    
    PANIC_IF(creation_mode == 0, "invalid creation mode");
    return win32_open_file(sz_path, creation_mode, GENERIC_WRITE|GENERIC_READ);
}

void write_file(FileHandle handle, char *data, i32 bytes)
{
    WriteFile(handle, data, bytes, nullptr, nullptr);
}

void close_file(FileHandle handle)
{
    CloseHandle(handle);
}

String get_exe_folder(Allocator mem)
{
    wchar_t buffer[255];
    i32 size = ARRAY_COUNT(buffer);
    
    wchar_t *sw = buffer;
    i32 length = GetModuleFileNameW(NULL, sw, size);

    while (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        if (sw == buffer) {
            sw = ALLOC_ARR(mem, wchar_t, size+10);
        } else {
            sw = REALLOC_ARR(mem, wchar_t, sw, size, size+10);
        }
        
        size += 10;

        length = GetModuleFileNameW(NULL, sw, size);
    }

    for (wchar_t *p = sw+length; p >= sw; p--) {
        if (*p == '\\') {
            length = (i32)(p - sw);
            break;
        }
    }
    
    String s = string_from_utf16(sw, length, mem);
    return s;
}

String get_current_working_dir(Allocator mem)
{
    wchar_t buffer[255];
    i32 size = ARRAY_COUNT(buffer);
    
    wchar_t *sw = buffer;
    i32 length = GetCurrentDirectoryW(size, sw); 
    
    if (length > size) {
        i32 req_size = length;
        sw = ALLOC_ARR(mem_tmp, wchar_t, req_size);
        length = GetCurrentDirectoryW(req_size, sw);
    }
    
    String s = string_from_utf16(sw, length, mem);
    return s;
}