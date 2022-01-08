#include "input.h"

#include "maths.cpp"
#include "gui.cpp"
#include "string.cpp"
#include "allocator.cpp"
#include "core.cpp"
#include "gfx_opengl.cpp"
#include "assets.cpp"

enum EditMode {
    MODE_EDIT,
    MODE_INSERT,
};

enum BufferHistoryType {
    BUFFER_INSERT,
    BUFFER_REMOVE,

    BUFFER_CURSOR_POS,
    BUFFER_HISTORY_GROUP_START,
    BUFFER_HISTORY_GROUP_END
};

struct BufferLine {
    i64 offset : 63;
    i64 wrapped : 1;
};

struct Application {
    Font mono;
    i32 tab_width = 4;
    bool animating = true;
    
    EditMode mode, next_mode;
    
    Vector3 bg = rgb_unpack(0x00142825);
    Vector3 fg = rgb_unpack(0x00D5C4A1);
    
    Vector3 mark_fg = rgb_unpack(0xFFEF5F0A);
    Vector3 caret_fg = rgb_unpack(0xFFEF5F0A);
    Vector3 caret_bg = rgb_unpack(0xFF8a523f);
    
    Vector3 line_bg = rgb_unpack(0xFF264041);
};

struct BufferHistory {
    BufferHistoryType type;
    union {
        struct {
            i64 offset;
            String text;
        };
        struct {
            i64 caret;
            i64 mark;
        };
    };
};

enum NewlineMode {
    NEWLINE_LF = 1,
    NEWLINE_CR,
    NEWLINE_CRLF,
    NEWLINE_LFCR,
};

enum BufferType {
    BUFFER_FLAT,
};


struct Buffer {
    String name;
    String file_path;
    
    DynamicArray<BufferHistory> history;
    i32 history_index;
    i32 saved_at;
    
    NewlineMode newline_mode;
    
    BufferType type;
    union {
        struct { char *data; i64 size; i64 capacity; } flat;
    };
};

struct BufferId {
    i32 index = -1;
};

bool operator==(const BufferId &lhs, const BufferId &rhs)
{
    static_assert(sizeof lhs == sizeof lhs.index);
    return memcmp(&lhs, &rhs, sizeof lhs) == 0;
}

struct Caret {
    i64 byte_offset = 0;
    
    i64 wrapped_column = 0;
    i64 column = 0;
    i64 preferred_column = 0;

    i32 line = 0;
    i32 wrapped_line = 0;
};

struct View {
    DynamicArray<BufferLine> lines;
    
    f32 voffset;
    i32 line_offset;
    i32 lines_visible;
    
    Rect rect;
    BufferId buffer;
    Caret caret, mark;
    
    struct {
        u64 lines_dirty : 1;
        u64 caret_dirty : 1;
    };

};

Application app{};
View view{};
DynamicArray<Buffer> buffers{};

Buffer* get_buffer(BufferId buffer_id)
{
    if (buffer_id.index < 0) return nullptr;
    return &buffers[buffer_id.index];
}

bool buffer_valid(BufferId buffer_id)
{
    return buffer_id.index >= 0;
}

void buffer_history(BufferId buffer_id, BufferHistory entry)
{
    Buffer *buffer = get_buffer(buffer_id);
    ASSERT(buffer);
    
    if (entry.type == BUFFER_HISTORY_GROUP_END) {
        if (buffer->history.count > 2 &&
            buffer->history[buffer->history.count-1].type == BUFFER_CURSOR_POS &&
            buffer->history[buffer->history.count-2].type == BUFFER_CURSOR_POS &&
            buffer->history[buffer->history.count-3].type == BUFFER_HISTORY_GROUP_START) 
        {
            buffer->history.count -= 3;
            buffer->history_index = MIN(buffer->history_index, buffer->history.count);
            return;
        } else if (buffer->history.count > 0 &&
                   buffer->history[buffer->history.count-1].type == BUFFER_HISTORY_GROUP_START) 
        {
            buffer->history.count -= 1;
            buffer->history_index = MIN(buffer->history_index, buffer->history.count);
            return;
        }
    }

    // TODO(jesper): history allocator. We're leaking the memory of the history strings
    // that we're overwriting after this. We should probably have some kind of per buffer
    // circular buffer allocator for these with some intrusive linked list type situation
    if (buffer->history_index < buffer->history.count) {
        buffer->history.count = buffer->history_index;
    }

    switch (entry.type) {
    case BUFFER_REMOVE:
    case BUFFER_INSERT:
        entry.text = duplicate_string(entry.text, mem_dynamic);
        break;
    case BUFFER_HISTORY_GROUP_START:
    case BUFFER_HISTORY_GROUP_END:
    case BUFFER_CURSOR_POS:
        break;
    }
    
    
    array_add(&buffer->history, entry);
    buffer->history_index = buffer->history.count;
}

bool is_word_boundary(i32 c)
{
    switch(c) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '*':
    case '!':
    case '@':
    case '$':
    case '&':
    case '#':
    case '^':
    case '+':
    case '-':
    case '=':
    case '.':
    case ',':
    case ';':
    case ':':
    case '?':
    case '<': case '>':
    case '%':
    case '[': case ']':
    case '{': case '}':
    case '(': case ')':
    case '\'': case '\"': case '`':
    case '/':
    case '\\':
    case '|':
        return true;
    default:
        return false;
    }

}


char char_at(Buffer *b, i64 i)
{
    switch (b->type) {
    case BUFFER_FLAT: return b->flat.data[i];
    }
}

i64 next_byte(Buffer *b, i64 i)
{
    switch (b->type) {
    case BUFFER_FLAT: return i+1;
    }
}

i64 prev_byte(Buffer *b, i64 i)
{
    switch (b->type) {
    case BUFFER_FLAT: return i-1;
    }
}

void calculate_num_visible_lines()
{
    view.lines_visible = (i32)((gfx.resolution.y / (f32)app.mono.line_height) + 1.999f);
    view.lines_dirty = true;
}

String string_from_enum(NewlineMode mode)
{
    switch (mode) {
    case NEWLINE_LF: return "LF";
    case NEWLINE_CR: return "CR";
    case NEWLINE_CRLF: return "CRLF";
    case NEWLINE_LFCR: return "LFCR";
    }
}

String buffer_newline_str(BufferId buffer_id)
{
    Buffer *buffer = get_buffer(buffer_id);
    ASSERT(buffer);
    
    switch (buffer->newline_mode) {
    case NEWLINE_LF: return "\n";
    case NEWLINE_CR: return "\r";
    case NEWLINE_CRLF: return "\r\n";
    case NEWLINE_LFCR: return "\n\r";
    }
}

