#include "platform.h"
#include "core.h"
#include "maths.h"
#include "allocator.h"
#include "window.h"
#include "process.h"

#include "maths.cpp"
#include "gui.cpp"
#include "string.cpp"
#include "allocator.cpp"
#include "core.cpp"
#include "window.cpp"
#include "gfx_opengl.cpp"
#include "assets.cpp"
#include "font.cpp"
#include "fzy.cpp"

#include "tree_sitter/api.h"
extern "C" const TSLanguage* tree_sitter_cpp();
extern "C" const TSLanguage* tree_sitter_rust();

#define DEBUG_LINE_WRAP_RECALC 0
#define DEBUG_TREE_SITTER_QUERY 0

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

enum Language {
    LANGUAGE_NONE,
    LANGUAGE_CPP,
    LANGUAGE_RUST,
    LANGUAGE_COUNT,
};

enum ViewFlags : u32 {
    VIEW_SET_MARK = 1 << 0,
};

struct Range_i64 {
    i64 start, end;
};

struct Range_i32 {
    i32 start, end;
};


struct BufferLine {
    i64 offset : 63;
    i64 wrapped : 1;
};

struct ProcessCommand {
    String exe;
    String args;

    Process *process;
};

struct Application {
    FontAtlas mono;
    bool animating = true;

    struct {
        bool active;
        DynamicArray<String> values;
        DynamicArray<String> filtered;

        i32 selected_item;
    } lister;
    
    struct {
        String str;
        i64 start_caret, start_mark;
        bool active;
        bool set_mark;
    } incremental_search;
    
    const TSLanguage *languages[LANGUAGE_COUNT];
    TSQuery *queries[LANGUAGE_COUNT];
    EditMode mode, next_mode;

    DynamicArray<String> process_command_names;
    DynamicArray<ProcessCommand> process_commands;
    i32 selected_process;

    Vector3 bg = bgr_unpack(0x00142825);
    Vector3 fg = bgr_unpack(0x00D5C4A1);
    Vector3 mark_fg = bgr_unpack(0xFFEF5F0A);
    Vector3 caret_fg = bgr_unpack(0xFFEF5F0A);
    Vector3 caret_bg = bgr_unpack(0xFF8a523f);
    Vector3 line_bg = bgr_unpack(0xFF264041);
    
    HashTable<String, u32> syntax_colors;

    struct {
        GLuint build;
    } icons;
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
    
    Language language;
    TSTree *syntax_tree;

    DynamicArray<BufferHistory> history;
    i32 history_index;
    i32 saved_at;

    NewlineMode newline_mode;
    i32 tab_width = 4;
    bool indent_with_tabs;

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

    GLuint glyph_data_ssbo;

