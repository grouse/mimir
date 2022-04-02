#include "input.h"

#include "maths.cpp"
#include "gui.cpp"
#include "string.cpp"
#include "allocator.cpp"
#include "core.cpp"
#include "gfx_opengl.cpp"
#include "assets.cpp"
#include "font.cpp"
#include "fzy.cpp"

#define DEBUG_LINE_WRAP_RECALC 0

enum EditMode {
    MODE_EDIT,
    MODE_INSERT,
    MODE_DIALOG,
};

enum BufferHistoryType {
    BUFFER_INSERT,
    BUFFER_REMOVE,

    BUFFER_CURSOR_POS,
    BUFFER_HISTORY_GROUP_START,
    BUFFER_HISTORY_GROUP_END
};

enum ViewFlags : u32 {
    VIEW_SET_MARK = 1 << 0,
};

struct Range_i64 {
    i64 start, end;
};

struct BufferLine {
    i64 offset : 63;
    i64 wrapped : 1;
};

struct Application {
    Font mono;
    i32 tab_width = 4;
    bool animating = true;
    
    struct {
        bool active;
        DynamicArray<String> values;
        DynamicArray<String> filtered;
        
        i32 selected_item;
    } lister;

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

#define BUFFER_INVALID BufferId{ -1 }

struct BufferId {
    i32 index = -1;

    bool operator==(const BufferId &rhs) { return index == rhs.index; }
    bool operator!=(const BufferId &rhs) { return index != rhs.index; }
    operator bool() { return *this != BUFFER_INVALID; }
};


struct Buffer {
    BufferId id;
    
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

struct Caret {
    i64 byte_offset = 0;
    
    i64 wrapped_column = 0;
    i64 column = 0;
    i64 preferred_column = 0;

    i32 line = 0;
    i32 wrapped_line = 0;
};

struct ViewBufferState {
    BufferId buffer;
    Caret caret, mark;
    
    f32 voffset;
    i32 line_offset;
};

struct View {
    GuiId gui_id = GUI_ID(0);
    
    DynamicArray<BufferId> buffers;
    DynamicArray<ViewBufferState> saved_buffer_state;
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

void move_view_to_caret();


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
    // write a binary blob visualiser, or at least something that won't choke or
    // crash trying to render invalid text
    
    Buffer b{ .type = BUFFER_FLAT };
    b.id = { .index = buffers.count };
    b.file_path = absolute_path(file, mem_dynamic);
    b.name = filename_of(b.file_path);
    b.newline_mode = NEWLINE_LF;
    
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
    
    String newline_str = string_from_enum(b.newline_mode);
    LOG_INFO("created buffer: %.*s, newline mode: %.*s", STRFMT(file), STRFMT(newline_str));
    
    array_add(&buffers, b);
    return b.id;
}

BufferId find_buffer(String file)
{
    for (auto b : buffers) {
        if (b.file_path == file) return b.id;
    }
    
    return BUFFER_INVALID;
}

void view_set_buffer(BufferId buffer)
{
    if (view.buffer == buffer) return;

    auto find = [](BufferId buffer) -> ViewBufferState*
    {
        for (auto &state : view.saved_buffer_state) {
            if (state.buffer == buffer) return &state;
        }
        return nullptr;
    };

    ViewBufferState state{
        .buffer = view.buffer,
        .caret = view.caret,
        .mark = view.mark,
        .voffset = view.voffset,
        .line_offset = view.line_offset,
    };

    ViewBufferState *it = find(view.buffer);
    if (!it) array_add(&view.saved_buffer_state, state);
    else *it = state;

    it = find(buffer);
    if (it) {
        view.caret = it->caret;
        view.mark = it->mark;
        view.voffset = it->voffset;
        view.line_offset = it->line_offset;
    } else {
        view.caret = view.mark = {};
        view.voffset = 0;
        view.line_offset = 0;
    }

    view.buffer = buffer;

    i32 idx = array_find_index(view.buffers, view.buffer);
    if (idx >= 0) array_remove_sorted(&view.buffers, idx);
    array_insert(&view.buffers, 0, view.buffer);

    // NOTE(jesper): not storing line wrap data (for now). Computing it is super quick, and storing
    // it will add up pretty quick. Especially if we start serialising the views and their state
    // to disk
    view.lines_dirty = true;
}

void init_app()
{
    String asset_folders[] = {
        "./",
        "../assets",
        get_exe_folder(),
        join_path(get_exe_folder(), "../assets"),
    };
    init_assets({ asset_folders, ARRAY_COUNT(asset_folders) });
    
    init_gui();
    
    app.mono = create_font("fonts/UbuntuMono/UbuntuMono-Regular.ttf", 18);
    
    calculate_num_visible_lines();
    
    //view_set_buffer(create_buffer("src/gui.cpp"));
    //view_set_buffer(create_buffer("src/gui.h"));
    //view_set_buffer(create_buffer("src/maths.cpp"));
    //view_set_buffer(create_buffer("src/maths.h"));
    //view_set_buffer(create_buffer("src/mimir.cpp"));

    fzy_init_table();
}


void app_open_file(String path)
{
    view_set_buffer(create_buffer(path));
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
    case BUFFER_FLAT: return utf8_decr(buffer->flat.data, i);
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

i64 line_start_offset(i32 wrapped_line, Array<BufferLine> lines)
{
    return lines[wrapped_line].offset;
}

i64 line_end_offset(i32 wrapped_line, Array<BufferLine> lines, Buffer *buffer)
{
    switch (buffer->type) {
    case BUFFER_FLAT: return (wrapped_line+1) < lines.count ? lines[wrapped_line+1].offset : buffer->flat.size;
    }
}

i64 line_end_offset(i32 wrapped_line, Array<BufferLine> lines, BufferId buffer_id)
{
    return line_end_offset(wrapped_line, lines, &buffers[buffer_id.index]);
}

Range_i64 caret_range(Caret c0, Caret c1, Array<BufferLine> lines, BufferId buffer, bool lines_block)
{
    Range_i64 r;
    
    if (lines_block) {
        i32 start_line = MIN(c0.wrapped_line, c1.wrapped_line);
        i32 end_line = MAX(c0.wrapped_line, c1.wrapped_line);
        
        r.start = line_start_offset(start_line, lines);
        r.end = line_end_offset(end_line, lines, buffer);
    } else {
        r.start = MIN(c0.byte_offset, c1.byte_offset);
        r.end = MAX(c0.byte_offset, c1.byte_offset);
    }
    
    if (r.start == r.end) r.end += 1;
    return r;
}

void recalculate_line_wrap(i32 wrapped_line, DynamicArray<BufferLine> *existing_lines, BufferId buffer_id)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return;
    
    Rect r = view.rect;
    f32 base_x = r.pos.x;

    i64 last_line = wrapped_line;
    while (last_line+1 < existing_lines->count && existing_lines->at(last_line+1).wrapped) last_line++;
    i64 end_line = MIN(last_line+1, existing_lines->count);

    i64 start = 0;
    i64 end = line_end_offset(last_line, *existing_lines, buffer);

    DynamicArray<BufferLine> tmp_lines{ .alloc = mem_tmp };
    DynamicArray<BufferLine> *new_lines = &tmp_lines;

    if (wrapped_line < existing_lines->count) {
        array_add(new_lines, existing_lines->at(wrapped_line));
        start = existing_lines->at(wrapped_line).offset;
    } else if (existing_lines->count == 0) {
        new_lines = existing_lines;
        array_add(new_lines, BufferLine{ 0, 0 });
    } else {
        LOG_ERROR("unimplemented, this is a case where we're appending new lines to the existing lines");
    }

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
            if (p < end && char_at(buffer, p) == '\r') p = next_byte(buffer, p);
            line_start = word_start = p;
            array_add(new_lines, { (i64)line_start, 0 });
            vcolumn = 0;
            continue;
        }