BufferId create_buffer(String file)
{
    // TODO(jesper): do something to try and figure out/guess file type,
    // in particular try to detect whether the file is a binary so that we can
    // write a binary blob visualiser, or at least something that won't choke and
    // crash trying to render invalid text
    
    Buffer b{ .type = BUFFER_FLAT };
    b.file_path = absolute_path(file, mem_dynamic);
    b.name = filename_of(b.file_path);
    
    FileInfo f = read_file(file, mem_dynamic);
    b.flat.data = (char*)f.data;
    b.flat.size = f.size;
    b.flat.capacity = f.size;
    
    char *start = (char*)f.data;
    char *p = start+f.size-1;
    while (p >= start) {
        if (*p == '\n') {
            if (p-1 >= start && *(p-1) == '\r') b.newline_mode = NEWLINE_CRLF;
            else b.newline_mode = NEWLINE_LF;
            break;
        } else if (*p == '\r') {
            if (p-1 >= start && *(p-1) == '\n') b.newline_mode = NEWLINE_LFCR;
            else b.newline_mode = NEWLINE_CR;
            break;
        }

        p--;
    }
    
    ASSERT(b.newline_mode != 0);
    String newline_str = string_from_enum(b.newline_mode);
    LOG_INFO("created buffer: %.*s, newline mode: %.*s", STRFMT(file), STRFMT(newline_str));
    
    BufferId id{};
    id.index = array_add(&buffers, b);
    return id;
}

void init_app()
{
    String asset_folders[] = {
        "./",
        "../assets",
        // bin_folder
    };
    init_assets({ asset_folders, ARRAY_COUNT(asset_folders) });
    
    init_gui();
    
    app.mono = load_font("fonts/Cousine/Cousine-Regular.ttf");
    
    calculate_num_visible_lines();
}

void app_open_file(String path)
{
    view.buffer = create_buffer(path);
    view.caret_dirty = true;
    view.lines_dirty = true;
}

bool app_change_resolution(Vector2 resolution)
{
    if (!gfx_change_resolution(resolution)) return false;
    calculate_num_visible_lines();
    return true;
}

i64 utf8_incr(Buffer *buffer, i64 i)
{
    switch (buffer->type) {
    case BUFFER_FLAT: return utf8_incr(buffer->flat.data, buffer->flat.size, i);
    }
}

i64 utf8_decr(Buffer *buffer, i64 i)
{
    switch (buffer->type) {
    case BUFFER_FLAT: return utf8_decr(buffer->flat.data, buffer->flat.size, i);
    }
}

i64 utf8_decr(BufferId buffer_id, i64 i)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return i;
    
    return utf8_decr(buffer, i);
}

i32 utf32_it_next(Buffer *buffer, i64 *byte_offset)
{
    switch (buffer->type) {
    case BUFFER_FLAT: return utf32_it_next(buffer->flat.data, buffer->flat.size, byte_offset);
    }
}

i64 buffer_end(Buffer *buffer)
{
    switch (buffer->type) { 
    case BUFFER_FLAT: return buffer->flat.size;
    }
}

i64 buffer_end(BufferId buffer_id)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return 0;
    
    return buffer_end(buffer);
}


i64 buffer_start(Buffer *buffer)
{
    switch (buffer->type) { 
    case BUFFER_FLAT: return 0;
    }
}

i64 line_end_offset(i32 wrapped_line, Array<BufferLine> lines, Buffer *buffer)
{
    switch (buffer->type) {
    case BUFFER_FLAT: return (wrapped_line+1) < lines.count ? lines[wrapped_line+1].offset : buffer->flat.size;
    }
}

void recalculate_line_wrap(i32 wrapped_line, DynamicArray<BufferLine> *lines, BufferId buffer_id)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return;
    
    Rect r = view.rect;

    Vector2 baseline{ r.pos.x, r.pos.y + (f32)app.mono.baseline };
    Vector2 pen = baseline;

    i64 last_line = wrapped_line;
    while (last_line+1 < lines->count && lines->at(last_line+1).wrapped) last_line++;

    DynamicArray<BufferLine> new_lines{ .alloc = mem_tmp };
    array_add(&new_lines, lines->at(wrapped_line));


    i64 start = lines->at(wrapped_line).offset;
    i64 end = line_end_offset(last_line, *lines, buffer);
    i64 p = start;

    i64 line_start = p;
    i64 word_start = p;

    i64 vcolumn = 0;
    i32 prev_c = ' ';

    while (p < end) {
        i64 pn = p;
        i32 c = utf32_it_next(buffer, &p);
        if (c == 0) break;

        if ((is_word_boundary(prev_c) && !is_word_boundary(c)) ||
            (is_word_boundary(c) && !is_word_boundary(prev_c)))
        {
            word_start = pn;
        }
        prev_c = c;

        if (c == '\n') {
            baseline.y += app.mono.line_height;
            pen = baseline;
            if (p < end && char_at(buffer, p) == '\r') p = next_byte(buffer, p);
            line_start = word_start = p;
            array_add(&new_lines, { (i64)line_start, 0 });
            vcolumn = 0;
            continue;
        }

        if (c == '\r') {
            baseline.y += app.mono.line_height;
            pen = baseline;
            if (p < end && char_at(buffer, p) == '\n') p = next_byte(buffer, p);
            line_start = word_start = p;
            array_add(&new_lines, { (i64)line_start, 0 });
            vcolumn = 0;
            continue;
        }

        if (c == ' ') {
            pen.x += app.mono.space_width;
            vcolumn++;
            continue;
        }

        if (c == '\t') {
            i32 w = app.tab_width - vcolumn % app.tab_width;
            pen.x += app.mono.space_width*w;
            vcolumn += w;
            continue;
        }

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(app.mono.atlas, 1024, 1024, c, &pen.x, &pen.y, &q, 1);

        if (q.x1 >= r.pos.x + r.size.x) {
            baseline.y += app.mono.line_height;
            pen = baseline;

            // TODO(jesper): go back 1 more glyph and replace it with some kind of word-break glyph
            // if the line has to be broken mid-word because of super long single word lines
            // Maybe the word-break glyph should be on the start of the artificial line start?
            if (!is_word_boundary(c) && line_start != word_start) { 
                p = line_start = word_start;
                array_add(&new_lines, { (i64)line_start, 1 });
                vcolumn = 0;
                continue;
            } else {
                p = line_start = word_start = pn;
                array_add(&new_lines, { (i64)line_start, 1 });
                vcolumn = 0;
                continue;
            }
        }

        vcolumn++;
    }

    array_replace_range(lines, wrapped_line, last_line+2, new_lines);
}