    struct {
        u64 lines_dirty : 1;
        u64 caret_dirty : 1;
        u64 defer_move_view_to_caret : 1;
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
    i32 lines_visible = (i32)((gfx.resolution.y / (f32)app.mono.line_height) + 1.999f);
    view.lines_dirty = view.lines_dirty || lines_visible != view.lines_visible;
    view.lines_visible = lines_visible;
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

String buffer_newline_str(Buffer *buffer)
{
    switch (buffer->newline_mode) {
    case NEWLINE_LF: return "\n";
    case NEWLINE_CR: return "\r";
    case NEWLINE_CRLF: return "\r\n";
    case NEWLINE_LFCR: return "\n\r";
    }
}

String buffer_newline_str(BufferId buffer_id)
{
    Buffer *buffer = get_buffer(buffer_id);
    return buffer_newline_str(buffer);
}

BufferId create_buffer(String file)
{
    // TODO(jesper): do something to try and figure out/guess file type,
    // in particular try to detect whether the file is a binary so that we can
    // write a binary blob visualiser, or at least something that won't choke or
    // crash trying to render invalid text
    // TODO(jesper): tab/spaces indent mode depending on file type and project settings

    Buffer b{ .type = BUFFER_FLAT };
    b.id = { .index = buffers.count };
    b.file_path = absolute_path(file, mem_dynamic);
    b.name = filename_of(b.file_path);
    b.newline_mode = NEWLINE_LF;

    FileInfo f = read_file(file, mem_dynamic);
    b.flat.data = (char*)f.data;
    b.flat.size = f.size;
    b.flat.capacity = f.size;
    
    String ext = extension_of(b.file_path);
    if (ext == ".cpp" || ext == ".c" ||
        ext == ".h" || ext == ".hpp" ||
        ext == ".cxx" || ext == ".cc")
    {
        b.language = LANGUAGE_CPP;
    } else if (ext == ".rs") {
        b.language = LANGUAGE_RUST;
    }
    
    if (f.data) {
        // TODO(jesper): guess tabs/spaces based on file content?
        
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

        if (auto lang = app.languages[b.language]; lang) {
            TSParser *parser = ts_parser_new();
            ts_parser_set_language(parser, lang);
            b.syntax_tree = ts_parser_parse_string(parser, b.syntax_tree, b.flat.data, b.flat.size);
            ts_parser_delete(parser);
#if 0
            TSNode root = ts_tree_root_node(b.syntax_tree);
            char *sexpr = ts_node_string(root);
            
            String n = slice(b.name, 0, b.name.length-extension_of(b.name).length);
            String scm_path = stringf(mem_tmp, "D:/projects/mimir/build/%.*s.scm", STRFMT(n));
            write_file(scm_path, sexpr, strlen(sexpr));
            
            String gv_path = stringf(mem_tmp, "D:/projects/mimir/build/%.*s.dot", STRFMT(n));
            FILE *f = fopen(sz_string(gv_path), "wb");
            ts_tree_print_dot_graph(b.syntax_tree, f);
            fclose(f);
#endif
        }
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
    if (idx >= 0) array_remove(&view.buffers, idx);
    array_insert(&view.buffers, 0, view.buffer);

    // NOTE(jesper): not storing line wrap data (for now). Computing it is super quick, and storing
    // it will add up pretty quick. Especially if we start serialising the views and their state
    // to disk
    view.lines_dirty = true;
}

Allocator ts_custom_alloc;

struct ts_alloc_header {
    size_t size;
    size_t pad;
};

void* ts_custom_malloc(size_t size)
{
    if (size == 0) return nullptr;
    
    void *ptr = ALLOC(ts_custom_alloc, size+sizeof(ts_alloc_header));
    auto header = (ts_alloc_header*)ptr;
    header->size = size;
    return (u8*)header + sizeof *header;
}

void* ts_custom_calloc(size_t count, size_t size)
{
    if (size == 0) return nullptr;
    
    void *ptr = ts_custom_malloc(count*size);
    memset(ptr, 0, count*size);
    return ptr;
}

void* ts_custom_realloc(void *ptr, size_t size)
{
    if (size == 0) return nullptr;
    if (ptr == nullptr) return ts_custom_malloc(size);
    
    auto header = (ts_alloc_header*)((u8*)ptr - sizeof(ts_alloc_header));
    void *nptr = REALLOC(ts_custom_alloc, header, header->size+sizeof(ts_alloc_header), size+sizeof(ts_alloc_header));
    
    header = (ts_alloc_header*)nptr;
    header->size = size;
    return (u8*)header + sizeof *header;

}

void ts_custom_free(void *ptr)
{
    if (!ptr) return;
    
    auto header = (ts_alloc_header*)((u8*)ptr - sizeof(ts_alloc_header));
    FREE(ts_custom_alloc, header);
}

TSQuery* ts_create_query(const TSLanguage *lang, String highlights)
{
    if (auto a = find_asset(highlights); a) {
        u32 error_loc;
        TSQueryError error;
        TSQuery *query = ts_query_new(lang, (char*)a->data, a->size, &error_loc, &error);

        if (error != TSQueryErrorNone) {
            String s = "";
            switch (error) {
            case TSQueryErrorNone: s = "none"; break;
            case TSQueryErrorSyntax: s = "syntax"; break;
            case TSQueryErrorNodeType: s = "node_type"; break;
            case TSQueryErrorField: s = "field"; break;
            case TSQueryErrorCapture: s = "capture"; break;
            case TSQueryErrorStructure: s = "structure"; break;
            case TSQueryErrorLanguage: s = "language"; break;
            }

            LOG_ERROR("tree-sitter query creation error: '%.*s' in %.*s:%d", STRFMT(s), STRFMT(highlights), error_loc);
            return nullptr;
        }
        
        return query;
    }
    
    return nullptr;
}

void init_app(Array<String> args)
{
    fzy_init_table();

    String exe_folder = get_exe_folder();

    String asset_folders[] = {
        exe_folder,
        join_path(exe_folder, "/assets"),
        join_path(exe_folder, "../assets"),
    };

    init_assets({ asset_folders, ARRAY_COUNT(asset_folders) });

    init_gui();
    
    app.languages[LANGUAGE_CPP] = tree_sitter_cpp();
    app.languages[LANGUAGE_RUST] = tree_sitter_rust();
    ts_custom_alloc = vm_freelist_allocator(5*1024*1024*1024ull);
    ts_set_allocator(ts_custom_malloc, ts_custom_calloc, ts_custom_realloc, ts_custom_free);
    
    app.queries[LANGUAGE_CPP] = ts_create_query(app.languages[LANGUAGE_CPP], "queries/cpp/highlights.scm");
    app.queries[LANGUAGE_RUST] = ts_create_query(app.languages[LANGUAGE_RUST], "queries/rust/highlights.scm");

    u32 fg = bgr_pack(app.fg);
    set(&app.syntax_colors, "unused", fg);
    set(&app.syntax_colors, "_parent", fg);
    set(&app.syntax_colors, "label", fg);
    set(&app.syntax_colors, "parameter", fg);
    set(&app.syntax_colors, "namespace", fg);
    set(&app.syntax_colors, "variable", fg);

    set(&app.syntax_colors, "preproc", 0xFE8019u);
    set(&app.syntax_colors, "include", 0xFE8019u);
    set(&app.syntax_colors, "string", 0x689D6Au);
    set(&app.syntax_colors, "comment", 0x8EC07Cu);
    set(&app.syntax_colors, "function", 0xccb486u);
    set(&app.syntax_colors, "operator", 0xfcedcfu);
    set(&app.syntax_colors, "punctuation", 0xfcedcfu);
    set(&app.syntax_colors, "type", 0xbcbf91u);
    set(&app.syntax_colors, "constant", 0xe9e4c6u);
    //set(&app.syntax_colors, "constant.identifier", 0xff0000u);//0xe9e4c6u);
    set(&app.syntax_colors, "keyword", 0xd36e2au);
    set(&app.syntax_colors, "namespace", 0xd36e2au);
    set(&app.syntax_colors, "preproc.identifier", 0x84a89au);
    set(&app.syntax_colors, "attribute", 0x84a89au);
    
    // TODO(jesper): this is kinda neat but I think I might want this as some kind of underline or background
    // style instead of foreground colour?
    //set(&app.syntax_colors, "error", 0xFFFF0000); 

    // TODO(jesper): I definitely need to figure this shit out
    for (i32 i = 0; i < app.syntax_colors.capacity; i++) {
        if (!app.syntax_colors.slots[i].occupied) continue;
        Vector3 c = bgr_unpack(app.syntax_colors.slots[i].value);
        app.syntax_colors.slots[i].value = bgr_pack(linear_from_sRGB(c));
    }

    if (auto a = find_asset("textures/build_16x16.png"); a) app.icons.build = gfx_load_texture(a->data, a->size);

    app.mono = create_font("fonts/UbuntuMono/UbuntuMono-Regular.ttf", 18, true);

    calculate_num_visible_lines();
    glGenBuffers(1, &view.glyph_data_ssbo);
    
    if (true) {
        array_add(&app.process_command_names, { "build.bat" });
        array_add(&app.process_commands, { .exe = "D:\\projects\\plumber\\build.bat" });
    }

    if (args.count > 0) {
        String dir = directory_of(args[0]);
        if (dir.length > 0) set_working_dir(dir);
        view_set_buffer(create_buffer(args[0]));
    }
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

i64 line_start_offset(i32 line, Array<BufferLine> lines)
{
    return line < lines.count ? lines[line].offset : 0;
}

i64 line_end_offset(i32 line, Array<BufferLine> lines, Buffer *buffer)
{
    switch (buffer->type) {
    case BUFFER_FLAT: return (line+1) < lines.count ? lines[line+1].offset : buffer->flat.size;
    }
}

i64 line_end_offset(i32 wrapped_line, Array<BufferLine> lines, BufferId buffer_id)
{
    return line_end_offset(wrapped_line, lines, &buffers[buffer_id.index]);
}

i64 buffer_prev_offset(Buffer *buffer, i64 byte_offset)
{
    ASSERT(buffer);
    i64 prev_offset = byte_offset;

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

Range_i32 range_i32(i32 a, i32 b)
{
    return { .start = MIN(a, b), .end = MAX(a, b) };
}

Range_i64 caret_range(Caret c0, Caret c1, Array<BufferLine> lines, BufferId buffer, bool lines_block)
{
    Range_i64 r;

    if (lines_block) {
        i32 start_line = MIN(c0.wrapped_line, c1.wrapped_line);
        i32 end_line = MAX(c0.wrapped_line, c1.wrapped_line);

        r.start = line_start_offset(start_line, lines);
        r.end = line_end_offset(end_line, lines, buffer);
        
        if (end_line < lines.count-1 && lines[end_line].wrapped && !lines[end_line+1].wrapped) {
            r.end = buffer_prev_offset(buffer, r.end);
        }
    } else {
        r.start = MIN(c0.byte_offset, c1.byte_offset);
        r.end = MAX(c0.byte_offset, c1.byte_offset);
    }

    if (r.start == r.end) r.end = buffer_next_offset(buffer, r.end);
    if (r.start == r.end) r.start = buffer_prev_offset(buffer, r.start);
    return r;
}

i64 caret_nth_offset(BufferId buffer_id, i64 current, i32 n)
{
    i64 offset = current;
    while (n--) offset = buffer_next_offset(buffer_id, offset);
    return offset;
}

i32 calc_unwrapped_line(i32 wrapped_line, Array<BufferLine> lines)
{
    i32 line = CLAMP(wrapped_line, 0, lines.count-1);
    while (line > 0 && lines[line].wrapped) line--;
    return line;
}

i32 wrapped_line_from_offset(i64 offset, Array<BufferLine> lines, i32 guessed_line = 0)
{
    i32 line = CLAMP(guessed_line, 0, lines.count-1);
    while (line > 0 && offset < lines[line].offset) line--;
    while (line < (lines.count-2) && offset > lines[line+1].offset) line++;
    return line;
}

i32 unwrapped_line_from_offset(i64 offset, Array<BufferLine> lines, u32 guessed_line = 0)
{
    i32 line = wrapped_line_from_offset(offset, lines, guessed_line);
    return calc_unwrapped_line(line, lines);
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

        if (c == '\t') count += buffer->tab_width - count % buffer->tab_width;
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

        if (c == '\t') count += buffer->tab_width - count % buffer->tab_width;
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

            if (c == '\t') count += buffer->tab_width - count % buffer->tab_width;
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

        if (c == '\t') column += buffer->tab_width - column % buffer->tab_width;
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

        if (c == '\t') column += buffer->tab_width - column % buffer->tab_width;
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

i64 unwrapped_column_from_offset(i64 offset, Array<BufferLine> lines, i32 wrapped_line)
{
    i32 line = calc_unwrapped_line(wrapped_line, lines);
    i64 column = 0;
    for (i64 i = lines[line].offset; i < offset; i++, column++);
    return column;
}
       


i64 calc_byte_offset(i64 wrapped_column, i32 wrapped_line, Array<BufferLine> lines, Buffer *buffer)
{
    if (wrapped_line >= lines.count) return 0;

    i64 offset = lines[wrapped_line].offset;

    for (i64 c = 0; c < wrapped_column; c++) {
        if (char_at(buffer, offset) == '\t') c += buffer->tab_width - c % buffer->tab_width - 1;
        offset = utf8_incr(buffer, offset);
    }

    return offset;
}


// NOTE(jesper): returns >= lines.count if line >= lines.count-1
i32 next_unwrapped_line(i32 line, Array<BufferLine> lines)
{
    i32 l = line;
    while (l < lines.count-1 && lines[l+1].wrapped) l++;
    return l+1;
}

// NOTE(jesper): returns -1 if line == 0
i32 prev_unwrapped_line(i32 line, Array<BufferLine> lines)
{
    i32 l = line-1;
    while (l > 0 && lines[l].wrapped) l--;
    return l;
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

void recalc_line_wrap(DynamicArray<BufferLine> *lines, i32 start_line, i32 end_line, BufferId buffer_id)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return;
    
    Rect r = view.rect;
    f32 base_x = r.pos.x;
    
    start_line = MAX(start_line, 0);
    end_line = MIN(end_line, lines->count);
    
    i64 start = line_start_offset(start_line, *lines);
    i64 end = line_end_offset(end_line, *lines, buffer);
    
    i32 prev_c = ' ';
    i64 vcolumn = 0;

    i64 p = start;
    i64 word_start = p;
    i64 line_start = p;
    
    DynamicArray<BufferLine> tmp_lines{ .alloc = mem_tmp };
    DynamicArray<BufferLine> *new_lines = &tmp_lines;
    
    if (lines->count == 0 || (start_line == 0 && end_line == lines->count)) {
        new_lines = lines;
        lines->count = start_line = end_line = 0;
        array_add(new_lines, BufferLine{ start, 0 });
    } else {
        end_line = MIN(lines->count, end_line+1);
        start_line = MIN(lines->count, start_line+1);
        
        if (start_line == lines->count) new_lines = lines;
    }
    
    while (p < end) {
        i64 pn = p;
        i32 c = utf32_it_next(buffer, &p);
        
        if ((is_word_boundary(prev_c) && !is_word_boundary(c)) ||
            (is_word_boundary(c) && !is_word_boundary(prev_c)))
        {
            word_start = pn;
        }
        prev_c = c;

        if (c == '\n' || c == '\r') {
            if (p < end && 
                ((c == '\n' && char_at(buffer, p) == '\r') || 
                 (c == '\r' && char_at(buffer, p) == '\n')))
            {
                p = next_byte(buffer, p);
            }
            
            line_start = word_start = p;
            array_add(new_lines, { line_start, 0 });

            vcolumn = 0;
            continue;
        }

        if (c == ' ') {
            vcolumn++;
            continue;
        }

        if (c == '\t') {
            i32 w = buffer->tab_width - vcolumn % buffer->tab_width;
            vcolumn += w;
            continue;
        }

        f32 x1 = base_x + (vcolumn+1) * app.mono.space_width;
        if (x1 >= r.pos.x + r.size.x) {
            if (!is_word_boundary(c) && line_start != word_start) {
                p = line_start = word_start;
            } else {
                p = line_start = word_start = pn;
            }
            vcolumn = 0;

            array_add(new_lines, { (i64)line_start, 1 });
        }

        vcolumn++;
    }
    
    if (new_lines != lines) {
        if (end != buffer_end(buffer) && new_lines->at(new_lines->count-1).offset == end) new_lines->count--;
        
#if DEBUG_LINE_WRAP_RECALC
        LOG_INFO("start line: %d, end line: %d, start offset: %lld, end offset: %lld", start_line, end_line, start, end);
        for (i32 i = MAX(0, start_line-5); i < MIN(end_line+5, lines->count); i++) {
            LOG_RAW("\t existing line[%d]: %lld, wrapped: %d\n", i, lines->at(i).offset, lines->at(i).wrapped);
        }

        LOG_INFO("replacing lines [%d, %d[ with %d lines:", start_line, end_line, new_lines->count);
        for (i32 i = 0; i < new_lines->count; i++) {
            LOG_RAW("\t new line[%d]: %lld, wrapped: %d\n", i, new_lines->at(i).offset, new_lines->at(i).wrapped);
        }
#endif

        array_replace(lines, start_line, end_line, *new_lines);

#if DEBUG_LINE_WRAP_RECALC
        for (i32 i = MAX(0, start_line-5); i < MIN(end_line+5, lines->count); i++) {
                LOG_RAW("resulting line[%d]: %lld, wrapped: %d\n", i, lines->at(i).offset, lines->at(i).wrapped);
        }
#endif
    }
}

bool buffer_remove(BufferId buffer_id, i64 byte_start, i64 byte_end, bool record_history = true)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return false;
    
    i32 start_line, old_end_line;
    i64 start_column, old_end_column;
    if (view.buffer == buffer_id) {
        start_line = unwrapped_line_from_offset(byte_start, view.lines, view.caret.line);
        start_column = unwrapped_column_from_offset(byte_start, view.lines, start_line);
        
        old_end_line = unwrapped_line_from_offset(byte_end, view.lines, view.caret.line);
        old_end_column = unwrapped_column_from_offset(byte_end, view.lines, old_end_line);
    }

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

        if (view.buffer == buffer_id) {
            i32 line = wrapped_line_from_offset(byte_start, view.lines, view.caret.wrapped_line);
            i64 start_offset = view.lines[line].offset;
            for (i32 i = line+1; i < view.lines.count; i++) {
                view.lines[i].offset -= num_bytes;

                if (view.lines[i].offset <= start_offset) array_remove(&view.lines, i--);
            }
            
            recalc_line_wrap(
                &view.lines,
                prev_unwrapped_line(line, view.lines),
                next_unwrapped_line(line, view.lines),
                view.buffer);
            
            
            if (auto lang = app.languages[buffer->language]; lang) {
                if (buffer->syntax_tree) {
                    i32 new_end_line = unwrapped_line_from_offset(byte_start, view.lines, start_line);
                    i64 new_end_column = unwrapped_column_from_offset(byte_start, view.lines, new_end_line);
                    
                    TSInputEdit edit{
                        .start_byte = (u32)byte_start, 
                        .old_end_byte = (u32)byte_end,
                        .new_end_byte = (u32)byte_start,
                        .start_point = { .row = (u32)start_line, .column = (u32)start_column},
                        .old_end_point = { .row = (u32)old_end_line, .column = (u32)old_end_column},
                        .new_end_point = { .row = (u32)new_end_line, .column = (u32)new_end_column },
                    };
                    
                    ts_tree_edit(buffer->syntax_tree, &edit);
                }
                
                TSParser *parser = ts_parser_new();
                defer { ts_parser_delete(parser); };
                
                ts_parser_set_language(parser, lang);
                buffer->syntax_tree = ts_parser_parse_string(parser, buffer->syntax_tree, buffer->flat.data, buffer->flat.size);
            }
        } else {
            if (buffer->syntax_tree) ts_tree_delete(buffer->syntax_tree);
            buffer->syntax_tree = nullptr;
        }
        
        return true;
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

i64 buffer_seek_forward(BufferId buffer_id, String needle, i64 start)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return start;
    
    switch (buffer->type) {
    case BUFFER_FLAT:
        for (i64 offset = start+1; 
             offset < buffer->flat.size && offset + needle.length <= buffer->flat.size; 
             offset++) 
        {
            char *p = buffer->flat.data+offset;
            for (i32 i = 0; i < needle.length; i++) {
                if (to_lower(*(p+i)) != needle[i]) goto next0;
            }

            return offset;
next0:;
        }

        for (i64 offset = 0;
             offset < start && offset + needle.length <= start && offset + needle.length <= buffer->flat.size;
             offset++) 
        {
            char *p = buffer->flat.data+offset;
            for (i32 i = 0; i < needle.length; i++) {
                if (to_lower(*(p+i)) != needle[i]) goto next1;
            }

            return offset;
next1:;
        }
        break;
    }


    return start;
}

i64 buffer_seek_first_forward(BufferId buffer_id, Array<char> chars, i64 start)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return start;

    switch (buffer->type) {
    case BUFFER_FLAT:
        for (i64 offset = start+1; offset < buffer->flat.size; offset++)  {
            char c = *(buffer->flat.data+offset);
            for (i32 i = 0; i < chars.count; i++) {
                if (c == chars[i]) return offset;
            }
        }

        for (i64 offset = 0; offset < start; offset++) {
            char c = *(buffer->flat.data+offset);
            for (i32 i = 0; i < chars.count; i++) {
                if (c == chars[i]) return offset;
            }
        }
        break;
    }


    return -1;
}

i64 buffer_seek_back(BufferId buffer_id, String needle, i64 start)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return start;

    switch (buffer->type) {
    case BUFFER_FLAT:
        for (i64 offset = start-1; offset >= 0; offset--) {
            char *p = buffer->flat.data+offset;
            for (i32 i = 0; i < needle.length; i++) {
                if (to_lower(*(p+i)) != needle[i]) goto next0;
            }

            return offset;
next0:;
        }
        
        for (i64 offset = buffer->flat.size-1; offset > start; offset--) {
            char *p = buffer->flat.data+offset;
            for (i32 i = 0; i < needle.length; i++) {
                if (to_lower(*(p+i)) != needle[i]) goto next1;
            }

            return offset;
next1:;
        }

        break;
    }


    return start;
}

i64 buffer_seek_first_back(BufferId buffer_id, Array<char> chars, i64 start)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return start;

    switch (buffer->type) {
    case BUFFER_FLAT:
        for (i64 offset = start-1; offset >= 0; offset--) {
            char c = *(buffer->flat.data+offset);
            for (i32 i = 0; i < chars.count; i++) {
                if (c == chars[i]) return offset;
            }
        }

        for (i64 offset = buffer->flat.size-1; offset > start; offset--) {
            char c = *(buffer->flat.data+offset);
            for (i32 i = 0; i < chars.count; i++) {
                if (c == chars[i]) return offset;
            }
        }
        break;
    }


    return -1;
}

i64 buffer_seek_first_forward(BufferId buffer_id, std::initializer_list<char> chars, i64 start)
{
    return buffer_seek_first_forward(buffer_id, Array<char>{ .data = (char*)chars.begin(), .count = (i32)chars.size() }, start);
}

i64 buffer_seek_first_back(BufferId buffer_id, std::initializer_list<char> chars, i64 start)
{
    return buffer_seek_first_back(buffer_id, Array<char>{ .data = (char*)chars.begin(), .count = (i32)chars.size() }, start);
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
    // Alternatively, just hash the contents and compare to hash of version on disk. Any
    // file I'm realistically going to open with this will take much less than a second to
    // calculate the hashes
    // When/if I make this robust for very large files, I can add alternative paths
    // specifically for that which do simpler and less computationally expensive things
    Buffer *buffer = get_buffer(buffer_id);
    return buffer && buffer->saved_at != buffer->history_index;
}

i64 buffer_insert(BufferId buffer_id, i64 offset, String in_text, bool record_history = true)
{
    if (in_text.length <= 0) return offset;

    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return offset;

    i32 at_line;
    i64 at_column;
    if (view.buffer == buffer_id) {
        at_line = unwrapped_line_from_offset(offset, view.lines, view.caret.line);
        at_column = unwrapped_column_from_offset(offset, view.lines, at_line);
    }

    i64 end_offset = offset;

    String nl = buffer_newline_str(buffer);

    i32 required_extra_space = 0;
    for (i32 i = 0; i < in_text.length; i++) {
        if (in_text[i] == '\r' || in_text[i] == '\n') {
            if (i < in_text.length-1 && in_text[i] == '\r' && in_text[i+1] == '\n') i++;
            if (i < in_text.length-1 && in_text[i] == '\n' && in_text[i+1] == '\r') i++;
            required_extra_space += nl.length;
        } else {
            required_extra_space++;
        }
    }

    String text = in_text;
    if (required_extra_space != in_text.length) {
        text.data = ALLOC_ARR(mem_tmp, char, required_extra_space);
        text.length = required_extra_space;

        i32 s = 0;
        for (i32 i = 0; i < in_text.length; i++) {
            if (in_text[i] == '\r' || in_text[i] == '\n') {
                if (i < in_text.length-1 && in_text[i] == '\r' && in_text[i+1] == '\n') i++;
                if (i < in_text.length-1 && in_text[i] == '\n' && in_text[i+1] == '\r') i++;

                memcpy(&text[s], nl.data, nl.length);
                s += nl.length;
            } else {
                text[s++] = in_text[i];
            }
        }
    }

    switch (buffer->type) {
    case BUFFER_FLAT: {
            if (buffer->flat.size + text.length > buffer->flat.capacity) {
                i64 new_capacity = buffer->flat.size + text.length;
                buffer->flat.data = (char*)REALLOC(mem_dynamic, buffer->flat.data, buffer->flat.capacity, new_capacity);
                buffer->flat.capacity = new_capacity;
            }
            
            memmove(buffer->flat.data+offset+text.length, buffer->flat.data+offset, buffer->flat.size-offset);
            memcpy(buffer->flat.data+offset, text.data, text.length);
            buffer->flat.size += required_extra_space;
            end_offset += required_extra_space;
        } break;
    }

    // TODO(jesper): multi-view support
    if (view.buffer == buffer_id) {
        i32 line = wrapped_line_from_offset(offset, view.lines, view.caret.wrapped_line);
        for (i32 i = line+1; i < view.lines.count; i++) {
            view.lines[i].offset += required_extra_space;
        }
        
        recalc_line_wrap(
            &view.lines, 
            calc_unwrapped_line(line, view.lines), 
            next_unwrapped_line(line, view.lines), 
            view.buffer);
        
        i32 new_end_line = unwrapped_line_from_offset(offset+required_extra_space, view.lines, view.caret.line);
        i64 new_end_column = unwrapped_column_from_offset(offset+required_extra_space, view.lines, new_end_line);

        if (auto lang = app.languages[buffer->language]; lang) {
            if (buffer->syntax_tree) {
                TSInputEdit edit{
                    .start_byte = (u32)offset, 
                    .old_end_byte = (u32)offset,
                    .new_end_byte = (u32)(offset+required_extra_space),
                    .start_point = { .row = (u32)at_line, .column = (u32)at_column },
                    .old_end_point = { .row = (u32)at_line, .column = (u32)at_column },
                    .new_end_point = { .row = (u32)new_end_line, .column = (u32)new_end_column },
                };
                
                ts_tree_edit(buffer->syntax_tree, &edit);
            }
            
            TSParser *parser = ts_parser_new();
            defer { ts_parser_delete(parser); };
            ts_parser_set_language(parser, lang);
            buffer->syntax_tree = ts_parser_parse_string(parser, buffer->syntax_tree, buffer->flat.data, buffer->flat.size);
        }
    } else {
        if (buffer->syntax_tree) ts_tree_delete(buffer->syntax_tree);
        buffer->syntax_tree = nullptr;
    }

    if (record_history) {
        buffer_history(buffer_id, { .type = BUFFER_INSERT, .offset = offset, .text = text });
    }
    
    return end_offset;
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
    if (start == end) return start;

    // NOTE(jesper): in general, return the byte offset to the start of the textual content,
    // skipping past whitespace. If the line only contains whitespace, return the absolute
    // start.
    i64 offset = start;
    while (offset < end) {
        if (!is_whitespace(char_at(buffer, offset))) return offset;
        offset = next_byte(buffer, offset);
    }

    return start;
}

i64 buffer_seek_end_of_line(BufferId buffer_id, Array<BufferLine> lines, i32 line)
{
    if (line >= lines.count) return 0;

    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return 0;

    i64 start = lines[line].offset;
    i64 end = line_end_offset(line, lines, buffer);
    if (start == end) return end;
    
    i64 endt = buffer_prev_offset(buffer, end);
    
    char c = char_at(buffer, endt);
    if (c == '\n' || c == '\r') return endt;
    return end;
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

bool app_needs_render()
{
    return app.next_mode != app.mode || app.animating || view.lines_dirty || view.caret_dirty;
}


Caret recalculate_caret(Caret caret, BufferId buffer_id, Array<BufferLine> lines)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return caret;

    if (caret.byte_offset == 0) caret.wrapped_line = caret.line = 0;
    else if (caret.byte_offset >= buffer_end(buffer_id)) {
        caret.byte_offset = buffer_end(buffer_id);
        recalc_caret_line(lines.count-1, &caret.wrapped_line, &caret.line, lines);
    } else {
        caret.wrapped_line = MIN(caret.wrapped_line, lines.count-1);
        caret.wrapped_line = MAX(0, caret.wrapped_line);

        while (caret.wrapped_line > 0 &&
               caret.byte_offset < lines[caret.wrapped_line].offset)
        {
            if (!lines[caret.wrapped_line--].wrapped) caret.line--;
        }

        while (caret.wrapped_line < lines.count-1 &&
               caret.byte_offset >= line_end_offset(caret.wrapped_line, lines, buffer))
        {
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
    view.defer_move_view_to_caret = 1;
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
    ASSERT(!view.lines_dirty);
    Buffer *buffer = get_buffer(view.buffer);
    
    wrapped_line = CLAMP(wrapped_line, 0, view.lines.count-1);
    if (buffer && wrapped_line != view.caret.wrapped_line) {
        i32 l = CLAMP(view.caret.wrapped_line, 0, view.lines.count-1);
        while (l > 0 && l > wrapped_line) {
            if (!view.lines[l--].wrapped) view.caret.line--;
        }

        while (l < view.lines.count-1 && l < wrapped_line) {
            if (!view.lines[++l].wrapped) view.caret.line++;
        }

        view.caret.wrapped_line = l;
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

        ASSERT(!view.lines_dirty);
        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
        view.mark = recalculate_caret(view.mark, view.buffer, view.lines);

        move_view_to_caret();

        app.animating = true;
    }
}

void write_string(BufferId buffer_id, String str, u32 flags = 0)
{
    if (!buffer_valid(buffer_id)) return;
    if (flags & VIEW_SET_MARK) view.mark = view.caret;

    BufferHistoryScope h(buffer_id);
    view.caret.byte_offset = buffer_insert(buffer_id, view.caret.byte_offset, str);
    ASSERT(!view.lines_dirty);
    view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
    view.mark = recalculate_caret(view.mark, view.buffer, view.lines);

    move_view_to_caret();

    app.animating = true;
}

void set_caret_xy(i64 x, i64 y)
{
    Buffer *buffer = get_buffer(view.buffer);
    if (!buffer) return;

    ASSERT(!view.lines_dirty);
    i64 wrapped_line = view.line_offset + (y - view.rect.pos.y + view.voffset) / app.mono.line_height;
    wrapped_line = CLAMP(wrapped_line, 0, view.lines.count-1);

    while (view.caret.wrapped_line > wrapped_line) {
        if (!view.lines[view.caret.wrapped_line].wrapped) view.caret.line--;
        view.caret.wrapped_line--;
    }

    while (view.caret.wrapped_line < wrapped_line) {
        if (!view.lines[view.caret.wrapped_line].wrapped) view.caret.line++;
        view.caret.wrapped_line++;
    }

    i64 wrapped_column = (x - view.rect.pos.x) / app.mono.space_width;
    view.caret.wrapped_column = calc_wrapped_column(view.caret.wrapped_line, wrapped_column, view.lines, buffer);
    view.caret.preferred_column = view.caret.wrapped_column;
    view.caret.column = calc_unwrapped_column(view.caret.wrapped_line, view.caret.wrapped_column, view.lines, buffer);
    view.caret.byte_offset = calc_byte_offset(view.caret.wrapped_column, view.caret.wrapped_line, view.lines, buffer);
}

void exec_process_command()
{
    if (app.process_commands.count > 0) {
        ProcessCommand *cmd = &app.process_commands[app.selected_process];
        if (!cmd->process || get_exit_code(cmd->process)) {
            release_process(cmd->process);
            cmd->process = create_process(cmd->exe, cmd->args, [](String s) { LOG_INFO("%.*s", STRFMT(s)); });
        }
    }
}

void app_event(WindowEvent event)
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
    case WE_QUIT:
        exit(0);
        break;
    case WE_TEXT:
        if (app.mode == MODE_INSERT) {
            write_string(view.buffer, String{ (char*)&event.text.c[0], event.text.length });
            app.animating = true;
        }
        break;
    case WE_KEY_PRESS:
        if (gui.focused != GUI_ID_INVALID) break;
        app.animating = true;
        if (app.mode == MODE_EDIT) {
            switch (event.key.keycode) {
            case KC_SLASH:
                app.incremental_search.start_caret = view.caret.byte_offset;
                app.incremental_search.start_mark = view.mark.byte_offset;
                app.incremental_search.active = true;
                app.incremental_search.set_mark = event.key.modifiers != MF_SHIFT;
                break;
            case KC_GRAVE: 
                // TODO(jesper): make a scope matching query for when tree sitter languages are available. This is the
                // textual fall-back version which doesn't handle comments or strings
                if (auto buffer = get_buffer(view.buffer); buffer) {
                    i64 f = buffer_seek_first_forward(view.buffer, {'{','}', '[',']', '(',')'}, view.caret.byte_offset-1);
                    i64 b = buffer_seek_first_back(view.buffer, {'{','}', '[',']', '(',')'}, view.caret.byte_offset+1);
                    
                    if (f != -1 && f != view.caret.byte_offset && 
                        (b < line_start_offset(view.caret.wrapped_line, view.lines) || 
                         b > line_end_offset(view.caret.wrapped_line, view.lines, view.buffer)))
                    {
                        view.caret.byte_offset = f;
                        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                        if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                        break;
                    }
                    
                    i64 offset = b == -1 || f-view.caret.byte_offset < view.caret.byte_offset-b ? f : b;
                    if (offset == -1) break;
                    
                    char l = char_at(buffer, offset);
                    bool forward = false;
                    char r;
                    switch (l) {
                    case '{': r = '}'; forward = true; break;
                    case '}': r = '{'; break;
                    case '[': r = ']'; forward = true; break;
                    case ']': r = '['; break;
                    case '(': r = ')'; forward = true; break;
                    case ')': r = '('; break;
                    default: 
                        LOG_ERROR("unexpected char '%c'", l); 
                        return;
                    }

                    i32 level = 1;
                    while (level >= 1) {
                        offset = forward ? 
                            buffer_seek_first_forward(view.buffer, {l, r}, offset) : 
                            buffer_seek_first_back(view.buffer, {l, r}, offset);

                        char c = char_at(buffer, offset);
                        if (c == l) level++;
                        else if (c == r) level--;
                    }

                    if (offset >= 0) {
                        view.caret.byte_offset = offset;
                        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                        if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                    }
                } break;
            case KC_ESC:
                FREE(mem_dynamic, app.incremental_search.str.data);
                app.incremental_search.str = {};
                break;
            case KC_F5: exec_process_command(); break;
            case KC_Q:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                ASSERT(!view.lines_dirty);
                view_seek_byte_offset(buffer_seek_beginning_of_line(view.buffer, view.lines, view.caret.wrapped_line));
                break;
            case KC_E:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                ASSERT(!view.lines_dirty);
                view_seek_byte_offset(buffer_seek_end_of_line(view.buffer, view.lines, view.caret.wrapped_line));
                break;
            case KC_RBRACKET:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                ASSERT(!view.lines_dirty);
                view_seek_line(buffer_seek_next_empty_line(view.buffer, view.lines, view.caret.wrapped_line));
                break;
            case KC_LBRACKET:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                ASSERT(!view.lines_dirty);
                view_seek_line(buffer_seek_prev_empty_line(view.buffer, view.lines, view.caret.wrapped_line));
                break;
            case KC_U: buffer_undo(view.buffer); break;
            case KC_R: buffer_redo(view.buffer); break;
            case KC_M:
                if (event.key.modifiers == MF_CTRL) SWAP(view.mark, view.caret);
                else view.mark = view.caret;
                break;
            case KC_O:
                app.lister.active = true;
                app.lister.selected_item = 0;

                for (String s : app.lister.values) FREE(mem_dynamic, s.data);
                app.lister.values.count = 0;

                list_files(&app.lister.values, "./", FILE_LIST_RECURSIVE, mem_dynamic);
                array_copy(&app.lister.filtered, app.lister.values);
                break;
            case KC_D: 
                if (buffer_valid(view.buffer)) {
                    ASSERT(!view.lines_dirty);
                    Range_i64 r = caret_range(view.caret, view.mark, view.lines, view.buffer, event.key.modifiers == MF_CTRL);
                    if (r.end == r.start) break;
                    
                    BufferHistoryScope h(view.buffer);
                    if (buffer_remove(view.buffer, r.start, r.end)) {
                        view.caret = view.caret.byte_offset > view.mark.byte_offset ? view.mark : view.caret;
                        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                        view.mark = view.caret;
                        move_view_to_caret();
                    }
                } break;
            case KC_X: {
                    BufferHistoryScope h(view.buffer);

                    ASSERT(!view.lines_dirty);
                    Range_i64 r = caret_range(view.caret, view.mark, view.lines, view.buffer, event.key.modifiers == MF_CTRL);
                    String str = buffer_read(view.buffer, r.start, r.end);
                    set_clipboard_data(str);

                    if (buffer_remove(view.buffer, r.start, r.end)) {
                        view.caret = view.caret.byte_offset > view.mark.byte_offset ? view.mark : view.caret;
                        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                        view.mark = view.caret;
                        move_view_to_caret();
                    }
                } break;
            case KC_Y: {
                    ASSERT(!view.lines_dirty);
                    Range_i64 r = caret_range(view.caret, view.mark, view.lines, view.buffer, event.key.modifiers == MF_CTRL);
                    set_clipboard_data(buffer_read(view.buffer, r.start, r.end));
                } break;
            case KC_N:
                if (app.incremental_search.str.length > 0) {
                    i64 offset = event.key.modifiers == MF_CTRL ? 
                        buffer_seek_back(view.buffer, app.incremental_search.str, view.caret.byte_offset) :
                        buffer_seek_forward(view.buffer, app.incremental_search.str, view.caret.byte_offset);
                    
                    if (offset != view.caret.byte_offset) {
                        view.caret.byte_offset = offset;
                        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                        if (app.incremental_search.set_mark) view.mark = view.caret;
                        
                        move_view_to_caret();
                    }
                } break;
            case KC_P:
                write_string(view.buffer, read_clipboard_str(), VIEW_SET_MARK);
                break;
            case KC_W:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_byte_offset(buffer_seek_next_word(view.buffer, view.caret.byte_offset));
                break;
            case KC_B:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_byte_offset(buffer_seek_prev_word(view.buffer, view.caret.byte_offset));
                break;
            case KC_J:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_line(view.caret.wrapped_line+1);
                break;
            case KC_K:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_line(view.caret.wrapped_line-1);
                break;
            case KC_I:
                app.next_mode = MODE_INSERT;
                view.mark = view.caret;
                break;
            case KC_L:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                view.caret.byte_offset = buffer_next_offset(view.buffer, view.caret.byte_offset);
                ASSERT(!view.lines_dirty);
                view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                move_view_to_caret();
                break;
            case KC_H:
                if (event.key.modifiers != MF_SHIFT) view.mark = view.caret;
                view.caret.byte_offset = buffer_prev_offset(view.buffer, view.caret.byte_offset);
                ASSERT(!view.lines_dirty);
                view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                move_view_to_caret();
                break;
            case KC_S:
                if (event.key.modifiers == MF_CTRL) buffer_save(view.buffer);
                break;
            case KC_TAB:
                if (auto buffer = get_buffer(view.buffer); buffer) {
                    i32 line = calc_unwrapped_line(view.caret.wrapped_line, view.lines);
                    Range_i32 r{ line, line };
                    if (event.key.modifiers & MF_CTRL) {
                        r = range_i32(line, calc_unwrapped_line(view.mark.wrapped_line, view.lines));
                    }

                    bool recalc_caret = false, recalc_mark = false;
                    defer {
                        if (recalc_caret) {
                            view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                            move_view_to_caret();
                        }

                        if (recalc_mark) {
                            view.mark = recalculate_caret(view.mark, view.buffer, view.lines);
                        }
                    };
                    
                    BufferHistoryScope h(view.buffer);

                    if (event.key.modifiers & MF_SHIFT) {
                        struct Edit { i64 offset; i32 bytes_to_remove; };
                        DynamicArray<Edit> edits{ .alloc = mem_tmp };
                        for (i32 l = r.start; l <= r.end; l = next_unwrapped_line(l, view.lines)) {
                            i64 line_start = line_start_offset(l, view.lines);

                            String current_indent = get_indent_for_line(view.buffer, view.lines, l);

                            i32 vcol = 0;
                            i32 i;
                            for (i = 0, vcol = 0; i < current_indent.length && vcol < buffer->tab_width; i++) {
                                if (current_indent[i] == '\t') vcol += buffer->tab_width - vcol % buffer->tab_width;
                                else vcol += 1;
                            }
                            i32 bytes_to_remove = i;

                            if (i > 0) array_add(&edits, { line_start, bytes_to_remove });
                        }
                        
                        for (i32 i = edits.count-1; i >= 0; i--) {
                            Edit edit = edits[i];

                            if (buffer_remove(view.buffer, edit.offset, edit.offset+edit.bytes_to_remove)) {
                                if (view.caret.byte_offset >= edit.offset) {
                                    view.caret.byte_offset -= edit.bytes_to_remove;
                                    recalc_caret = true;
                                }

                                if (view.mark.byte_offset > edit.offset) {
                                    view.mark.byte_offset -= edit.bytes_to_remove;
                                    recalc_mark = true;
                                }
                            }
                        }
                        
                    } else {
                        for (i32 l = r.end; l >= r.start; l = prev_unwrapped_line(l, view.lines)) {
                            i64 line_start = line_start_offset(l, view.lines);

                            i64 new_offset;
                            if (buffer->indent_with_tabs) new_offset = buffer_insert(view.buffer, line_start, "\t");
                            else {
                                String current_indent = get_indent_for_line(view.buffer, view.lines, l);

                                i32 vcol = 0;
                                for (i32 i = 0; i < current_indent.length; i++) {
                                    if (current_indent[i] == '\t') vcol += buffer->tab_width - vcol % buffer->tab_width;
                                    else vcol += 1;
                                }

                                i32 w = buffer->tab_width - vcol % buffer->tab_width;
                                char *spaces = ALLOC_ARR(mem_tmp, char, w);
                                memset(spaces, ' ', w);
                                new_offset = buffer_insert(view.buffer, line_start, { spaces, w });
                            }

                            if (view.caret.byte_offset >= line_start) {
                                view.caret.byte_offset += new_offset - line_start;
                                recalc_caret = true;
                            }

                            if (view.mark.byte_offset > line_start) {
                                view.mark.byte_offset += new_offset - line_start;
                                recalc_mark = true;
                            }
                        }
                    }
                } break;
            default: break;
            }
        } else if (app.mode == MODE_INSERT) {
            switch (event.key.keycode) {
            case KC_ESC:
                app.next_mode = MODE_EDIT;
                break;
            case KC_LEFT:
                view.caret.byte_offset = buffer_prev_offset(view.buffer, view.caret.byte_offset);
                ASSERT(!view.lines_dirty);
                view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                move_view_to_caret();
                break;
            case KC_RIGHT:
                view.caret.byte_offset = buffer_next_offset(view.buffer, view.caret.byte_offset);
                ASSERT(!view.lines_dirty);
                view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                move_view_to_caret();
                break;
            case KC_ENTER: 
                if (buffer_valid(view.buffer)) {
                    BufferHistoryScope h(view.buffer);
                    ASSERT(!view.lines_dirty);
                    String indent = get_indent_for_line(view.buffer, view.lines, view.caret.wrapped_line);
                    write_string(view.buffer, buffer_newline_str(view.buffer));
                    write_string(view.buffer, indent);
                } break;
            case KC_TAB: 
                if (Buffer *buffer = get_buffer(view.buffer); buffer) {
                    if (buffer->indent_with_tabs) write_string(view.buffer, "\t");
                    else {
                        String current_indent = get_indent_for_line(view.buffer, view.lines, view.caret.wrapped_line);

                        i32 vcol = 0;
                        for (i32 i = 0; i < current_indent.length; i++) {
                            if (current_indent[i] == '\t') vcol += buffer->tab_width - vcol % buffer->tab_width;
                            else vcol += 1;
                        }


                        i32 w = buffer->tab_width - vcol % buffer->tab_width;
                        char *spaces = ALLOC_ARR(mem_tmp, char, w);
                        memset(spaces, ' ', w);
                        write_string(view.buffer, { spaces, w });
                    }

                } break;
            case KC_BACKSPACE:
                if (buffer_valid(view.buffer)) {
                    BufferHistoryScope h(view.buffer);
                    i64 start = buffer_prev_offset(view.buffer, view.caret.byte_offset);
                    if (buffer_remove(view.buffer, start, view.caret.byte_offset)) {
                        view.caret.byte_offset = start;
                        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                        move_view_to_caret();
                    }
                } break;
            case KC_DELETE:
                if (buffer_valid(view.buffer)) {
                    BufferHistoryScope h(view.buffer);
                    i64 end = buffer_next_offset(view.buffer, view.caret.byte_offset);
                    if (buffer_remove(view.buffer, view.caret.byte_offset, end)) {
                        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                        move_view_to_caret();
                    }
                } break;

            default: break;
            }
        }

        if (app.mode == MODE_EDIT || app.mode == MODE_INSERT) {
            switch (event.key.keycode) {
            case KC_PAGE_DOWN:
                // TODO(jesper: page up/down should take current caret line into account when moving
                // view to caret, so that it remains where it was proportionally
                if (event.text.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_line(view.caret.wrapped_line+view.lines_visible-1);
                break;
            case KC_PAGE_UP:
                // TODO(jesper: page up/down should take current caret line into account when moving
                // view to caret, so that it remains where it was proportionally
                if (event.text.modifiers != MF_SHIFT) view.mark = view.caret;
                view_seek_line(view.caret.wrapped_line-view.lines_visible-1);
                break;
            default: break;
            }
        }
        break;
    case WE_MOUSE_MOVE:
        if (gui.hot == view.gui_id &&
            event.mouse.button == MB_PRIMARY &&
            point_in_rect({ (f32)event.mouse.x, (f32)event.mouse.y }, view.rect))
        {
            set_caret_xy(event.mouse.x, event.mouse.y);
            app.animating = true;
        } break;
    case WE_MOUSE_PRESS:
        if (gui.hot == view.gui_id &&
            event.mouse.button == MB_PRIMARY &&
            point_in_rect({ (f32)event.mouse.x, (f32)event.mouse.y }, view.rect))
        {
            set_caret_xy(event.mouse.x, event.mouse.y);
            view.mark = view.caret;
            app.animating = true;
        } break;
    case WE_MOUSE_WHEEL:
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

        ASSERT(!view.lines_dirty);
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


void update_and_render()
{
    //LOG_INFO("------- frame --------");
    //defer { LOG_INFO("-------- frame end -------\n\n"); };

    // NOTE(jesper): this is something of a hack because WM_CHAR messages come after the WM_KEYDOWN, and
    // we're listening to WM_KEYDOWN to determine whether to switch modes, so the actual mode switch has to
    // be deferred.
    if (app.mode != app.next_mode) {
        if (app.next_mode == MODE_INSERT) {
            buffer_history(view.buffer, { .type = BUFFER_HISTORY_GROUP_START });
        } else if (app.mode == MODE_INSERT) {
            buffer_history(view.buffer, { .type = BUFFER_HISTORY_GROUP_END });
            if (view.mark.byte_offset > view.caret.byte_offset) view.mark = view.caret;
        }
    }

    app.mode = app.next_mode;
    if (app.mode == MODE_INSERT || gui.capture_text[0]) enable_text_input();
    else disable_text_input();

    gfx_begin_frame();
    gui_begin_frame();

    GfxCommandBuffer debug_gfx = gfx_command_buffer();

    GuiLayout *root = gui_current_layout();
    gui_begin_layout({ .type = GUI_LAYOUT_ROW, .pos = root->pos, .size = root->size });

    gui_menu() {
        gui_menu("file") {
            if (gui_menu_button("save")) buffer_save(view.buffer);

            if (gui_menu_button("select working dir...")) {
                String wd = select_folder_dialog();
                if (wd.length > 0) set_working_dir(wd);
            }
        }

        gui_menu("debug") {
            gui_checkbox("buffer history", &debug.buffer_history.wnd.active);
        }

        if (app.process_commands.count > 0) {
            app.selected_process = gui_dropdown(app.process_command_names, app.selected_process);
            if (gui_button(16, app.icons.build)) exec_process_command();
        }
    }

    gui_window("buffer history", &debug.buffer_history.wnd) {
        char str[256];

		if (auto buffer = get_buffer(view.buffer); buffer) {
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
    }

    f32 lister_w = gfx.resolution.x*0.7f;
    lister_w = MAX(lister_w, 500.0f);
    lister_w = MIN(lister_w, gfx.resolution.x-10);

    Vector2 lister_p = gfx.resolution*0.5f;

    gui_window("open file", lister_p, { lister_w, 200.0f }, { 0.5f, 0.5f }, &app.lister.active) {
        GuiId id = GUI_ID(0);
        GuiEditboxAction edit_action = gui_editbox_id(id, "");

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

                i32 lower_buffer_size = 100;
                char *lower_buffer = ALLOC_ARR(mem_tmp, char, lower_buffer_size);

                for (String s : app.lister.values) {
                    fzy_score_t match_score;

                    array_resize(&D, needle.length*s.length);
                    array_resize(&M, needle.length*s.length);

                    memset(D.data, 0, D.count*sizeof *D.data);
                    memset(M.data, 0, M.count*sizeof *M.data);

                    array_resize(&match_bonus, s.length);

                    if (s.length >= lower_buffer_size) {
                        lower_buffer = REALLOC_ARR(mem_tmp, char, lower_buffer, lower_buffer_size, s.length);
                        lower_buffer_size = s.length;
                    }
                    memcpy(lower_buffer, s.data, s.length);

                    String ls{ lower_buffer, s.length };
                    to_lower(&ls);

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

                LOG_INFO("num items: %d, filtered: %d", app.lister.values.count, app.lister.filtered.count);
                quick_sort(scores, app.lister.filtered);
            }
        }

        GuiListerAction lister_action = gui_lister_id(id, app.lister.filtered, &app.lister.selected_item);
        if (app.lister.selected_item >= 0 && app.lister.selected_item < app.lister.filtered.count &&
            (lister_action == GUI_LISTER_FINISH || edit_action == GUI_EDITBOX_FINISH))
        {
            String file = app.lister.filtered[app.lister.selected_item];
            String path = absolute_path(file);

            BufferId buffer = find_buffer(path);
            if (!buffer) buffer = create_buffer(path);

            view_set_buffer(buffer);

            app.lister.active = false;
            gui.focused = GUI_ID_INVALID;
        } else if (lister_action == GUI_LISTER_CANCEL) {
            // TODO(jesper): this results in 1 frame of empty lister window being shown because
            // we don't really have a good way to close windows from within the window
            app.lister.active = false;
            gui.focused = GUI_ID_INVALID;
        }

        if (!app.lister.active) {
            for (String s : app.lister.values) FREE(mem_dynamic, s.data);
            app.lister.values.count = 0;
        }
    }

    view.caret_dirty |= view.lines_dirty;
    {
        gui_begin_layout({ .type = GUI_LAYOUT_ROW, .rect = gui_layout_widget_fill() });
        defer { gui_end_layout(); };

        // TODO(jesper): kind of only want to do the tabs if there are more than 1 file open, but then I'll
        // need to figure out reasonable ways of visualising unsaved changes as well as which file is open.
        // Likely do-able with the window titlebar decoration on most/all OS?
        if (view.buffers.count > 0) {
            GuiWindow *wnd = gui_current_window();

            Vector2 size{ 0, gui.style.button.font.line_height + 2 };
            Vector2 pos = gui_layout_widget(&size);
            gui_draw_rect(pos, size, wnd->clip_rect, gui.style.bg_dark1);

            gui_divider();

            gui_begin_layout({ .type = GUI_LAYOUT_COLUMN, .pos = pos, .size = size, .column.spacing = 2 });
            defer { gui_end_layout(); };

            FontAtlas *font = &gui.style.button.font;
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

                if (gui_clicked(id, pos, size)) active_buffer = b->id;

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
                        bgr_unpack(0xFF990000));
                }
            }

            if (cutoff >= 0) {
                // TODO(jesper): this is essentially a dropdown with an icon button as the trigger button
                // instead of a label button

                GuiId mid = GUI_ID(0);
                gui_hot_rect(mid, mpos, msize);
                if (gui_pressed(mid)) {
                    if (gui.focused == mid) {
                        gui.focused = GUI_ID_INVALID;
                    } else {
                        gui.focused = mid;
                    }
                }

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

                    Vector3 border_col = bgr_unpack(0xFFCCCCCC);
                    Vector3 bg = bgr_unpack(0xFF1d2021);
                    Vector3 bg_hot = bgr_unpack(0xFF3A3A3A);

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

                        // TODO(jesper): due to changes in gui_hot to not allow a new hot widget
                        // if one is pressed, this doesn't behave the way I want for dropdowns where
                        // I can press the trigger button, drag to an item, then release to select it
                        gui_hot_rect(lid, rect_p, rect_s);
                        if ((gui.hot == lid && gui.pressed == mid && !gui.mouse.left_pressed) ||
                            gui_clicked(lid, rect_p, rect_s))
                        {
                            active_buffer = b->id;
                            gui.focused = GUI_ID_INVALID;
                        }
                    }

                    GuiLayout *cl = gui_current_layout();
                    gui_draw_rect(cl->pos, cl->size, overlay->clip_rect, border_col, &overlay->command_buffer, bg_index);
                    gui_draw_rect(cl->pos + border, cl->size - 2*border, overlay->clip_rect, bg, &overlay->command_buffer, bg_index+1);

                }
            }

            view_set_buffer(active_buffer);
        }
        