        if (c == '\r') {
            if (p < end && char_at(buffer, p) == '\n') p = next_byte(buffer, p);
            line_start = word_start = p;
            array_add(new_lines, { (i64)line_start, 0 });
            vcolumn = 0;
            continue;
        }

        if (c == ' ') {
            vcolumn++;
            continue;
        }

        if (c == '\t') {
            i32 w = app.tab_width - vcolumn % app.tab_width;
            vcolumn += w;
            continue;
        }

        f32 x1 = base_x + (vcolumn+1) * app.mono.space_width;
        if (x1 >= r.pos.x + r.size.x) {
            if (!is_word_boundary(c) && line_start != word_start) { 
                p = line_start = word_start;
                array_add(new_lines, { (i64)line_start, 1 });
                vcolumn = 0;
                continue;
            } else {
                p = line_start = word_start = pn;
                array_add(new_lines, { (i64)line_start, 1 });
                vcolumn = 0;
                continue;
            }
        }

        vcolumn++;
    }
    
    Array<BufferLine> lines = *new_lines;
    if (end != buffer_end(buffer)) lines = slice(*new_lines, 0, new_lines->count-1);
    
    if (new_lines != existing_lines) {
        if (wrapped_line == end_line) {
#if DEBUG_LINE_WRAP_RECALC
            LOG_INFO("inserting %d new lines at %d", lines.count, wrapped_line);
            for (i32 i = 0; i < lines.count; i++) LOG_RAW("\tnew line[%d]: %lld\n", i, lines[i].offset);
            for (i32 i = 0; i < existing_lines->count; i++) LOG_RAW("\texisting line[%d]: %lld\n", i, existing_lines->at(i).offset);
#endif
            array_insert(existing_lines, wrapped_line, lines);
#if DEBUG_LINE_WRAP_RECALC
            for (i32 i = 0; i < existing_lines->count; i++) LOG_RAW("\tresulting line[%d]: %lld\n", i, existing_lines->at(i).offset);
#endif
        } else {
#if DEBUG_LINE_WRAP_RECALC
            LOG_INFO("replacing [%d, %d] old lines with %d new lines at %d", wrapped_line, end_line, lines.count, wrapped_line);
            for (i32 i = 0; i < lines.count; i++) LOG_RAW("\tnew line[%d]: %lld\n", i, lines[i].offset);
            for (i32 i = 0; i < existing_lines->count; i++) LOG_RAW("\texisting line[%d]: %lld\n", i, existing_lines->at(i).offset);
#endif
            array_replace_range(existing_lines, wrapped_line, end_line, lines);
            
#if DEBUG_LINE_WRAP_RECALC
            for (i32 i = 0; i < existing_lines->count; i++) LOG_RAW("\tresulting line[%d]: %lld\n", i, existing_lines->at(i).offset);
#endif
        }
    }
}

bool buffer_remove(BufferId buffer_id, i64 byte_start, i64 byte_end, bool record_history = true)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return false;
    
    switch (buffer->type) {
    case BUFFER_FLAT:
        byte_start = MAX(0, byte_start);
        byte_end = MIN(byte_end, buffer->flat.size);
        
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
        break;
    }
    
    return false;
}

String buffer_read(BufferId buffer_id, i64 byte_start, i64 byte_end, Allocator mem = mem_tmp)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return {};

    String s{};
    
    switch (buffer->type) {
    case BUFFER_FLAT:
        byte_start = MAX(0, byte_start);
        byte_end = MIN(byte_end, buffer->flat.size);

        i64 num_bytes = byte_end-byte_start;
        
        s.length = num_bytes;
        s.data = ALLOC_ARR(mem, char, s.length);
        memcpy(s.data, &buffer->flat.data[byte_start], num_bytes);
        
        break;
    }

    return s;
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