bool buffer_remove(BufferId buffer_id, i64 byte_start, i64 byte_end, bool record_history = true)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return false;
    
    switch (buffer->type) {
    case BUFFER_FLAT:
        if (byte_start >= 0 && byte_end <= buffer->flat.size) {
            i64 num_bytes = byte_end-byte_start;
            if (record_history) {
                BufferHistory h{
                    .type = BUFFER_REMOVE, 
                    .offset = byte_start, 
                    .text = { &buffer->flat.data[byte_start], (i32)num_bytes }
                };
                buffer_history(buffer_id, h);
            }
            memmove(&buffer->flat.data[byte_start], &buffer->flat.data[byte_end], buffer->flat.size-byte_end);
            buffer->flat.size -= num_bytes;
            return true;
        } else LOG_ERROR("out of range");
        break;
    }
    
    return false;
}

void buffer_save(BufferId buffer_id)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return;
    
    if (buffer->saved_at == buffer->history_index) return;
    
    FileHandle f = open_file(buffer->file_path, FILE_OPEN_TRUNCATE);
    switch (buffer->type) {
    case BUFFER_FLAT:
        write_file(f, buffer->flat.data, buffer->flat.size);
        break;
    }
    close_file(f);
    
    buffer->saved_at = buffer->history_index;
}

bool buffer_unsaved_changes(BufferId buffer_id)
{
    // TODO(jesper): ideally this should be smart enough to detect whether the changes 
    // made since last save point cancel out. There should be enough information in 
    // the buffer history to figure that out
    Buffer *buffer = get_buffer(buffer_id);
    return buffer && buffer->saved_at != buffer->history_index;
}

void buffer_insert(BufferId buffer_id, i64 offset, String text, bool record_history = true)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return;
    
    switch (buffer->type) {
    case BUFFER_FLAT: 
        if (buffer->flat.size + text.length > buffer->flat.capacity) {
            i64 new_capacity = buffer->flat.size + text.length;
            buffer->flat.data = (char*)REALLOC(mem_dynamic, buffer->flat.data, buffer->flat.capacity, new_capacity);
            buffer->flat.capacity = new_capacity;
        }

        for (i64 i = buffer->flat.size+text.length-1; i > offset; i--) {
            buffer->flat.data[i] = buffer->flat.data[i-text.length];
        }

        memcpy(buffer->flat.data+offset, text.data, text.length);
        buffer->flat.size += text.length;
        break;
    }

    if (view.buffer == buffer_id) {
        for (i32 i = view.caret.wrapped_line+1; i < view.lines.count; i++) {
            view.lines[i].offset += text.length;
        }

        recalculate_line_wrap(view.caret.wrapped_line, &view.lines, view.buffer);
    }

    if (record_history) {
        buffer_history(buffer_id, { .type = BUFFER_INSERT, .offset = offset, .text = text });
    }
}

void buffer_undo(BufferId buffer_id)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return;
    
    if (buffer->history_index == 0) return;
    
    i32 group_level = 0;
    do {
        BufferHistory h = buffer->history[--buffer->history_index];
        switch (h.type) {
        case BUFFER_INSERT:
            buffer_remove(buffer_id, h.offset, h.offset+h.text.length, false);
            break;
        case BUFFER_REMOVE:
            buffer_insert(buffer_id, h.offset, h.text, false);
            break;
        case BUFFER_CURSOR_POS:
            view.caret.byte_offset = h.caret;
            view.mark.byte_offset = h.mark;
            view.caret_dirty = true;
            break;
        case BUFFER_HISTORY_GROUP_START:
            group_level++;
            break;
        case BUFFER_HISTORY_GROUP_END:
            group_level--;
            break;
        }
    } while (group_level < 0);
}

void buffer_redo(BufferId buffer_id)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return;
    
    if (buffer->history_index == buffer->history.count) return;

    i32 group_level = 0;
    do {
        BufferHistory h = buffer->history[buffer->history_index++];
        switch (h.type) {
        case BUFFER_INSERT:
            buffer_insert(buffer_id, h.offset, h.text, false);
            break;
        case BUFFER_REMOVE:
            buffer_remove(buffer_id, h.offset, h.offset+h.text.length, false);
            break;
        case BUFFER_CURSOR_POS:
            view.caret.byte_offset = h.caret;
            view.mark.byte_offset = h.mark;
            view.caret_dirty = true;
            break;
        case BUFFER_HISTORY_GROUP_START:
            group_level++;
            break;
        case BUFFER_HISTORY_GROUP_END:
            group_level--;
            break;
        }
    } while (group_level > 0);
}


i64 buffer_prev_offset(BufferId buffer_id, i64 byte_offset)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return byte_offset;
    
    i64 prev_offset = byte_offset;

    switch (buffer->type) {
    case BUFFER_FLAT:
        if (byte_offset == 0) return 0;
        
        char c = char_at(buffer, prev_byte(buffer, byte_offset));
        if (c == '\n' || c == '\r') {
            prev_offset = prev_byte(buffer, byte_offset);
            char prev_c = char_at(buffer, prev_byte(buffer, prev_offset));

            if (prev_offset > 0 &&
                ((c == '\n' && prev_c == '\r') ||
                 (c == '\r' && prev_c == '\n')))
            {
                prev_offset = prev_byte(buffer, prev_offset);
            }
        } else {
            prev_offset = utf8_decr(buffer, byte_offset);
        }
        break;
    }
    
    return prev_offset;
}

i64 buffer_next_offset(BufferId buffer_id, i64 byte_offset)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return byte_offset;

    i64 end_offset = buffer_end(buffer_id);
    if (byte_offset >= end_offset) return end_offset;
    
    i64 next_offset = byte_offset;
    
    switch (buffer->type) {
    case BUFFER_FLAT:
        char c = char_at(buffer, byte_offset);
        if (c == '\n' || c == '\r') {
            next_offset = next_byte(buffer, byte_offset);
            char next_c = char_at(buffer, next_offset);

            if (next_offset < buffer->flat.size &&
                ((c == '\n' && next_c == '\r') ||
                 (c == '\r' && next_c == '\n')))
            {
                next_offset = next_byte(buffer, next_offset);
            }
        } else {
            next_offset = utf8_incr(buffer, byte_offset);
        }
        break;
    }

    return next_offset;
}

