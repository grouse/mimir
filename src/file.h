#ifndef FILE_H
#define FILE_H

#ifdef _WIN32
using FileHandle = HANDLE;
#else
struct FileHandle_;
using FileHandle = FileHandle_*;
#endif

#include "array.h"
#include "string.h"

struct FileInfo {
    u8 *data;
    i32 size;
};

enum FileOpenMode {
    FILE_OPEN_CREATE = 1,
    FILE_OPEN_TRUNCATE,
};

enum ListFileFlags : u32 {
    FILE_LIST_RECURSIVE = 1 << 0,
    FILE_LIST_ABSOLUTE  = 1 << 1,
};

FileInfo read_file(String path, Allocator mem = mem_tmp, i32 retry_count = 0);

DynamicArray<String> list_files(String dir, u32 flags = 0, Allocator mem = mem_tmp);
void list_files(DynamicArray<String> *dst, String dir, u32 flags = 0, Allocator mem = mem_tmp);

String directory_of(String path, Allocator mem = mem_tmp);
String absolute_path(String relative, Allocator mem = mem_tmp);

FileHandle open_file(String path, FileOpenMode mode);
void write_file(FileHandle handle, char *data, i32 bytes);
void close_file(FileHandle handle);

void write_file(String path, void *data, i32 bytes);
void write_file(String path, StringBuilder *sb);

bool is_directory(String path);
bool file_exists_sz(const char *path);
bool file_exists(String path);

void remove_file(String path);

String get_exe_folder(Allocator mem = mem_tmp);
String get_working_dir(Allocator mem = mem_tmp);
void set_working_dir(String path);

String select_folder_dialog(Allocator mem = mem_tmp);

#endif //FILE_H