i32 wrapped_line_from_offset(i64 offset, i32 guessed_line, Array<BufferLine> lines)
{
    i32 line = guessed_line;
    while (line > 0 && offset < lines[line].offset) line--;
    while (line < (lines.count-2) && offset > lines[line+1].offset) line++;
    return line;
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
    
    // TODO(jesper): multi-view support
    if (view.buffer == buffer_id) {
        i32 line = wrapped_line_from_offset(offset, view.caret.wrapped_line, view.lines);
        for (i32 i = line+1; i < view.lines.count; i++) {
            view.lines[i].offset += text.length;
        }
        recalculate_line_wrap(line, &view.lines, view.buffer);
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
    
    // TODO(jesper): maybe we should just store the view coordinates in the buffer history?
    move_view_to_caret();
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
    
    // TODO(jesper): maybe we should just store the view coordinates in the buffer history?
    move_view_to_caret();
}


i64 buffer_prev_offset(Buffer *buffer, i64 byte_offset)
{
    ASSERT(buffer);
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

i64 buffer_prev_offset(BufferId buffer_id, i64 byte_offset)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return byte_offset;
    return buffer_prev_offset(buffer, byte_offset);
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

i64 buffer_increment_offset(BufferId buffer_id, i64 byte_offset, i64 count)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return byte_offset;

    i64 end_offset = buffer_end(buffer_id);
    if (byte_offset >= end_offset) return end_offset;

    switch (buffer->type) {
    case BUFFER_FLAT:
        return MIN(end_offset, byte_offset+count);
    }

    return byte_offset;
}

bool line_is_empty_or_whitespace(BufferId buffer_id, Array<BufferLine> lines, i32 wrapped_line)
{
    Buffer *buffer = get_buffer(buffer_id);
    ASSERT(buffer);
    
    i64 start = lines[wrapped_line].offset;
    i64 end = line_end_offset(wrapped_line, lines, buffer);
    
    i64 p = prev_byte(buffer, end);
    while (p >= start) {
        if (!is_whitespace(char_at(buffer, p))) return false;
        p = prev_byte(buffer, p);
    }
    
    return true;
}

i32 seek_non_empty_line_back(BufferId buffer_id, Array<BufferLine> lines, i32 start_line)
{
    i32 line = start_line;
    while (line > 0 && (lines[line].wrapped || line_is_empty_or_whitespace(buffer_id, lines, line))) line--;
    return line;
}

i32 buffer_seek_next_empty_line(BufferId buffer_id, Array<BufferLine> lines, i32 wrapped_line)
{
    if (wrapped_line >= lines.count-1) return wrapped_line;
    bool had_non_empty = !line_is_empty_or_whitespace(buffer_id, lines, wrapped_line);
    
    i32 line = wrapped_line+1;
    for (; line < lines.count; line++) {
        if (line_is_empty_or_whitespace(buffer_id, lines, line)) {
            if (had_non_empty ) return line;
        } else {
            had_non_empty = true;
        }
    }
    return MIN(line, lines.count-1);
}

i32 buffer_seek_prev_empty_line(BufferId buffer_id, Array<BufferLine> lines, i32 wrapped_line)
{
    if (wrapped_line <= 1) return 0;
    bool had_non_empty = !line_is_empty_or_whitespace(buffer_id, lines, wrapped_line);
    
    i32 line = wrapped_line-1;
    for (; line >= 0; line--) {
        if (line_is_empty_or_whitespace(buffer_id, lines, line)) {
            if (had_non_empty) return line;
        } else {
            had_non_empty = true;
        }
    }
    return MAX(line, 0);
}

i64 buffer_seek_next_word(BufferId buffer_id, i64 byte_offset)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return byte_offset;

    switch (buffer->type) {
    case BUFFER_FLAT: {
            i64 offset = byte_offset;
            i64 end = buffer_end(buffer_id);
            
            if (byte_offset >= end) return end;
            
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
            
            return offset;
        } break;
    }
    
    return byte_offset;
}

i64 buffer_seek_beginning_of_line(BufferId buffer_id, Array<BufferLine> lines, i32 line)
{
    if (line >= lines.count) return 0;
    
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return 0;
    
    i64 start = lines[line].offset;
    i64 end = line_end_offset(line, lines, buffer);
    
    i64 offset = start;
    while (offset < end) {
        if (!is_whitespace(char_at(buffer, offset))) return offset;
        offset = next_byte(buffer, offset);
    }
    
    return offset;
}