i64 buffer_seek_next_word(BufferId buffer_id, i64 byte_offset)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return byte_offset;

    switch (buffer->type) {
    case BUFFER_FLAT: {
            i64 offset = byte_offset;
            i64 end = buffer_end(buffer_id);
            
            char *p = &buffer->flat.data[0];
            
            char start_c = p[offset++];
            bool start_is_whitespace = is_whitespace(start_c);
            bool start_is_boundary = !start_is_whitespace && is_word_boundary(start_c);
            bool in_whitespace = start_is_whitespace;
            bool was_cr = start_c == '\r';
            
            for (; offset < end; offset++) {
                char c = buffer->flat.data[offset];
                
                bool whitespace = is_whitespace(c);
                bool boundary = !whitespace && is_word_boundary(c);
                
                if (c == '\r') {
                    was_cr = true;
                    boundary = true;
                } else { 
                    if (!was_cr && c == '\r') boundary = true;
                    was_cr = false;
                }
                
                if (start_is_boundary && !whitespace) return offset;
                if (!start_is_whitespace) {
                    if (boundary || (in_whitespace && !whitespace)) {
                        return offset;
                    } else if (whitespace) {
                        in_whitespace = true;
                    }
                } else if (!whitespace || boundary) {
                    return offset;
                }
            }
        } break;
    }
    
    return byte_offset;
}

i64 buffer_seek_prev_word(BufferId buffer_id, i64 byte_offset)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return byte_offset;

    switch (buffer->type) {
    case BUFFER_FLAT: {
            i64 offset = byte_offset;
            i64 end = buffer_end(buffer_id);
            
            char *p = &buffer->flat.data[0];
            
            char start_c = p[offset--];
            bool start_is_whitespace = is_whitespace(start_c);
            bool start_is_boundary = !start_is_whitespace && is_word_boundary(start_c);
            bool start_is_normal = !start_is_boundary && !start_is_whitespace;

            bool was_whitespace = start_is_whitespace;
            bool has_whitespace = false;
            bool was_ln = start_c == '\n';
            
            for (; offset >= 0; offset--) {
                char c = p[offset];
                bool whitespace = is_whitespace(c);
                bool boundary = !whitespace && is_word_boundary(c);
                
                if (c == '\n') {
                    was_ln = true;
                    boundary = true;
                } else {
                    if (was_ln && c == '\r') boundary = true;
                    was_ln = false;
                }
                
                has_whitespace = has_whitespace || whitespace;
                if (whitespace && !was_whitespace) {
                    if (offset+1 != byte_offset) {
                        return CLAMP(0, offset+1, end);
                    }
                }
                
                if (start_is_boundary && !whitespace && boundary && (offset+1 != byte_offset)) {
                    if (was_whitespace) {
                        return CLAMP(0, offset, end);
                    } else {
                        return CLAMP(0, offset+1, end);
                    }
                }
                
                if (boundary && offset != byte_offset) {
                    if ((has_whitespace || start_is_normal) && !was_whitespace && offset+1 != byte_offset) {
                        return CLAMP(0, offset+1, end);
                    } else {
                        return CLAMP(0, offset, end);
                    }
                }
                
                was_whitespace = whitespace;
            }
            
            return 0;
        } break;
    }
    
    return byte_offset;
}

i64 caret_nth_offset(BufferId buffer_id, i64 current, i32 n)
{
    i64 offset = current;
    while (n--) offset = buffer_next_offset(buffer_id, offset);
    return offset;
}

i64 line_end_offset(i32 wrapped_line, Array<BufferLine> lines, BufferId buffer_id)
{
    return line_end_offset(wrapped_line, lines, &buffers[buffer_id.index]);
}

i64 wrapped_column_count(i32 wrapped_line, Array<BufferLine> lines, Buffer *buffer)
{
    i64 count = 0;
    
    i64 offset = lines[wrapped_line].offset;
    i64 end_offset = line_end_offset(wrapped_line, lines, buffer);
    while (true) {
        char c = char_at(buffer, offset);
        
        offset = utf8_incr(buffer, offset);
        if (offset >= end_offset) break;
        if (c == '\n' || c == '\r') break;
        
        if (c == '\t') count += app.tab_width - count % app.tab_width;
        else count++;
    }

    
    return count;
}

i64 column_count(i32 wrapped_line, Array<BufferLine> lines, Buffer *buffer)
{
    i64 count = 0;
    
    i32 line = wrapped_line;
    while (line > 0 && lines[line].wrapped) line--;
    
    i64 offset = lines[line].offset;
    i64 end_offset = line_end_offset(line, lines, buffer);

    while (offset < end_offset) {
        char c = char_at(buffer, offset);
        if (c == '\n' || c == '\r') break;
        offset = utf8_incr(buffer, offset);
        
        if (c == '\t') count += app.tab_width - count % app.tab_width;
        else count++;
    }
    
    while (line < lines.count-1 && lines[line+1].wrapped) {
        line++;
        
        offset = lines[line].offset;
        end_offset = line_end_offset(line, lines, buffer);

        while (offset < end_offset) {
            char c = char_at(buffer, offset);
            if (c == '\n' || c == '\r') break;
            offset = utf8_incr(buffer, offset);
            
            if (c == '\t') count += app.tab_width - count % app.tab_width;
            else count++;
        }
    }

    return count;
}

i64 calc_wrapped_column_from_byte_offset(i64 byte_offset, i32 wrapped_line, Array<BufferLine> lines, Buffer *buffer)
{
    i64 column = 0;
    
    i64 offset = lines[wrapped_line].offset;
    while (offset < byte_offset) {
        char c = char_at(buffer, offset);
        
        if (c == '\t') column += app.tab_width - column % app.tab_width;
        else if (c == '\r' || c == '\n') break;
        else column += 1;
        
        offset = utf8_incr(buffer, offset);
    }

    return column;
}

i64 calc_wrapped_column(i32 wrapped_line, i64 target_column, Array<BufferLine> lines, Buffer *buffer)
{
    i64 column = 0;
    
    i64 offset = lines[wrapped_line].offset;
    i64 end_offset = line_end_offset(wrapped_line, lines, buffer);
    while (offset < end_offset && column < target_column) {
        char c = char_at(buffer, offset);
        
        if (c == '\t') column += app.tab_width - column % app.tab_width;
        else if (c == '\r' || c == '\n') break;
        else column += 1;
        
        offset = utf8_incr(buffer, offset);
    }
    
    
    return column;
}

i64 calc_unwrapped_column(i32 wrapped_line, i64 wrapped_column, Array<BufferLine> lines, Buffer *buffer)
{
    i64 column = wrapped_column;

    if (lines[wrapped_line].wrapped) {
        for (i32 l = wrapped_line-1; l >= 0; l--) {
            column += wrapped_column_count(l, lines, buffer);
            if (!lines[l].wrapped) break;
        }
    }
    
    return column;
}


i64 calc_byte_offset(i64 wrapped_column, i32 wrapped_line, Array<BufferLine> lines, Buffer *buffer)
{
    i64 offset = lines[wrapped_line].offset;
    
    for (i64 c = 0; c < wrapped_column; c++) {
        if (char_at(buffer, offset) == '\t') c += app.tab_width - c % app.tab_width - 1;
        offset = utf8_incr(buffer, offset);
    }
    
    return offset;
}