        if (auto buffer = get_buffer(view.buffer); buffer) {
            if (app.incremental_search.active) {
                Vector2 size{ 0, 25.0f };
                Vector2 pos = gui_layout_widget(&size, GUI_ANCHOR_BOTTOM);
                gui_begin_layout({ .type = GUI_LAYOUT_ROW, .pos = pos, .size = size });
                defer { gui_end_layout(); };

                auto action = gui_editbox("");
                if (action & GUI_EDITBOX_CHANGE) {
                    String needle{ gui.edit.buffer, gui.edit.length };
                    string_copy(&app.incremental_search.str, needle, mem_dynamic);
                    
                    view.caret.byte_offset = buffer_seek_forward(view.buffer, needle, app.incremental_search.start_caret);
                    view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                    if (app.incremental_search.set_mark) view.mark = view.caret;
                } 
                
                if (action & GUI_EDITBOX_FINISH) {
                    app.incremental_search.active = false;
                }
                
                if (action & GUI_EDITBOX_CANCEL) {
                    FREE(mem_dynamic, app.incremental_search.str.data);
                    app.incremental_search.str = {};
                    
                    view.caret.byte_offset = app.incremental_search.start_caret;
                    view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                    if (app.incremental_search.set_mark) {
                        view.mark.byte_offset = app.incremental_search.start_mark;
                        view.mark = recalculate_caret(view.mark, view.buffer, view.lines);
                    }
                    app.incremental_search.active = false;
                }
            } else {
                Vector2 size{ 0, 15.0f };
                Vector2 pos = gui_layout_widget(&size, GUI_ANCHOR_BOTTOM);
                gui_begin_layout({ .type = GUI_LAYOUT_ROW, .pos = pos, .size = size });
                defer { gui_end_layout(); };

                String nl = string_from_enum(buffer->newline_mode);
                String in = buffer->indent_with_tabs ? String("TAB") : String("SPACE");

                char str[256];
                gui_textbox(
                    stringf(str, sizeof str,
                            "Ln: %d, Col: %lld, Pos: %lld, %.*s %.*s",
                            view.caret.line+1, view.caret.column+1, view.caret.byte_offset,
                            STRFMT(nl), STRFMT(in)));
            }
        }