i64 buffer_seek_end_of_line(BufferId buffer_id, Array<BufferLine> lines, i32 line)
{
    if (line >= lines.count) return 0;

    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return 0;
    
    if (line == lines.count-1) return buffer_end(buffer_id);

    i64 end = line_end_offset(line, lines, buffer);
    return buffer_prev_offset(buffer, end);
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

String get_indent_for_line(BufferId buffer_id, Array<BufferLine> lines, i32 line)
{
    line = seek_non_empty_line_back(buffer_id, lines, line);
    i64 line_start = buffer_seek_beginning_of_line(buffer_id, lines, line);
    i64 abs_start = line_start_offset(line, lines);
    return buffer_read(buffer_id, abs_start, line_start);
}

i64 caret_nth_offset(BufferId buffer_id, i64 current, i32 n)
{
    i64 offset = current;
    while (n--) offset = buffer_next_offset(buffer_id, offset);
    return offset;
}

i64 wrapped_column_count(i32 wrapped_line, Array<BufferLine> lines, Buffer *buffer)
{
    if (wrapped_line >= lines.count) return 0;
    
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
    if (wrapped_line >= lines.count) return 0;
    
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
    if (wrapped_line >= lines.count) return 0;
    
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
    if (wrapped_line >= lines.count) return 0;
    
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
    if (wrapped_line >= lines.count) return 0;
    
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
    if (wrapped_line >= lines.count) return 0;
    
    i64 offset = lines[wrapped_line].offset;
    
    for (i64 c = 0; c < wrapped_column; c++) {
        if (char_at(buffer, offset) == '\t') c += app.tab_width - c % app.tab_width - 1;
        offset = utf8_incr(buffer, offset);
    }
    
    return offset;
}

void recalc_caret_line(i32 new_wrapped_line, i32 *wrapped_line, i32 *line, Array<BufferLine> lines)
{
    if (new_wrapped_line == 0) {
        *wrapped_line = 0;
        *line = 0;
        return;
    } else if (new_wrapped_line == *wrapped_line) return;
    
    if (*wrapped_line >= lines.count) {
        *wrapped_line = MIN(new_wrapped_line, lines.count-1);
        *line = 0;
        for (i32 i = 0; i < *wrapped_line; i++) {
            if (!lines[i].wrapped) *line = *line + 1;
        }
        
        return;
    }
    
    while (*wrapped_line > new_wrapped_line) {
        if (!lines[(*wrapped_line)--].wrapped) (*line)--;
    }

    while (*wrapped_line < new_wrapped_line) {
        if (!lines[++(*wrapped_line)].wrapped) (*line)++;
    }
}

bool app_needs_render()
{
    return app.next_mode != app.mode || app.animating || view.lines_dirty || view.caret_dirty;
}


Caret recalculate_caret(Caret caret, BufferId buffer_id, Array<BufferLine> lines)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return caret;

    if (caret.byte_offset == 0) caret.wrapped_line = 0;
    else if (caret.byte_offset >= buffer_end(buffer_id)) {
        caret.byte_offset = buffer_end(buffer_id);
        recalc_caret_line(lines.count-1, &caret.wrapped_line, &caret.line, lines);
    } else {
        caret.wrapped_line = MIN(caret.wrapped_line, lines.count-1);
        caret.wrapped_line = MAX(0, caret.wrapped_line);
        
        while (caret.byte_offset < lines[caret.wrapped_line].offset) {
            if (!lines[caret.wrapped_line--].wrapped) caret.line--;
        }
        
        while (caret.byte_offset >= line_end_offset(caret.wrapped_line, lines, buffer)) {
            if (!lines[++caret.wrapped_line].wrapped) caret.line++;
        }
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
    } else if (view.caret.wrapped_line+line_padding > view.line_offset + view.lines_visible-4) {
        view.line_offset = MIN(view.lines.count-1, view.caret.wrapped_line+line_padding - view.lines_visible+4);
    }
}

// NOTE(jesper): this is poorly named. It starts/stops a history group and records the active view's
// caret and mark appropriately so that undo/redo also moves the carets appropriately
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

void view_seek_line(i32 wrapped_line)
{
    wrapped_line = CLAMP(wrapped_line, 0, view.lines.count-1);
    Buffer *buffer = get_buffer(view.buffer);
    
    if (buffer && wrapped_line != view.caret.wrapped_line) {
        while (view.caret.wrapped_line > wrapped_line) {
            if (!view.lines[view.caret.wrapped_line--].wrapped) view.caret.line--;
        }
        
        while (view.caret.wrapped_line < wrapped_line) {
            if (!view.lines[++view.caret.wrapped_line].wrapped) view.caret.line++;
        }

        view.caret.wrapped_column = calc_wrapped_column(view.caret.wrapped_line, view.caret.preferred_column, view.lines, buffer);
        view.caret.column = calc_unwrapped_column(view.caret.wrapped_line, view.caret.wrapped_column, view.lines, buffer);
        view.caret.byte_offset = calc_byte_offset(view.caret.wrapped_column, view.caret.wrapped_line, view.lines, buffer);

        move_view_to_caret();

        app.animating = true;
    }
}

void view_seek_byte_offset(i64 byte_offset)
{
    byte_offset = CLAMP(byte_offset, 0, buffer_end(view.buffer));
    
    if (byte_offset != view.caret.byte_offset) {
        view.caret.byte_offset = byte_offset;
        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
        view.mark = recalculate_caret(view.mark, view.buffer, view.lines);

        move_view_to_caret();

        app.animating = true;
    }
}

void write_string(BufferId buffer_id, String str, u32 flags = 0)
{
    if (!buffer_valid(buffer_id)) return;
    
    BufferHistoryScope h(buffer_id);
    buffer_insert(buffer_id, view.caret.byte_offset, str);
    
    if (flags & VIEW_SET_MARK) view.mark = view.caret;
    view.caret.byte_offset = buffer_increment_offset(buffer_id, view.caret.byte_offset, str.length);
    view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
    view.mark = recalculate_caret(view.mark, view.buffer, view.lines);

    move_view_to_caret();
    
    app.animating = true;
}