bool app_needs_render()
{
    return app.next_mode != app.mode || app.animating || view.lines_dirty || view.caret_dirty;
}


Caret recalculate_caret(Caret current, BufferId buffer_id, Array<BufferLine> lines)
{
    Caret caret = current;

    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return current;

    if (caret.byte_offset == 0) caret.wrapped_line = 0;
    else if (caret.byte_offset == buffer_end(buffer_id)) caret.wrapped_line = view.lines.count-1;
    else {
        caret.wrapped_line = MIN(caret.wrapped_line, lines.count-1);
        while (caret.byte_offset < lines[caret.wrapped_line].offset) caret.wrapped_line--;
        while (caret.byte_offset >= line_end_offset(caret.wrapped_line, lines, buffer)) caret.wrapped_line++;
    }

    caret.wrapped_column = calc_wrapped_column_from_byte_offset(caret.byte_offset, caret.wrapped_line, lines, buffer);
    caret.column = calc_unwrapped_column(caret.wrapped_line, caret.wrapped_column, lines, buffer);
    caret.preferred_column = caret.wrapped_column;
    
    app.animating = true;
    return caret;
}

void move_view_to_caret()
{
    i32 line_padding = 3;
    
    // TODO(jesper): do something sensible with the decimal vertical offset
    if (view.caret.wrapped_line-line_padding < view.line_offset) {
        view.line_offset = MAX(0, view.caret.wrapped_line-line_padding);
    } else if (view.caret.wrapped_line+line_padding > view.line_offset + view.lines_visible-2) {
        view.line_offset = MIN(view.lines.count-1, view.caret.wrapped_line+line_padding - view.lines_visible+2);
    }
}

// NOTE(jesper): this is poorly named. It starts/stops a history group and records the active view's
// cursor position appropriately
struct BufferHistoryScope {
    BufferId buffer;
    
    BufferHistoryScope(BufferId buffer) : buffer(buffer)
    {
        buffer_history(buffer, { .type = BUFFER_HISTORY_GROUP_START });
        buffer_history(buffer, { .type = BUFFER_CURSOR_POS, .caret = view.caret.byte_offset, .mark = view.mark.byte_offset });
    }
    
    ~BufferHistoryScope()
    {
        buffer_history(buffer, { .type = BUFFER_CURSOR_POS, .caret = view.caret.byte_offset, .mark = view.mark.byte_offset });
        buffer_history(buffer, { .type = BUFFER_HISTORY_GROUP_END });
    }
};