        {
            gui_begin_layout({ .type = GUI_LAYOUT_COLUMN, .rect = gui_layout_widget_fill() });
            defer { gui_end_layout(); };

            // NOTE(jesper): the scrollbar is 1 frame delayed if the reflowing results in a different number of lines,
            // which we are doing afterwards to allow the gui layout system trigger line reflowing
            // We could move it to after the reflow handling with some more layout plumbing, by letter caller deal with
            // the layouting and passing in absolute positions and sizes to the scrollbar
            gui_scrollbar(app.mono.line_height, &view.line_offset, view.lines.count, view.lines_visible, &view.voffset);

            Rect text_rect = gui_layout_widget_fill();
            if (view.lines_dirty || text_rect != view.rect) {
                view.rect = text_rect;

                recalc_line_wrap(&view.lines, 0, view.lines.count, view.buffer);

                view.lines_dirty = false;
            }

            gui_hot_rect(view.gui_id, view.rect.pos, view.rect.size);
        }
    }

    // TODO(jesper): this probably signifies the caret APIs aren't all ready yet and it might
    // make sense for this path to go away completely when they are, but I won't completely discount that
    // we might still need a path to re-calculate it from scratch from a byte offset. But probably we
    // need to do that immediately instead of deferring, which first relies on all the line wrapping
    // recalculation to be done immediately as well
    if (view.caret_dirty) {
        ASSERT(!view.lines_dirty);
        view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
        view.caret_dirty = false;
    }
    view.mark = recalculate_caret(view.mark, view.buffer, view.lines);

