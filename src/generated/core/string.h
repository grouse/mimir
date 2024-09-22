#ifndef STRING_PUBLIC_H
#define STRING_PUBLIC_H

namespace PUBLIC {}
using namespace PUBLIC;

extern String string(const char *str, i32 length, Allocator mem);
extern String string(const char *sz_str, Allocator mem);
extern String slice(String str, i32 start, i32 end);
extern String slice(String str, i32 start);
extern String slice(const char *str, i32 start, i32 end);
extern u8 u8_from_string(String s);
extern bool u8_from_string(String s, u8 *dst);
extern u64 u64_from_string(String s);
extern bool u64_from_string(String s, u64 *dst);
extern bool string_contains(String lhs, String rhs);
extern i32 utf8_length(const u16 *str, i32 utf16_len, i32 limit);
extern i32 utf8_length(const u16 *str, i32 utf16_len);
extern i32 utf16_length(String str);
extern void utf16_from_string(u16 *dst, i32 capacity, String src);
extern u16 *utf16_from_string(String str, i32 *utf16_length, Allocator mem);
extern i32 byte_index_from_codepoint_index(String str, i32 codepoint);
extern i32 codepoint_index_from_byte_index(String str, i32 byte);
extern i64 utf8_decr(char *str, i64 i);
extern i64 utf8_incr(char *str, i64 length, i64 i);
extern i32 utf8_decr(String str, i32 i);
extern i32 utf8_incr(String str, i32 i);
extern bool path_equals(String lhs, String rhs);
extern String extension_of(String path);
extern const char *sz_extension_of(const char *path);
extern String filename_of(String path);
extern String filename_of_sz(const char *path);
extern String directory_of(String path);
extern String join_path(String root, String filename, Allocator mem);
extern char *join_path(const char *sz_root, const char *sz_filename, Allocator mem);
extern i32 utf8_from_utf32(u8 utf8[4], i32 utf32);
extern i64 utf32_it_next(char *str, i64 length, i64 *offset);
extern i32 utf32_it_next(String str, i32 *offset);
extern i32 utf32_it_next(char **utf8, char *end);
extern void reset_string_builder(StringBuilder *sb);
extern String create_string(StringBuilder *sb, Allocator mem);
extern char *sz_string(StringBuilder *sb, Allocator mem);
extern void append_stringf(StringBuilder *sb, const char *fmt);
extern bool is_whitespace(i32 c);
extern bool is_number(i32 c);
extern bool is_newline(i32 c);
extern String to_lower(String s, Allocator mem);
extern i32 last_of(String str, char c);
extern i32 last_of(const char *str, char c);
extern i32 first_of(String str, char c);
extern i32 first_of(const char *str, char c);
extern bool parse_cmd_argument(String *args, i32 count, String name, i32 values[2]);
extern bool parse_cmd_argument(String *args, i32 count, String name, f32 values[2]);

#endif // STRING_PUBLIC_H