void app_event(InputEvent event)
{
    // NOTE(jesper): the input processing procedure contains a bunch of buffer undo history
    // management in order to record the cursor position in the history groups, so that when
    // a user undo/redo the cursor is repositioned to where it was before/after the edit 

    // TODO(jesper): figure out if there's a better way for us to signal that the event results
    // in an update/redraw. If nothing else, the app.animating variable name just feels wrong at
    // this point, especially when I don't even have anything animating
    
    switch (event.type) {
    case IE_TEXT: 
        if (app.mode == MODE_INSERT && buffer_valid(view.buffer)) {
            BufferHistoryScope h(view.buffer);
            buffer_insert(view.buffer, view.caret.byte_offset, String{ (char*)&event.text.c[0], event.text.length });

            // TODO(jesper): calc this from buffer api
            view.caret.byte_offset += event.text.length;
            view.caret = recalculate_caret(view.caret, view.buffer, view.lines);

            app.animating = true;
        } break;
    case IE_KEY_PRESS: 
        if (app.mode == MODE_EDIT) {
            switch (event.key.code) {
            case IK_U:
                buffer_undo(view.buffer);
                view.lines_dirty = true;
                view.caret_dirty = true;
                app.animating = true;
                break;
            case IK_R:
                buffer_redo(view.buffer);
                view.lines_dirty = true;
                view.caret_dirty = true;
                app.animating = true;
                break;
            case IK_D: {
                    BufferHistoryScope h(view.buffer);
                    
                    i64 start = MIN(view.mark.byte_offset, view.caret.byte_offset);
                    i64 end = MAX(view.mark.byte_offset, view.caret.byte_offset);
                    if (start == end) end += 1;
                    
                    if (buffer_remove(view.buffer, start, end)) {
                        // TODO(jesper): recalculate only the lines affected, and short-circuit the caret re-calc
                        view.lines_dirty = true;

                        view.caret.byte_offset = start;
                        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                        view.mark = view.caret;

                        move_view_to_caret();
                    }
                } break;
            case IK_W: 
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                
                view.caret.byte_offset = buffer_seek_next_word(view.buffer, view.caret.byte_offset);
                view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                move_view_to_caret();
                app.animating = true;
                break;
            case IK_B:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                
                view.caret.byte_offset = buffer_seek_prev_word(view.buffer, view.caret.byte_offset);
                view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                move_view_to_caret();
                app.animating = true;
                break;
            case IK_J:
                if (view.caret.wrapped_line < view.lines.count-1) {
                    if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                    
                    i32 wrapped_line = ++view.caret.wrapped_line;
                    if (!view.lines[view.caret.wrapped_line].wrapped) view.caret.line++;

                    Buffer *buffer = get_buffer(view.buffer);
                    ASSERT(buffer);

                    // NOTE(jesper): these are pretty wasteful and could be combined, and might cause
                    // problems in edge case files with super long lines
                    // TODO(jesper): notepad does a funky thing here with the caret when going down into a tab-character wherein
                    // it rounds up or down to after or before the tab-character depending on the mid point of the current column.
                    view.caret.wrapped_column = calc_wrapped_column(wrapped_line, view.caret.preferred_column, view.lines, buffer);
                    view.caret.column = calc_unwrapped_column(wrapped_line, view.caret.wrapped_column, view.lines, buffer);
                    view.caret.byte_offset = calc_byte_offset(view.caret.wrapped_column, view.caret.wrapped_line, view.lines, buffer);
                    move_view_to_caret();
                    app.animating = true;
                } break;
            case IK_K:
                if (view.caret.wrapped_line > 0) {
                    if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                    
                    i32 wrapped_line = --view.caret.wrapped_line;
                    if (!view.lines[view.caret.wrapped_line+1].wrapped) view.caret.line--;

                    Buffer *buffer = get_buffer(view.buffer);
                    ASSERT(buffer);

                    // NOTE(jesper): these are pretty wasteful and could be combined, and might cause
                    // problems in edge case files with super long lines
                    // TODO(jesper): notepad does a funky thing here with the caret when going down into a tab-character wherein
                    // it rounds up or down to after or before the tab-character depending on the mid point of the current column.
                    view.caret.wrapped_column = calc_wrapped_column(wrapped_line, view.caret.preferred_column, view.lines, buffer);
                    view.caret.column = calc_unwrapped_column(wrapped_line, view.caret.wrapped_column, view.lines, buffer);
                    view.caret.byte_offset = calc_byte_offset(view.caret.wrapped_column, view.caret.wrapped_line, view.lines, buffer);
                    move_view_to_caret();
                    app.animating = true;
                } break;
            case IK_I:
                app.next_mode = MODE_INSERT;
                break;
            default: break;
            }
        } else if (app.mode == MODE_INSERT) {
            switch (event.key.code) {
            case IK_ESC:
                app.next_mode = MODE_EDIT;
                break;
            case IK_ENTER: 
                if (buffer_valid(view.buffer)) {
                    BufferHistoryScope h(view.buffer);
                    buffer_insert(view.buffer, view.caret.byte_offset, buffer_newline_str(view.buffer));

                    view.caret.byte_offset = buffer_next_offset(view.buffer, view.caret.byte_offset);
                    view.caret.wrapped_line++;
                    view.caret.line++;
                    view.caret.column = view.caret.wrapped_column = view.caret.preferred_column = 0;

                    move_view_to_caret();
                    app.animating = true;
                } break;
            case IK_TAB: 
                if (buffer_valid(view.buffer)) {
                    BufferHistoryScope h(view.buffer);
                    // TODO(jesper): this should insert tab or spaces depending on buffer setting
                    buffer_insert(view.buffer, view.caret.byte_offset, "\t");

                    view.caret.byte_offset = buffer_next_offset(view.buffer, view.caret.byte_offset);
                    view.caret = recalculate_caret(view.caret, view.buffer, view.lines);

                    move_view_to_caret();
                } break;
            case IK_BACKSPACE: 
                if (buffer_valid(view.buffer)) {
                    BufferHistoryScope h(view.buffer);
                    i64 start = buffer_prev_offset(view.buffer, view.caret.byte_offset);
                    if (buffer_remove(view.buffer, start, view.caret.byte_offset)) {
                        // TODO(jesper): recalculate only the lines affected, and short-circuit the caret re-calc
                        view.lines_dirty = true;

                        view.caret.byte_offset = start;
                        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);

                        move_view_to_caret();
                    }
                } break;
            case IK_DELETE: 
                if (buffer_valid(view.buffer)) {
                    BufferHistoryScope h(view.buffer);
                    i64 end = buffer_next_offset(view.buffer, view.caret.byte_offset);
                    if (buffer_remove(view.buffer, view.caret.byte_offset, end)) {
                        view.lines_dirty = true;
                        view.caret_dirty = true;
                    }
                } break;
                
            default: break;
            }
        }
        
        switch (event.key.code) {
        case IK_PAGE_DOWN: {
                Buffer *buffer = get_buffer(view.buffer);
                if (!buffer) break;

                for (i32 i = 0; view.caret.wrapped_line < view.lines.count-1 && i < view.lines_visible-1; i++) {
                    view.caret.wrapped_line++;
                    if (!view.lines[view.caret.wrapped_line].wrapped) view.caret.line++;
                }

                view.caret.wrapped_column = calc_wrapped_column(view.caret.wrapped_line, view.caret.preferred_column, view.lines, buffer);
                view.caret.column = calc_unwrapped_column(view.caret.wrapped_line, view.caret.wrapped_column, view.lines, buffer);
                view.caret.byte_offset = calc_byte_offset(view.caret.wrapped_column, view.caret.wrapped_line, view.lines, buffer);
                move_view_to_caret();
                app.animating = true;
            } break;
        case IK_PAGE_UP: {
                Buffer *buffer = get_buffer(view.buffer);
                if (!buffer) break;

                for (i32 i = 0; view.caret.wrapped_line > 0 && i < view.lines_visible-1; i++) {
                    view.caret.wrapped_line--;
                    if (!view.lines[view.caret.wrapped_line].wrapped) view.caret.line--;
                }

                view.caret.wrapped_column = calc_wrapped_column(view.caret.wrapped_line, view.caret.preferred_column, view.lines, buffer);
                view.caret.column = calc_unwrapped_column(view.caret.wrapped_line, view.caret.wrapped_column, view.lines, buffer);
                view.caret.byte_offset = calc_byte_offset(view.caret.wrapped_column, view.caret.wrapped_line, view.lines, buffer);
                move_view_to_caret();
                app.animating = true;
            } break;
#if 0
        case IK_S:
            if (event.key.modifiers == MF_CTRL) buffer_save(view.buffer);
            app.animating = true;
            break;
        case IK_LEFT: 
            view.caret.byte_offset = buffer_next_offset(view.buffer, view.caret.byte_offset);
            view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
            move_view_to_caret();
            break;
        case IK_RIGHT:
            view.caret.byte_offset = buffer_prev_offset(view.buffer, view.caret.byte_offset);
            view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
            move_view_to_caret();
            break;
        case IK_DOWN:
#endif
        default: break;
        }
        break;
    case IE_KEY_RELEASE: break;
    case IE_MOUSE_PRESS:
        if (event.mouse.button == 1) {
            if (event.mouse.x >= view.rect.pos.x &&
                event.mouse.x <= view.rect.pos.x + view.rect.size.x &&
                event.mouse.y >= view.rect.pos.y &&
                event.mouse.y <= view.rect.pos.y + view.rect.size.y)
            {
                Buffer *buffer = get_buffer(view.buffer);
                if (!buffer) break;
                
                i64 wrapped_line = view.line_offset + (event.mouse.y - view.rect.pos.y + view.voffset) / app.mono.line_height;
                wrapped_line = CLAMP(wrapped_line, 0, view.lines.count-1);
                
                while (view.caret.wrapped_line > wrapped_line) {
                    if (!view.lines[view.caret.wrapped_line].wrapped) view.caret.line--;
                    view.caret.wrapped_line--;
                }
                
                while (view.caret.wrapped_line < wrapped_line) {
                    if (!view.lines[view.caret.wrapped_line].wrapped) view.caret.line++;
                    view.caret.wrapped_line++;
                }

                i64 wrapped_column = (event.mouse.x - view.rect.pos.x) / app.mono.space_width;
                view.caret.wrapped_column = calc_wrapped_column(view.caret.wrapped_line, wrapped_column, view.lines, buffer);
                view.caret.preferred_column = view.caret.wrapped_column;
                view.caret.column = calc_unwrapped_column(view.caret.wrapped_line, view.caret.wrapped_column, view.lines, buffer);
                view.caret.byte_offset = calc_byte_offset(view.caret.wrapped_column, view.caret.wrapped_line, view.lines, buffer);

            }
        } break;
    case IE_MOUSE_WHEEL:
        view.voffset += event.mouse_wheel.delta;
        if (view.line_offset == 0 && view.voffset > 0) view.voffset = 0;

        if (view.voffset > app.mono.line_height) {
            f32 div = view.voffset / app.mono.line_height;
            view.line_offset = MAX(0, view.line_offset-1*div);
            view.voffset -= app.mono.line_height*div;
        } else if (view.voffset < -app.mono.line_height) {
            f32 div = view.voffset / -app.mono.line_height;
            view.line_offset += 1*div;
            view.voffset += app.mono.line_height*div;
        }
        
        view.line_offset = MIN(view.line_offset, view.lines.count - 3);
        
        app.animating = true;
        break;
    default: break;
    }
}

