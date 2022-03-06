#ifndef STRING_H
#define STRING_H
    
#include "allocator.h"

#define STRFMT(str) (str).length, (str).data

struct String {
    char *data;
    i32 length;

    String() = default;
    String(char *data, i32 length)
    {
        this->data = data;
        this->length = length;
    }

    template<i32 N>
    String(const char(&str)[N])
    {
        length = N-1;
        data = (char*)str;
    }

    char& operator[](i32 i) 
    { 
        // NOTE(jesper): disabled because C/C++ is garbage and assert is defined 
        // in core.h but core.h needs to include string.h for its procedures. Need 
        // to make them not rely on string or some nonsense
        //ASSERT(i < length && i >= 0);
        return data[i]; 
    }
};

struct StringBuilder {
    struct Block {
        Block *next = nullptr;
        i32 written = 0;
        char data[4096];
    };

    Allocator alloc = mem_dynamic;
    Block head = {};
    Block *current = &head;
};

bool operator!=(String lhs, String rhs);
bool operator==(String lhs, String rhs);

String create_string(char *str, i32 length, Allocator mem = mem_tmp);
String duplicate_string(String other, Allocator mem = mem_tmp);

bool is_whitespace(i32 c);

String stringf(char *buffer, i32 size, const char *fmt, ...);
String stringf(Allocator mem, const char *fmt, ...);

i32 i32_from_string(String s);
bool f32_from_string(String s, f32 *dst);

String filename_of(String path);
String extension_of(String path);
String path_relative_to(String path, String root);
String join_path(String root, String filename, Allocator mem = mem_tmp);
char* join_path(const char *sz_root, const char *sz_filename, Allocator mem = mem_tmp);

String slice(String str, i32 start, i32 end);
String slice(String str, i32 start);

char* sz_string(String str, Allocator mem = mem_tmp);

i32 utf8_length(const u16 *str, i32 utf16_len, i32 limit);
i32 utf8_length(const u16 *str, i32 utf16_len);
i32 utf8_length(const wchar_t *str, i32 utf16_len);
i32 utf16_length(String str);

void utf16_from_string(u16 *dst, i32 capacity, String src);

i32 utf8_from_utf16(u8 *dst, i32 capacity, const u16 *src, i32 length);
i32 utf8_from_utf16(u8 *dst, i32 capacity, const wchar_t *src, i32 length);

i32 utf8_from_utf32(u8 utf8[4], i32 utf32);

String string_from_utf16(const u16 *in_str, i32 length, Allocator mem = mem_tmp);
String string_from_utf16(const wchar_t *in_str, i32 length, Allocator mem = mem_tmp);

i32 utf32_next(char **p, char *end);
i32 utf32_it_next(char **utf8, char *end);

i32 utf8_decr(String str, i32 i);
i32 utf8_incr(String str, i32 i);
i32 utf8_truncate(String str, i32 limit);

i32 byte_index_from_codepoint_index(String str, i32 codepoint);
i32 codepoint_index_from_byte_index(String str, i32 byte);

void append_string(StringBuilder *sb, String str);
void append_stringf(StringBuilder *sb, const char *fmt, ...);

#endif // STRING_H