    if (view.defer_move_view_to_caret) {
        i32 line_padding = 3;

        // TODO(jesper): do something sensible with the decimal vertical offset
        if (view.caret.wrapped_line-line_padding < view.line_offset) {
            view.line_offset = MAX(0, view.caret.wrapped_line-line_padding);
        } else if (view.caret.wrapped_line+line_padding > view.line_offset + view.lines_visible-4) {
            view.line_offset = MIN(view.lines.count-1, view.caret.wrapped_line+line_padding - view.lines_visible+4);
        }

        view.defer_move_view_to_caret = 0;
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
    
    if (buffer) {
        FontAtlas *font = &app.mono;

        Vector2 pos{ view.rect.pos.x, view.rect.pos.y };
        Vector2 size{ view.rect.size.x, view.rect.size.y };

        i32 columns = (view.rect.size.x / font->space_width) + 1;
        i32 rows = (view.rect.size.y / font->line_height) + 2;

        struct RangeColor {
            i64 start, end;
            u32 color;
        };
        DynamicArray<RangeColor> colors{ .alloc = mem_tmp };
        if (auto query = app.queries[buffer->language]; query && buffer->syntax_tree) {
            i64 start = line_start_offset(view.line_offset, view.lines);
            i64 end = line_end_offset(view.line_offset+rows, view.lines, view.buffer);

            TSNode root = ts_tree_root_node(buffer->syntax_tree);

            TSQueryCursor *cursor = ts_query_cursor_new();
            defer { ts_query_cursor_delete(cursor); };

            ts_query_cursor_set_byte_range(cursor, (u32)start, (u32)end);
            ts_query_cursor_exec(cursor, query, root);

            TSQueryMatch match;
            u32 capture_index;

            while (ts_query_cursor_next_capture(cursor, &match, &capture_index)) {
                auto *capture = &match.captures[capture_index];
                u32 start_byte = ts_node_start_byte(capture->node);
                u32 end_byte = ts_node_end_byte(capture->node);

                u32 capture_name_length;
                const char *tmp = ts_query_capture_name_for_id(query, capture->index, &capture_name_length);
                String capture_name{ (char*)tmp, (i32)capture_name_length };
                
#if DEBUG_TREE_SITTER_QUERY
                const char *node_type = ts_node_type(capture->node);
                LOG_INFO("query match id: %d, pattern_index: %d, capture_index: %d", match.id, match.pattern_index, capture_index);
                LOG_INFO("node type: %s, node range: [%d, %d]", node_type ? node_type : "null", start_byte, end_byte);
                LOG_INFO("capture name: %.*s", STRFMT(capture_name));
#endif
                String str = capture_name;
                u32 *color = nullptr;
                do {
                    color = find(&app.syntax_colors, str);
                    i32 p = last_of(str, '.');
                    if (p > 0) str = slice(str, 0, p);
                    else str.length = 0;
                } while(color == nullptr && str.length > 0);
                
                if (color) array_add(&colors, { start_byte, end_byte, *color });
            }
        }
        
        ANON_ARRAY(u32 glyph_index; u32 fg) glyphs{};

        void *mapped = nullptr;
        i32 buffer_size = 0;
        //if (columns*rows*(i32)sizeof glyphs[0] > buffer_size) 
        {
            if (mapped) {
                glUnmapNamedBuffer(view.glyph_data_ssbo);
                mapped = nullptr;
            }
            
            buffer_size = columns*rows*sizeof glyphs[0];
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, view.glyph_data_ssbo);
            glBufferData(GL_SHADER_STORAGE_BUFFER, buffer_size, nullptr, GL_STREAM_DRAW);
        }
        
        mapped = glMapNamedBufferRange(view.glyph_data_ssbo, 0, buffer_size, GL_MAP_WRITE_BIT);
        glyphs = { .data = (decltype(glyphs.data))mapped, .count = columns*rows };
        defer { glUnmapNamedBuffer(view.glyph_data_ssbo); };
        
        Vector3 fg = linear_from_sRGB(app.fg);
        
        for (auto &g : glyphs) {
            g.glyph_index = 0xFFFFFFFF;
            g.fg = bgr_pack(fg);
        }
        
        i32 current_color = 0;
        for (i32 line_index = view.line_offset; line_index < MIN(view.lines.count, view.line_offset+rows); line_index++) {
            i32 i = line_index-view.line_offset;
            i64 p = view.lines[line_index].offset;
            i64 end = line_end_offset(line_index, view.lines, buffer);

            i64 vcolumn = 0;
            while (p < end) {
                i64 pc = p;
                i32 c = utf32_it_next(buffer, &p);
                if (c == 0) break;

                if (c == '\n' || c == '\r') {
                    if (p < end && c == '\n' && char_at(buffer, p) == '\r') p = next_byte(buffer, p);
                    if (p < end && c == '\r' && char_at(buffer, p) == '\n') p = next_byte(buffer, p);
                    vcolumn = 0;
                    continue;
                }

                if (c == ' ') {
                    vcolumn++;
                    continue;
                }

                if (c == '\t') {
                    i32 w = buffer->tab_width - vcolumn % buffer->tab_width;
                    vcolumn += w;
                    continue;
                }
                
                // TODO(jesper): this is broken if a glyph is larger than the cell whatever unicode esque reason
                Glyph glyph = find_or_create_glyph(font, c);
                u32 index = (u32(glyph.x0) & 0xFFFF) | (u32(glyph.y0) << 16);
                glyphs[i*columns + vcolumn].glyph_index = index;
                
                while (current_color < colors.count && colors[current_color].end <= pc) current_color++;
                if (current_color < colors.count && pc >= colors[current_color].start) {
                    glyphs[i*columns+vcolumn].fg = colors[current_color].color;
                }

                vcolumn++;
            }
        }
        
        GfxCommand cmd{ 
            .type = GFX_COMMAND_MONO_TEXT,
            .mono_text = {
                .vbo = gfx.vbos.frame,
                .vbo_offset = gfx.frame_vertices.count,
                .glyph_ssbo = view.glyph_data_ssbo,
                .glyph_atlas = font->texture,
                .cell_size = { app.mono.space_width, app.mono.line_height },
                .pos = pos, 
                .offset = view.voffset,
                .line_offset = view.line_offset,
                .columns = columns,
            }
        };

        array_add(
            &gfx.frame_vertices, 
            {
                pos.x+size.x, pos.y,
                pos.x       , pos.y,
                pos.x       , pos.y+size.y,
                pos.x       , pos.y+size.y,
                pos.x+size.x, pos.y+size.y,
                pos.x+size.x, pos.y,
            });
        
        gfx_push_command(&gfx.frame_cmdbuf, cmd);
    }
    
    Vector3 clear_color = linear_from_sRGB(app.bg);
    glClearColor(clear_color.r, clear_color.g, clear_color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    gui_end_layout();
    
    app.animating = false;
    if (gui.capture_text[0] != gui.capture_text[1]) app.animating = true;
    gui_end_frame();

    gfx_flush_transfers();
    gfx_submit_commands(gfx.frame_cmdbuf);
    gfx_submit_commands(debug_gfx);

    gui_render();
}
    