void reflow_lines()
{
    // TODO(jesper): merge this with the recalculate_line_wrap procedure
    Buffer *buffer = get_buffer(view.buffer);
    if (!buffer) return;

    view.lines.count = 0;
    array_add(&view.lines, BufferLine{ 0, 0 });

    // TODO(jesper): figure out what I wanna do here to re-flow the lines. Do I create persistent gui layouts
    // that I can use when reflowing? Do I reflow again if view_rect is different? How do I calculate the view_rect
    // before the scrollbar?
    Rect r{ .size = { gfx.resolution.x-gui.style.scrollbar.thickness, gfx.resolution.y } };

    Vector2 baseline{ r.pos.x, r.pos.y + (f32)app.mono.baseline };
    Vector2 pen = baseline;

    i64 start = buffer_start(buffer);
    i64 end = buffer_end(buffer);
    i64 p = start;

    i64 line_start = p;
    i64 word_start = p;

    i64 vcolumn = 0;
    i32 prev_c = ' ';
    while (p < end) {
        i64 pn = p;
        i32 c = utf32_it_next(buffer, &p);
        if (c == 0) break;

        if ((is_word_boundary(prev_c) && !is_word_boundary(c)) ||
            (is_word_boundary(c) && !is_word_boundary(prev_c)))
        {
            word_start = pn;
        }
        prev_c = c;

        if (c == '\n') {
            baseline.y += app.mono.line_height;
            pen = baseline;
            if (p < end && char_at(buffer, p) == '\r') p = next_byte(buffer, p);
            line_start = word_start = p;
            array_add(&view.lines, { (i64)(line_start - start), 0 });
            vcolumn = 0;
            continue;
        }

        if (c == '\r') {
            baseline.y += app.mono.line_height;
            pen = baseline;
            if (p < end && char_at(buffer, p) == '\n') p = next_byte(buffer, p);
            line_start = word_start = p;
            array_add(&view.lines, { (i64)(line_start - start), 0 });
            vcolumn = 0;
            continue;
        }

        if (c == ' ') {
            pen.x += app.mono.space_width;
            vcolumn++;
            continue;
        }

        if (c == '\t') {
            i32 w = app.tab_width - vcolumn % app.tab_width;
            pen.x += app.mono.space_width*w;
            vcolumn += w;
            continue;
        }

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(app.mono.atlas, 1024, 1024, c, &pen.x, &pen.y, &q, 1);

        if (q.x1 >= r.pos.x + r.size.x) {
            baseline.y += app.mono.line_height;
            pen = baseline;

            // TODO(jesper): go back 1 more glyph and replace it with some kind of word-break glyph
            // if the line has to be broken mid-word because of super long single word lines
            // Maybe the word-break glyph should be on the start of the artificial line start?
            if (!is_word_boundary(c) && line_start != word_start) { 
                p = line_start = word_start;
                array_add(&view.lines, { (i64)(line_start - start), 1 });
                vcolumn = 0;
                continue;
            } else {
                p = line_start = word_start = pn;
                array_add(&view.lines, { (i64)(line_start - start), 1 });
                vcolumn = 0;
                continue;
            }
        }

        vcolumn++;
    }
}

struct {
    struct {
        GuiWindowState wnd;
    } buffer_history;
} debug{};

