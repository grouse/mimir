#ifndef FILE_H
#define FILE_H

struct FileInfo {
    u8 *data;
    i32 size;
};

FileInfo read_file(String path, Allocator mem = mem_tmp, i32 retry_count = 0);
Array<String> list_files(String dir, Allocator mem = mem_tmp);

void write_file(String path, StringBuilder *sb);

bool is_directory(String path);
bool file_exists_sz(const char *path);
bool file_exists(String path);

void remove_file(String path);

#endif //FILE_H