void app_event(InputEvent event)
{
    if (gui_input(event)) {
        app.animating = true;
        return;
    }
    
    // NOTE(jesper): the input processing procedure contains a bunch of buffer undo history
    // management in order to record the cursor position in the history groups, so that when
    // a user undo/redo the cursor is repositioned to where it was before/after the edit 

    // TODO(jesper): figure out if there's a better way for us to signal that the event results
    // in an update/redraw. If nothing else, the app.animating variable name just feels wrong at
    // this point, especially when I don't even have anything animating
    
    switch (event.type) {
    case IE_TEXT: 
        if (app.mode == MODE_INSERT) {
            write_string(view.buffer, String{ (char*)&event.text.c[0], event.text.length });
        } 
        break;
    case IE_KEY_PRESS: 
        app.animating = true;
        if (app.mode == MODE_EDIT) {
            switch (event.key.virtual_code) {
            case VC_Q:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_byte_offset(buffer_seek_beginning_of_line(view.buffer, view.lines, view.caret.wrapped_line));
                break;
            case VC_E:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_byte_offset(buffer_seek_end_of_line(view.buffer, view.lines, view.caret.wrapped_line));
                break;
            case VC_CLOSE_BRACKET:
                if (event.text.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_line(buffer_seek_next_empty_line(view.buffer, view.lines, view.caret.wrapped_line));
                break;
            case VC_OPEN_BRACKET:
                if (event.text.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_line(buffer_seek_prev_empty_line(view.buffer, view.lines, view.caret.wrapped_line));
                break;
            case VC_U:
                buffer_undo(view.buffer);
                view.lines_dirty = true;
                view.caret_dirty = true;
                break;
            case VC_R:
                buffer_redo(view.buffer);
                view.lines_dirty = true;
                view.caret_dirty = true;
                break;
            case VC_M:
                if (event.key.modifiers == MF_CTRL) SWAP(view.mark, view.caret);
                else view.mark = view.caret;
                break;
            case VC_O:
                app.lister.active = true;
                app.lister.selected_item = 0;
                app.lister.values.count = 0;
                list_files(&app.lister.values, "./", FILE_LIST_RECURSIVE, mem_dynamic);
                array_copy(&app.lister.filtered, app.lister.values);
                
                app.mode = MODE_DIALOG;
                break;
            case VC_D: {
                    BufferHistoryScope h(view.buffer);
                    Range_i64 r = caret_range(view.caret, view.mark, view.lines, view.buffer, event.key.modifiers == MF_CTRL);
                    if (buffer_remove(view.buffer, r.start, r.end)) {
                        // TODO(jesper): recalculate only the lines affected, and short-circuit the caret re-calc
                        view.lines_dirty = true;

                        view.caret.byte_offset = r.start;
                        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                        view.mark = view.caret;

                        move_view_to_caret();
                    }
                } break;
            case VC_X: {
                    BufferHistoryScope h(view.buffer);
                    
                    Range_i64 r = caret_range(view.caret, view.mark, view.lines, view.buffer, event.key.modifiers == MF_CTRL);
                    String str = buffer_read(view.buffer, r.start, r.end);
                    set_clipboard_data(str);
                    
                    if (buffer_remove(view.buffer, r.start, r.end)) {
                        view.lines_dirty = true;

                        view.caret.byte_offset = r.start;
                        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                        view.mark = view.caret;

                        move_view_to_caret();
                    }
                } break;
            case VC_Y: {
                    Range_i64 r = caret_range(view.caret, view.mark, view.lines, view.buffer, event.key.modifiers == MF_CTRL);
                    set_clipboard_data(buffer_read(view.buffer, r.start, r.end));
                } break;
            case VC_P:
                write_string(view.buffer, read_clipboard_str(), VIEW_SET_MARK);
                break;
            case VC_W: 
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_byte_offset(buffer_seek_next_word(view.buffer, view.caret.byte_offset));
                break;
            case VC_B:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_byte_offset(buffer_seek_prev_word(view.buffer, view.caret.byte_offset));
                break;
            case VC_J:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_line(view.caret.wrapped_line+1);
                break;
            case VC_K:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_line(view.caret.wrapped_line-1);
                break;
            case VC_I:
                app.next_mode = MODE_INSERT;
                view.mark = view.caret;
                break;
            case VC_L: 
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                view.caret.byte_offset = buffer_next_offset(view.buffer, view.caret.byte_offset);
                view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                move_view_to_caret();
                break;
            case VC_H:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                view.caret.byte_offset = buffer_prev_offset(view.buffer, view.caret.byte_offset);
                view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                move_view_to_caret();
                break;
            case VC_S:
                if (event.key.modifiers == MF_CTRL) buffer_save(view.buffer);
                break;
            default: break;
            }
        } else if (app.mode == MODE_INSERT) {
            switch (event.key.virtual_code) {
            case VC_ESC:
                app.next_mode = MODE_EDIT;
                break;
            case VC_LEFT: 
                view.caret.byte_offset = buffer_prev_offset(view.buffer, view.caret.byte_offset);
                view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                move_view_to_caret();
                break;
            case VC_RIGHT:
                view.caret.byte_offset = buffer_next_offset(view.buffer, view.caret.byte_offset);
                view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                move_view_to_caret();
                break;
            case VC_ENTER: {
                    BufferHistoryScope h(view.buffer);
                    String indent = get_indent_for_line(view.buffer, view.lines, view.caret.wrapped_line);
                    write_string(view.buffer, buffer_newline_str(view.buffer));
                    write_string(view.buffer, indent);
                } break;
            case VC_TAB: 
                write_string(view.buffer, "\t");
                break;
            case VC_BACKSPACE: 
                if (buffer_valid(view.buffer)) {
                    BufferHistoryScope h(view.buffer);
                    i64 start = buffer_prev_offset(view.buffer, view.caret.byte_offset);
                    if (buffer_remove(view.buffer, start, view.caret.byte_offset)) {
                        // TODO(jesper): recalculate only the lines affected, and short-circuit the caret re-calc
                        view.lines_dirty = true;
                        view.caret.byte_offset = start;
                        view.mark = view.caret;
                        view.caret_dirty = true; 
                        
                        //view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                        // TODO(jesper): what do I want here?
                        //view.mark = view.caret;

                        //move_view_to_caret();
                    }
                } break;
            case VC_DELETE: 
                if (buffer_valid(view.buffer)) {
                    BufferHistoryScope h(view.buffer);
                    i64 end = buffer_next_offset(view.buffer, view.caret.byte_offset);
                    if (buffer_remove(view.buffer, view.caret.byte_offset, end)) {
                        
                        //view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                        //view.mark = view.caret;
                        view.mark = view.caret;
                        view.caret_dirty = true;

                        // TODO(jesper0: recalculate only lines affected
                        view.lines_dirty = true;
                    }
                } break;
                
            default: break;
            }
        }
        
        if (app.mode == MODE_EDIT || app.mode == MODE_INSERT) {
            switch (event.key.virtual_code) {
            case VC_PAGE_DOWN: 
                // TODO(jesper: page up/down should take current caret line into account when moving
                // view to caret, so that it remains where it was proportionally
                if (event.text.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_line(view.caret.wrapped_line+view.lines_visible-1);
                break;
            case VC_PAGE_UP:
                // TODO(jesper: page up/down should take current caret line into account when moving
                // view to caret, so that it remains where it was proportionally
                if (event.text.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_line(view.caret.wrapped_line-view.lines_visible-1);
                break;
            default: break;
            }
        }
        break;
    case IE_KEY_RELEASE: break;
    case IE_MOUSE_PRESS:
        if (gui.hot == view.gui_id && event.mouse.button == 1) {
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
                
                app.animating = true;
            }
        } break;
    case IE_MOUSE_WHEEL:
        // TODO(jesper): make a decision/option whether this should be moving
        // the caret or not. My current gut feeling says no, as a way to allow a
        // kind of temporary scroll back to check on something, that you then get
        // taken back to the current caret when you type something

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
    
    gui_menu() {
        gui_menu("file") {
            if (gui_menu_button("save")) buffer_save(view.buffer);
        }

        gui_menu("debug") {
            gui_checkbox("buffer history", &debug.buffer_history.wnd.active);
            gui_menu_button("foobar");
            gui_menu_button("foobar");
            gui_menu_button("foobar");
            gui_checkbox("buffer history", &debug.buffer_history.wnd.active);
        }
    }
    
    gui_window("buffer history", &debug.buffer_history.wnd) {
        
        static i32 val = 1;
        val = gui_dropdown({ "A", "B", "C" }, { 1, 2, 3 }, val);
        if (gui_button("foobar")) LOG_INFO("CLICK");
        
        gui_editbox("foobar");
        
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
    }

    // TODO(jesper): something smarter to use more of the space in small windows without taking up _all_ the space in
    // large windows
    f32 lister_w = gfx.resolution.x*0.7f;
    gui_window("fuzzy lister", gfx.resolution * 0.5f, { lister_w, 200.0f }, { 0.5f, 0.5f }, app.lister.active, 0) {
        GuiEditboxAction edit_action = gui_editbox("");
        
        if (edit_action & (GUI_EDITBOX_CHANGE)) {
            String needle{ gui.edit.buffer, gui.edit.length };
            if (needle.length == 0) {
                array_copy(&app.lister.filtered, app.lister.values);
            } else {
                String lneedle = to_lower(needle);

                DynamicArray<fzy_score_t> scores{ .alloc = mem_tmp };
                array_reserve(&scores, app.lister.values.count);

                app.lister.filtered.count = 0;

                DynamicArray<fzy_score_t> D{ .alloc = mem_tmp };
                DynamicArray<fzy_score_t> M{ .alloc = mem_tmp };
                DynamicArray<fzy_score_t> match_bonus{ .alloc = mem_tmp };

                for (String s : app.lister.values) {
                    fzy_score_t match_score;
                    
                    array_resize(&D, needle.length*s.length);
                    array_resize(&M, needle.length*s.length);

                    memset(D.data, 0, D.count*sizeof *D.data);
                    memset(M.data, 0, M.count*sizeof *M.data);

                    array_resize(&match_bonus, s.length);
                    
                    String ls = to_lower(s);
                    char prev = '/';
                    for (i32 i = 0; i < ls.length; i++) {
                        if (ls[i] == '\\') ls[i] = '/';
                        match_bonus[i] = fzy_compute_bonus(prev, ls[i]);
                        
                    }
                    
                    for (i32 i = 0; i < needle.length; i++) {
                        fzy_score_t prev_score = FZY_SCORE_MIN;
                        fzy_score_t gap_score = i == needle.length-1 ? FZY_SCORE_GAP_TRAILING : FZY_SCORE_GAP_INNER;

                        char ln = lneedle[i];
                        for (i32 j = 0; j < s.length; j++) {
                            if (ln == ls[j]) {
                                fzy_score_t score = FZY_SCORE_MIN;
                                if (!i) {
                                    score = (j*FZY_SCORE_GAP_LEADING) + match_bonus[j];
                                } else if (j) {
                                    fzy_score_t d_val = D[(i-1) * s.length + j-1];
                                    fzy_score_t m_val = M[(i-1) * s.length + j-1];
                                    score = MAX(m_val+match_bonus[j], d_val+FZY_SCORE_MATCH_CONSECUTIVE);
                                }

                                D[i*s.length + j] = score;
                                M[i*s.length + j] = prev_score = MAX(score, prev_score + gap_score);
                            } else {
                                D[i*s.length + j] = FZY_SCORE_MIN;
                                M[i*s.length + j] = prev_score = prev_score + gap_score;
                            }
                        }

                        if (prev_score == FZY_SCORE_MIN) goto next_node;
                    }

                    match_score = M[(needle.length-1) * s.length + s.length-1];
                    if (match_score != FZY_SCORE_MIN) {
                        array_add(&app.lister.filtered, s);
                        array_add(&scores, match_score);
                    }
next_node:;
                }

                sort(scores, app.lister.filtered);
            }
        }
        
        GuiListerAction lister_action = gui_lister(app.lister.filtered, &app.lister.selected_item);
        if (lister_action == GUI_LISTER_FINISH || edit_action == GUI_EDITBOX_FINISH) {
            String file = app.lister.filtered[app.lister.selected_item];
            String path = absolute_path(file);
            
            BufferId buffer = find_buffer(path);
            if (!buffer) buffer = create_buffer(path);
            
            view_set_buffer(buffer);
            
            app.lister.active = false;
            gui_clear_hot();
            gui.focused = GUI_ID_INVALID;
        } else if (lister_action == GUI_LISTER_CANCEL) {
            // TODO(jesper): this results in 1 frame of empty lister window being shown because
            // we don't really have a good way to close windows from within the window
            app.lister.active = false;
        }
    }
    
    view.caret_dirty |= view.lines_dirty;
    {
        Rect tr = gui_layout_widget_fill();
        gui_begin_layout({ .type = GUI_LAYOUT_ROW, .rect = tr });
        defer { gui_end_layout(); };
        
        if (true) {
            GuiWindow *wnd = gui_current_window();
            
            Vector2 size{ 0, gui.style.button.font.line_height + 2 };
            Vector2 pos = gui_layout_widget(&size);
            gui_draw_rect(pos, size, wnd->clip_rect, gui.style.bg_dark1);
            
            gui_divider();
            
            gui_begin_layout({ .type = GUI_LAYOUT_COLUMN, .pos = pos, .size = size, .column.spacing = 2 });
            defer { gui_end_layout(); };
            
            Font *font = &gui.style.button.font;
            Vector2 msize{ gui.style.button.font.line_height, gui.style.button.font.line_height };
            Vector2 mpos = gui_layout_widget(msize, GUI_ANCHOR_RIGHT);
            
            i32 current_tab = 0;
            GuiId parent = GUI_ID(0);
            
            BufferId active_buffer = view.buffer;
            
            i32 cutoff = -1;
            for (i32 i = 0; i < view.buffers.count; i++) {
                Buffer *b = get_buffer(view.buffers[i]);
                
                GuiId id = GUI_ID_INDEX(parent, i);
                
                TextQuadsAndBounds td = calc_text_quads_and_bounds(b->name, font);
                
                Vector2 size = td.bounds.size + Vector2{ 14.0f, 2.0f };
                if (gui_current_layout()->available_space.x < size.x) {
                    cutoff = i;
                    break;
                }
                
                Vector2 pos = gui_layout_widget(size);
                Vector2 text_offset = size*0.5f - Vector2{ td.bounds.size.x*0.5f, font->line_height*0.5f };
                
                if (gui_clicked(id)) active_buffer = b->id;
                gui_hot_rect(id, pos, size);
                
                if (i == current_tab) {
                    gui_draw_accent_button(id, pos, size);
                    gui_draw_text(td.glyphs, pos+text_offset, wnd->clip_rect, gui.style.fg, font);
                } else {
                    gui_draw_button(id, pos, size);
                    gui_draw_text(td.glyphs, pos+text_offset, wnd->clip_rect, gui.style.fg, font);
                }
                
                if (b->saved_at != b->history_index) {
                    gui_draw_rect(
                        pos + Vector2{ size.x-4-1, 1 }, 
                        { 4, 4 }, 
                        wnd->clip_rect, 
                        rgb_unpack(0xFF990000));
                }
            }
            
            if (cutoff >= 0) {
                // TODO(jesper): this is essentially a dropdown with an icon button as the trigger button
                // instead of a label button

                GuiId mid = GUI_ID(0);
                if (gui_pressed(mid)) {
                    if (gui.focused == mid) {
                        gui_clear_hot();
                        gui.focused = GUI_ID_INVALID;
                    } else {
                        gui.focused = mid;
                    }
                }
                gui_hot_rect(mid, mpos, msize);

                Vector3 mbg = gui.hot == mid ? gui.style.accent_bg_hot : gui.style.accent_bg;
                gui_draw_rect(mpos, msize, wnd->clip_rect, mbg);
                
                Vector2 icon_s{ 16, 16 };
                Vector2 icon_p = mpos + 0.5f*(msize - icon_s);
                gui_draw_rect(icon_p, icon_s, gui.icons.down);

                if (gui.focused == mid) {
                    i32 old_window = gui.current_window;
                    gui.current_window = GUI_OVERLAY;
                    defer { gui.current_window = old_window; };
                    
                    GuiWindow *overlay = &gui.windows[GUI_OVERLAY];
                    
                    Vector3 border_col = rgb_unpack(0xFFCCCCCC);
                    Vector3 bg = rgb_unpack(0xFF1d2021);
                    Vector3 bg_hot = rgb_unpack(0xFF3A3A3A);

                    i32 bg_index = overlay->command_buffer.commands.count;
                    
                    // TODO(jesper): figure out a reasonable way to make this layout anchored to the right and expand to the
                    // left with the subsequent labels. Right now I'm just hoping that the width is wide enough
                    Vector2 s{ 200.0f, 0 };
                    Vector2 p{ mpos.x-s.x+msize.x, mpos.y + msize.y };
                    Vector2 border{ 1, 1 };

                    gui_begin_layout({ .type = GUI_LAYOUT_ROW, .flags = GUI_LAYOUT_EXPAND_Y, .pos = p, .size = s, .margin = { 1, 1 } });
                    defer { gui_end_layout(); };

                    for (i32 i = cutoff; i < view.buffers.count; i++) {
                        Buffer *b = get_buffer(view.buffers[i]);

                        Vector2 rect_s{ 0, font->line_height };
                        Vector2 rect_p = gui_layout_widget(&rect_s);

                        GuiId lid = GUI_ID_INDEX(parent, i);
                        if (gui.hot == lid) gui_draw_rect(rect_p, rect_s, bg_hot);
                        gui_textbox(b->name, rect_p);

                        if ((gui.hot == lid && gui.pressed == mid && !gui.mouse.left_pressed) || 
                            gui_clicked(lid)) 
                        {
                            active_buffer = b->id;
                            gui.focused = GUI_ID_INVALID;
                            gui_clear_hot();
                        } else {
                            // TODO(jesper): due to changes in gui_hot to not allow a new hot widget
                            // if one is pressed, this doesn't behave the way I want for dropdowns where
                            // I can press the trigger button, drag to an item, then release to select it
                            gui_hot_rect(lid, rect_p, rect_s);
                        }
                    }

                    GuiLayout *cl = gui_current_layout();
                    gui_draw_rect(cl->pos, cl->size, overlay->clip_rect, border_col, &overlay->command_buffer, bg_index);
                    gui_draw_rect(cl->pos + border, cl->size - 2*border, overlay->clip_rect, bg, &overlay->command_buffer, bg_index+1);

                }
            }

            view_set_buffer(active_buffer);
        }

        {
            Rect r = gui_layout_widget_fill();
            gui_begin_layout( { .type = GUI_LAYOUT_COLUMN, .rect = r });
            defer { gui_end_layout(); };

            // NOTE(jesper): the scrollbar is 1 frame delayed if the reflowing results in a different number of lines,
            // which we are doing afterwards to allow the gui layout system trigger line reflowing
            // We could move it to after the reflow handling with some more layout plumbing, by letter caller deal with
            // the layouting and passing in absolute positions and sizes to the scrollbar
            gui_scrollbar(app.mono.line_height, &view.line_offset, view.lines.count, view.lines_visible, &view.voffset);
            Rect text_rect = gui_layout_widget_fill();

            if (view.lines_dirty || text_rect != view.rect) {
                view.rect = text_rect;

                view.lines.count = 0;
                recalculate_line_wrap(0, &view.lines, view.buffer);

                view.lines_dirty = false;
            }

            gui_hot_rect(view.gui_id, view.rect.pos, view.rect.size, -1);
        }
    }

    // TODO(jesper): this probably signifies the caret APIs aren't all ready yet and it might
    // make sense for this path to go away completely when they are, but I won't completely discount that
    // we might still need a path to re-calculate it from scratch from a byte offset. But probably we
    // need to do that immediately instead of deferring, which first relies on all the line wrapping
    // recalculation to be done immediately as well
    if (view.caret_dirty) {
        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
        view.caret_dirty = false;
    }
    view.mark = recalculate_caret(view.mark, view.buffer, view.lines);
    
    if (true) {
        Vector2 ib_s{ 0, 15.0f };
        gui_begin_layout({ .type = GUI_LAYOUT_COLUMN, .pos = { view.rect.pos.x, view.rect.pos.y+view.rect.size.y-ib_s.y }, .size = ib_s });
        defer { gui_end_layout(); };

        char buffer[256];
        gui_textbox(stringf(buffer, sizeof buffer, "line: %d, col: %lld", view.caret.line+1, view.caret.column+1));
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

        gui_draw_rect({ view.rect.pos.x, p0.y }, { view.rect.size.x, h }, app.line_bg, &gfx.frame_cmdbuf);

        gui_draw_rect(p0, { w, h }, app.caret_bg, &gfx.frame_cmdbuf);
        gui_draw_rect(p0, { 1.0f, h }, app.caret_fg, &gfx.frame_cmdbuf);
        gui_draw_rect(p0, { w, 1.0f }, app.caret_fg, &gfx.frame_cmdbuf);
        gui_draw_rect(p1, { w, 1.0f }, app.caret_fg, &gfx.frame_cmdbuf);
    }

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

        gui_draw_rect(p0, { 1.0f, h }, app.mark_fg, &gfx.frame_cmdbuf);
        gui_draw_rect(p0, { w, 1.0f }, app.mark_fg, &gfx.frame_cmdbuf);
        gui_draw_rect(p1, { w, 1.0f }, app.mark_fg, &gfx.frame_cmdbuf);
    }

    Buffer *buffer = get_buffer(view.buffer);
    if (buffer && true) {
        GfxCommand cmd = gui_command(GFX_COMMAND_GUI_TEXT);
        cmd.gui_text.font_atlas = app.mono.texture;
        cmd.gui_text.color = linear_from_sRGB(app.fg);

        f32 voffset = view.voffset;
        i32 line_offset = view.line_offset;

        for (i32 i = 0; i < MIN(view.lines.count - line_offset, view.lines_visible); i++) {
            i32 line_index = i + line_offset;

            Vector2 baseline{ view.rect.pos.x, view.rect.pos.y + i*app.mono.line_height + (f32)app.mono.baseline - voffset };

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
                    vcolumn++;
                    continue;
                }

                if (c == '\t') {
                    i32 w = app.tab_width - vcolumn % app.tab_width;
                    vcolumn += w;
                    continue;
                }
                
                Vector2 pen{ baseline.x + vcolumn*app.mono.space_width, baseline.y };
                GlyphRect g = get_glyph_rect(&app.mono, c, &pen);

                // NOTE(jesper): if this fires then we haven't done line reflowing correctly
                ASSERT(g.x1 < view.rect.pos.x + view.rect.size.x);

                // TODO(jesper): the way this text rendering works it'd probably be far cheaper for us to just 
                // overdraw a background outside the view rect
                if (apply_clip_rect(&g, view.rect)) {
                    f32 vertices[] = { 
                        g.x0, g.y0, g.s0, g.t0,
                        g.x0, g.y1, g.s0, g.t1,
                        g.x1, g.y1, g.s1, g.t1,

                        g.x1, g.y1, g.s1, g.t1,
                        g.x1, g.y0, g.s1, g.t0,
                        g.x0, g.y0, g.s0, g.t0,
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