void update_and_render(f32 dt)
{
    gfx_begin_frame();
    gui_begin_frame();
    
    GfxCommandBuffer debug_gfx = gfx_command_buffer();

    GuiLayout *root = gui_current_layout();
    gui_begin_layout({ .type = GUI_LAYOUT_ROW, .pos = root->pos, .size = root->size });
    
    if (gui_begin_menu()) {
        if (gui_begin_menu("debug")) {
            gui_checkbox("buffer history", &debug.buffer_history.wnd.active);
            gui_end_menu();
        }
        
        gui_end_menu();
    }
    
    if (gui_begin_window("buffer history", &debug.buffer_history.wnd)) {
        char str[256];

        Buffer *buffer = &buffers[view.buffer.index];
        for (auto h : buffer->history) {
            switch (h.type) {
            case BUFFER_INSERT:
                gui_textbox(stringf(str, sizeof str, "INSERT (%lld): '%.*s'", h.offset, STRFMT(h.text)));
                break;
            case BUFFER_REMOVE:
                gui_textbox(stringf(str, sizeof str, "REMOVE (%lld): '%.*s'", h.offset, STRFMT(h.text)));
                break;
            case BUFFER_CURSOR_POS:
                gui_textbox(stringf(str, sizeof str, "CURSOR: %lld", h.offset));
                break;
            case BUFFER_HISTORY_GROUP_START:
                gui_textbox("GROUP_START");
                break;
            case BUFFER_HISTORY_GROUP_END:
                gui_textbox("GROUP_END");
                break;
            }
        }

        gui_end_window();
    }

    view.caret_dirty |= view.lines_dirty;
    {
        Rect tr = gui_layout_widget_fill();
        gui_begin_layout({ .type = GUI_LAYOUT_ROW, .pos = tr.pos, .size = tr.size });
        defer { gui_end_layout(); };

        {
            Vector2 ib_s{ tr.size.x, 15.0f };
            Vector2 ib_p = gui_layout_widget(ib_s, GUI_ANCHOR_BOTTOM);
            
            gui_begin_layout({ .type = GUI_LAYOUT_COLUMN, .pos = ib_p, .size = ib_s });
            defer { gui_end_layout(); };

            char buffer[256];
            gui_textbox(stringf(buffer, sizeof buffer, "line: %d, col: %lld", view.caret.line+1, view.caret.column+1));
        }

        {
            Rect r = gui_layout_widget_fill();
            gui_begin_layout( { .type = GUI_LAYOUT_COLUMN, .pos = r.pos, .size = r.size });
            defer { gui_end_layout(); };

            // NOTE(jesper): the scrollbar is 1 frame delayed if the reflowing results in a different number of lines,
            // which we are doing afterwards to allow the gui layout system trigger line reflowing
            // We could move it to after the reflow handling with some more layout plumbing, by letter caller deal with
            // the layouting and passing in absolute positions and sizes to the scrollbar
            gui_scrollbar(app.mono.line_height, &view.line_offset, view.lines.count, view.lines_visible, &view.voffset);
            Rect text_rect = gui_layout_widget_fill();

            if (view.lines_dirty || text_rect != view.rect) {
                view.rect = text_rect;
                reflow_lines();
                view.lines_dirty = false;
            }
        }
    }
    
    if (view.caret_dirty) {
        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
        view.caret_dirty = false;
    }

    if (view.caret.wrapped_line >= view.line_offset && 
        view.caret.wrapped_line < view.line_offset + view.lines_visible)
    {
        i32 y = view.caret.wrapped_line - view.line_offset;
        f32 w = app.mono.space_width;
        f32 h = app.mono.line_height - 2.0f;
        
        Vector2 p0{ 
            view.rect.pos.x + view.caret.wrapped_column*app.mono.space_width,
            view.rect.pos.y + y*app.mono.line_height - view.voffset,
        };
        Vector2 p1{ p0.x, p0.y + h };
        
        gui_draw_rect(&gfx.frame_cmdbuf, { view.rect.pos.x, p0.y }, { view.rect.size.x, h }, app.line_bg);

        gui_draw_rect(&gfx.frame_cmdbuf, p0, { w, h }, app.caret_bg);
        gui_draw_rect(&gfx.frame_cmdbuf, p0, { 1.0f, h }, app.caret_fg);
        gui_draw_rect(&gfx.frame_cmdbuf, p0, { w, 1.0f }, app.caret_fg);
        gui_draw_rect(&gfx.frame_cmdbuf, p1, { w, 1.0f }, app.caret_fg);
    }
    
    view.mark = recalculate_caret(view.mark, view.buffer, view.lines);
    if (view.mark.wrapped_line >= view.line_offset && 
        view.mark.wrapped_line < view.line_offset + view.lines_visible)
    {
        i32 y = view.mark.wrapped_line - view.line_offset;
        f32 w = app.mono.space_width/2;
        f32 h = app.mono.line_height - 2.0f;

        Vector2 p0{ 
            view.rect.pos.x + view.mark.wrapped_column*app.mono.space_width,
            view.rect.pos.y + y*app.mono.line_height - view.voffset,
        };
        Vector2 p1{ p0.x, p0.y + h };

        gui_draw_rect(&gfx.frame_cmdbuf, p0, { 1.0f, h }, app.mark_fg);
        gui_draw_rect(&gfx.frame_cmdbuf, p0, { w, 1.0f }, app.mark_fg);
        gui_draw_rect(&gfx.frame_cmdbuf, p1, { w, 1.0f }, app.mark_fg);
    }

    Buffer *buffer = get_buffer(view.buffer);
    if (buffer) {
        GfxCommand cmd = gui_command(GFX_COMMAND_GUI_TEXT);
        cmd.gui_text.font_atlas = app.mono.texture;
        cmd.gui_text.color = linear_from_sRGB(app.fg);
        
        f32 voffset = view.voffset;
        i32 line_offset = view.line_offset;

        for (i32 i = 0; i < MIN(view.lines.count - line_offset, view.lines_visible); i++) {
            i32 line_index = i + line_offset;
            
            Vector2 baseline{ view.rect.pos.x, view.rect.pos.y + i*app.mono.line_height + (f32)app.mono.baseline - voffset };
            Vector2 pen = baseline;

            i64 p = view.lines[line_index].offset;
            i64 end = line_end_offset(line_index, view.lines, buffer);
            
            i64 vcolumn = 0;
            while (p < end) {
                i32 c = utf32_it_next(buffer, &p);
                if (c == 0) break;

                if (c == '\n') {
                    if (p < end && char_at(buffer, p) == '\r') p = next_byte(buffer, p);
                    vcolumn = 0;
                    continue;
                }

                if (c == '\r') {
                    if (p < end && char_at(buffer, p) == '\n') p = next_byte(buffer, p);
                    vcolumn = 0;
                    continue;
                }

                if (c == ' ') {
                    pen.x += app.mono.space_width;
                    vcolumn++;
                    continue;
                }
                
                if (c == '\t') {
                    i32 w = app.tab_width - vcolumn % app.tab_width;
                    pen.x += app.mono.space_width*w;
                    vcolumn += w;
                    continue;
                }

                stbtt_aligned_quad q;
                stbtt_GetBakedQuad(app.mono.atlas, 1024, 1024, c, &pen.x, &pen.y, &q, 1);
                
                // NOTE(jesper): if this fires then we haven't done line reflowing correctly
                ASSERT(q.x1 < view.rect.pos.x + view.rect.size.x);

                // TODO(jesper): the way this text rendering works it'd probably be far cheaper for us to just 
                // overdraw a background outside the view rect
                if (apply_clip_rect(&q, view.rect)) {
                    f32 vertices[] = { 
                        q.x0, q.y0, q.s0, q.t0,
                        q.x0, q.y1, q.s0, q.t1,
                        q.x1, q.y1, q.s1, q.t1,

                        q.x1, q.y1, q.s1, q.t1,
                        q.x1, q.y0, q.s1, q.t0,
                        q.x0, q.y0, q.s0, q.t0,
                    };

                    array_add(&gui.vertices, vertices, ARRAY_COUNT(vertices));
                    cmd.gui_text.vertex_count += ARRAY_COUNT(vertices);
                }
                vcolumn++;
            }
        }
        
        // TODO(jesper): I'm lazy and re-using the GUI's vertex buffer here, which is why this is using
        // gui draw procedures and being kind of weird as hell about it. This also makes some causes
        // some error-prone weirdness because we need to end the gui frame, submit the standard command buffers
        // and only then submit the gui draw commands. I don't much like having gui_end_frame separate from 
        // gui_render, and this interaction is just weird.
        gui_push_command(&gfx.frame_cmdbuf, cmd);
    }
    
    Vector3 clear_color = linear_from_sRGB(app.bg);
    glClearColor(clear_color.r, clear_color.g, clear_color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    gui_end_layout();
    gui_end_frame();

    gfx_flush_transfers();
    gfx_submit_commands(gfx.frame_cmdbuf);
    gfx_submit_commands(debug_gfx);

    gui_render();
    
    app.animating = false;
    
    // NOTE(jesper): this is something of a hack because WM_CHAR messages come after the WM_KEYDOWN, and
    // we're listening to WM_KEYDOWN to determine whether to switch modes, so the actual mode switch has to
    // be deferred.
    app.mode = app.next_mode;
}