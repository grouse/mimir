#include "file.h"

bool file_exists_sz(const char *sz_path)
{
    return access(sz_path, F_OK) == 0;
}

bool file_exists(String path)
{
    char *sz_path = sz_string(path);
    return file_exists_sz(sz_path);
}

FileInfo read_file(String path, Allocator mem, i32 /*retry_count*/)
{
    FileInfo fi{};

    char *sz_path = sz_string(path);

    struct stat st;
    i32 result = stat(sz_path, &st);
    if (result != 0) {
        LOG_ERROR("couldn't stat file: %s", sz_path);
        return fi;
    }

    fi.data = (u8*)ALLOC(mem, st.st_size);

    i32 fd = open(sz_path, O_RDONLY);
    if (fd < 0) {
        LOG_ERROR("unable to open file descriptor for file: '%s' - '%s'", sz_path, strerror(errno));
    }
    ASSERT(fd >= 0);

    // TODO(jesper): we should probably error on 4gb files here and have a different API
    // for loading large files. This API is really only suitable for small files
    i64 bytes_read = read(fd, fi.data, st.st_size);
    ASSERT(bytes_read == st.st_size);
    PANIC_IF(st.st_size > i32_MAX, "file size is too large");
    fi.size = (i32)st.st_size;

    return fi;
}

DynamicArray<String> list_files(String dir, u32 flags, Allocator mem)
{
	DynamicArray<String> dirs{ .alloc = mem };
	list_files(&dirs, dir, flags);
	return dirs;
}


void list_files(DynamicArray<String> *dst, String dir, u32 flags, Allocator mem)
{
    if (flags & FILE_LIST_ABSOLUTE) LOG_ERROR("unimplemented");

	DynamicArray<char*> folders{ .alloc = mem_tmp };
	array_add(&folders, sz_string(dir));

	for (i32 i = 0; i < folders.count; i++) {
		char *sz_path = folders[i];
    	DIR *d = opendir(sz_path);

    	if (!d) {
        	LOG_ERROR("unable to open directory: %s", sz_path);
        	continue;
    	}

    	struct dirent *it;
    	while ((it = readdir(d)) != nullptr) {
        	if (it->d_name[0] == '.') continue;

        	if (it->d_type == DT_REG) {
            	char *fp = join_path(sz_path, it->d_name, mem);
            	array_add(dst, String{ fp, (i32)strlen(fp) });
        	} else if (it->d_type == DT_DIR && flags & FILE_LIST_RECURSIVE) {
            	char *fp = join_path(sz_path, it->d_name, mem);
            	array_add(&folders, fp);
			} else if (it->d_type == DT_UNKNOWN) {
				LOG_ERROR("unknown d_type value from readdir, supposed to fall back to stat");
			} else {
				LOG_INFO("unhandled d_type for %s", it->d_name);
			}
    	}

    	closedir(d);
	}
}


void write_file(String path, StringBuilder *sb)
{
    char *sz_path = sz_string(path);
    i32 fd = open(sz_path, O_TRUNC | O_CREAT | O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    if (fd < 0) {
        LOG_ERROR("unable to open file descriptor for file: '%s' - '%s'", sz_path, strerror(errno));
        return;
    }

    StringBuilder::Block *it = &sb->head;
    while (it && it->written > 0) {
        write(fd, it->data, it->written);
        it = it->next;
    }

    close(fd);
}

void remove_file(String path)
{
    char *sz_path = sz_string(path);
    int result = remove(sz_path);
    if (result != 0) LOG_ERROR("error removing file: '%s'", sz_path);
}

String absolute_path(String relative, Allocator mem)
{
	if (relative[0] == '/') return duplicate_string(relative, mem);

	char buffer[PATH_MAX];
	char *wd = getcwd(buffer, sizeof buffer);
	PANIC_IF(wd == nullptr, "current working dir exceeds PATH_MAX");

	return join_path({ wd, (i32)strlen(wd) }, relative, mem);
}

bool is_directory(String path)
{
	char *sz_path = sz_string(path);

	struct stat st;
	if (stat(sz_path, &st)) {
		LOG_ERROR("failed to stat file: '%s', errno: %d", sz_path, errno);
		return false;
	}

	return st.st_mode & S_IFDIR;
}

String get_exe_folder(Allocator mem)
{
	return duplicate_string(core.exe_path, mem);
}

String get_working_dir(Allocator mem)
{
	char buffer[PATH_MAX];
	char *wd = getcwd(buffer, sizeof buffer);
	PANIC_IF(wd == nullptr, "current working dir exceeds PATH_MAX");

	return create_string(wd, (i32)strlen(wd), mem);
}

String directory_of(String path, Allocator mem)
{
	i32 p = last_of(path, '/');
	if (p > 0) return duplicate_string(slice(path, 0, p), mem);
	return {};
}

void set_working_dir(String path)
{
	char *sz_path = sz_string(path);
	if (chdir(sz_path)) {
		LOG_ERROR("failed to set working dir to: '%s', errno: %d", sz_path, errno);
	}
}

FileHandle open_file(String /*path*/, FileOpenMode /*mode*/)
{
	LOG_ERROR("unimplemented");
	return nullptr;
}

void write_file(FileHandle /*handle*/, char * /*data*/, i32 /*bytes*/)
{
	LOG_ERROR("unimplemented");
}

void close_file(FileHandle /*handle*/)
{
	LOG_ERROR("unimplemented");
}

String select_folder_dialog(Allocator /*mem*/)
{
	LOG_ERROR("unimplemented");
	return {};
}
