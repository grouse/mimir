#include "core/platform.h"
#include "core/core.h"
#include "core/maths.h"
#include "core/process.h"
#include "core/memory.h"
#include "window.h"

#include "gui.cpp"
#include "fzy.cpp"

#include "tree_sitter/api.h"
extern "C" const TSLanguage* tree_sitter_cpp();
extern "C" const TSLanguage* tree_sitter_rust();
extern "C" const TSLanguage* tree_sitter_bash();
extern "C" const TSLanguage* tree_sitter_c_sharp();
extern "C" const TSLanguage* tree_sitter_lua();
extern "C" const TSLanguage* tree_sitter_comment();

#define DEBUG_LINE_WRAP_RECALC 0
#define DEBUG_TREE_SITTER_SYNTAX_TREE 0
#define DEBUG_TREE_SITTER_COLORS 0
#define DEBUG_TREE_SITTER_INJECTIONS 0

enum {
    APP_INPUT = APP_INPUT_ID_START,

    EDIT_MODE,
    INSERT_MODE,

    FUZZY_FIND_FILE,

    COPY_RANGE,
    CUT_RANGE,
    DELETE_RANGE,
};

enum EditMode {
    MODE_EDIT,
    MODE_INSERT,
};

enum BufferHistoryType {
    BUFFER_INSERT,
    BUFFER_REMOVE,

    BUFFER_CURSOR,
    BUFFER_GROUP_START,
    BUFFER_GROUP_END
};

enum Language {
    LANGUAGE_NONE,
    LANGUAGE_CPP,
    LANGUAGE_RUST,
    LANGUAGE_BASH,
    LANGUAGE_CS,
    LANGUAGE_LUA,
    LANGUAGE_COMMENT,
    LANGUAGE_COUNT,
};

struct SyntaxTree {
    Language language;
    TSTree *tree;
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

    bool operator==(const BufferId &rhs) const { return index == rhs.index; }
    bool operator!=(const BufferId &rhs) const { return index != rhs.index; }
    operator bool() { return *this != BUFFER_INVALID; }
};

struct BufferHistory {
    BufferHistoryType type;
    union {
        struct {
            i64 offset;
            String text;
        };
        struct {
            i32 view_id;
            i64 caret;
            i64 mark;
        };
    };
};

struct Buffer {
    BufferId id;

    String name;
    String file_path;

    Language language;
    TSTree *syntax_tree;
    DynamicArray<SyntaxTree> subtrees;

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

struct ProcessCommand {
    String exe;
    String args;

    Process *process;
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
    i32 id = -1;
    GuiId gui_id;

    GLuint glyph_data_ssbo;
    DynamicArray<BufferId> buffers;
    DynamicArray<ViewBufferState> saved_buffer_state;
    DynamicArray<BufferLine> lines;

    f32 voffset;
    i32 line_offset;
    i32 lines_visible;

    Rect rect;
    Rect text_rect;

    BufferId buffer;
    Caret caret, mark;

    struct {
        u64 lines_dirty : 1;
        u64 caret_dirty : 1;
        u64 defer_move_view_to_caret : 1;
    };
};

struct Application {
    AppWindow *wnd;
    FontAtlas mono;
    bool animating = true;

    struct {
        bool active;
        DynamicArray<String> values;
        DynamicArray<String> filtered;

        i32 selected_item;
    } lister;

    StaticArray<View, 5> views;
    View *current_view = &views[0];

    struct {
        String str;
        i64 start_caret, start_mark;
        bool active;
        bool set_mark;
    } incremental_search;

    HashTable<String, Language> language_map;
    const TSLanguage *languages[LANGUAGE_COUNT];
    TSQuery *highlights[LANGUAGE_COUNT];
    TSQuery *injections[LANGUAGE_COUNT];

    struct {
        InputMapId insert;
        InputMapId edit;
    } input;

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

struct RangeColor {
    i64 start, end;
    u32 color;

    bool operator>(const RangeColor &rhs) { return start > rhs.start; }
    bool operator<(const RangeColor &rhs) { return start < rhs.start; }
};

#include "gen/internal/mimir.h"


String string_from_enum(NewlineMode mode)
{
    switch (mode) {
    case NEWLINE_LF: return "LF";
    case NEWLINE_CR: return "CR";
    case NEWLINE_CRLF: return "CRLF";
    case NEWLINE_LFCR: return "LFCR";
    }
}

String string_from_enum(Language lang)
{
    switch (lang) {
    case LANGUAGE_CPP: return "CPP";
    case LANGUAGE_RUST: return "RUST";
    case LANGUAGE_BASH: return "BASH";
    case LANGUAGE_CS: return "CS";
    case LANGUAGE_LUA: return "LUA";
    case LANGUAGE_COMMENT: return "COMMENT";

    case LANGUAGE_NONE:
    case LANGUAGE_COUNT: return "invalid";
    }
};

String buffer_newline_str(Buffer *buffer)
{
    switch (buffer->newline_mode) {
    case NEWLINE_LF: return "\n";
    case NEWLINE_CR: return "\r";
    case NEWLINE_CRLF: return "\r\n";
    case NEWLINE_LFCR: return "\n\r";
    }
}

Allocator mem_frame;

Application app{};
DynamicArray<Buffer> buffers{};

struct {
    struct {
        GuiId wnd;
    } buffer_history;
} debug{};


void move_view_to_caret(View *view);


Buffer* get_buffer(BufferId buffer_id)
{
    if (buffer_id.index < 0) return nullptr;
    return &buffers[buffer_id.index];
}

String buffer_newline_str(BufferId buffer_id)
{
    Buffer *buffer = get_buffer(buffer_id);
    return buffer_newline_str(buffer);
}

bool buffer_valid(BufferId buffer_id)
{
    return buffer_id.index >= 0;
}

TSLogger ts_logger  {
    .payload = nullptr,
    .log = [](void */*payload*/, TSLogType type, const char *msg) {
        const char *type_s = nullptr;
        switch (type) {
        case TSLogTypeParse: type_s = "parse"; break;
        case TSLogTypeLex: type_s = "lex"; break;
        }
        LOG_INFO("tree-sitter [%s]: %s", type_s, msg);
    }
};

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
    if (auto a = find_asset<String>(highlights); a) {
        u32 error_loc;
        TSQueryError error;
        TSQuery *query = ts_query_new(lang, a->data, a->length, &error_loc, &error);

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

HashTable<u32, DynamicArray<TSRange>> ts_get_injection_ranges(
    Buffer *buffer,
    TSQuery *injection_query,
    Allocator mem)
{
    if (!buffer) return {};
    if (!buffer->syntax_tree) return {};

    SArena scratch = tl_scratch_arena(mem);

    ASSERT(injection_query);

    HashTable<u32, DynamicArray<TSRange>> lang_range_map{ .alloc = mem };

    TSNode root = ts_tree_root_node(buffer->syntax_tree);
    TSQueryCursor *cursor = ts_query_cursor_new();
    defer { ts_query_cursor_delete(cursor); };

    ts_query_cursor_set_byte_range(cursor, 0, buffer->flat.size);
    ts_query_cursor_exec(cursor, injection_query, root);


    TSQueryMatch match;
    u32 capture_index;
    while (ts_query_cursor_next_capture(cursor, &match, &capture_index)) {
        auto *capture = &match.captures[capture_index];

        u32 capture_name_length;
        const char *tmp = ts_query_capture_name_for_id(injection_query, capture->index, &capture_name_length);
        String capture_name{ (char*)tmp, (i32)capture_name_length };

        if (Language *l = map_find(&app.language_map, capture_name); l) {
            DynamicArray<TSRange> *ranges = map_find_emplace(&lang_range_map, (u32)*l, { .alloc = scratch });

            u32 start_byte = ts_node_start_byte(capture->node);
            u32 end_byte = ts_node_end_byte(capture->node);

            array_add(
                ranges,
                {
                    .start_point = ts_node_start_point(capture->node),
                    .end_point = ts_node_end_point(capture->node),
                    .start_byte = start_byte,
                    .end_byte = end_byte,
                });
        }
    }

    return lang_range_map;
}

void ts_parse_buffer(Buffer *buffer, TSInputEdit edit = {}) INTERNAL
{
    SArena scratch = tl_scratch_arena();

    auto lang = app.languages[buffer->language];
    if (!lang) return;

    TSParser *parser = ts_parser_new();
    defer { ts_parser_delete(parser); };
    ts_parser_set_language(parser, lang);
    buffer->syntax_tree = ts_parser_parse_string(parser, buffer->syntax_tree, buffer->flat.data, buffer->flat.size);

    if (auto inj = app.injections[buffer->language]; inj) {
        HashTable<u32, DynamicArray<TSRange>> lang_range_map = ts_get_injection_ranges(buffer, inj, scratch);

        for (auto it : lang_range_map) {
            SyntaxTree *subtree = nullptr;
            for (auto &st : buffer->subtrees) {
                if (st.language == it.key) {
                    subtree = &st;
                    break;
                }
            }

            if (!subtree) {
                i32 i = array_add(&buffer->subtrees, {
                    .language = (Language)it.key,
                });
                subtree = &buffer->subtrees[i];
            }

            DynamicArray<TSRange> invalid_ranges{ .alloc = scratch };
            array_reserve(&invalid_ranges, it->count);

            if (edit.old_end_byte == edit.new_end_byte) {
                array_copy(&invalid_ranges, *it);
            } else {
                for (auto range : *it) {
                    if (range.start_byte >= edit.old_end_byte) continue;
                    if (range.end_byte <= edit.start_byte) continue;
                    array_add(&invalid_ranges, range);
                }
            }

            if (invalid_ranges.count) {
                LOG_INFO("updating %d (%d) injected %.*s ranges", invalid_ranges.count, it->count, STRFMT(string_from_enum((Language)it.key)));
                for (auto range : invalid_ranges) LOG_INFO("injected range: [%u, %u]", range.start_byte, range.end_byte);

                ts_parser_set_language(parser, app.languages[it.key]);
                ts_parser_set_included_ranges(parser, invalid_ranges.data, invalid_ranges.count);
                subtree->tree = ts_parser_parse_string(parser,  subtree->tree, buffer->flat.data, buffer->flat.size);
            }
        }
    }
}

void ts_update_buffer(Buffer *buffer, TSInputEdit edit)
{
    if (auto lang = app.languages[buffer->language]; lang) {
        if (buffer->syntax_tree) ts_tree_edit(buffer->syntax_tree, &edit);
        for (auto st : buffer->subtrees) ts_tree_edit(st.tree, &edit);
    }

    ts_parse_buffer(buffer, edit);
}


void get_syntax_colors(
    DynamicArray<RangeColor> *colors,
    i64 byte_start,
    i64 byte_end,
    TSTree *syntax_tree,
    Language language)
{
    auto query = app.highlights[language];
    if (!query || !syntax_tree) return;

    i32 insert_at = 0;
    i32 parent_index = 0;

    TSNode root = ts_tree_root_node(syntax_tree);

    TSQueryCursor *cursor = ts_query_cursor_new();
    defer { ts_query_cursor_delete(cursor); };

    ts_query_cursor_set_byte_range(cursor, (u32)byte_start, (u32)byte_end);
    ts_query_cursor_exec(cursor, query, root);

    TSQueryMatch match;
    u32 capture_index;

    while (ts_query_cursor_next_capture(cursor, &match, &capture_index)) {
        auto *capture = &match.captures[capture_index];
        u32 start_byte = ts_node_start_byte(capture->node);
        u32 end_byte = ts_node_end_byte(capture->node);

        i32 old_parent_index = parent_index;
        while (parent_index < colors->count &&
               (start_byte < colors->at(parent_index).start || end_byte > colors->at(parent_index).end))
        {
            parent_index++;
        }

        if (old_parent_index != parent_index) {
            insert_at = MIN(colors->count, parent_index+1);
        }

        u32 capture_name_length;
        const char *tmp = ts_query_capture_name_for_id(query, capture->index, &capture_name_length);
        String capture_name{ (char*)tmp, (i32)capture_name_length };

#if DEBUG_TREE_SITTER_COLORS
        if (false) {
            const char *node_type = ts_node_type(capture->node);
            LOG_INFO("query match id: %d, pattern_index: %d, capture_index: %d", match.id, match.pattern_index, capture_index);
            LOG_INFO("node type: %s, node range: [%d, %d]", node_type ? node_type : "null", start_byte, end_byte);
            LOG_INFO("capture name: %.*s", STRFMT(capture_name));
        }
#endif
        String str = capture_name;
        u32 *color = nullptr;
        do {
            color = map_find(&app.syntax_colors, str);
            i32 p = last_of(str, '.');
            if (p > 0) str = slice(str, 0, p);
            else str.length = 0;
        } while(color == nullptr && str.length > 0);

        if (color) {
#if DEBUG_TREE_SITTER_COLORS
            LOG_INFO("highlight color[%d] '%.*s' in [%d, %d]", insert_at, STRFMT(capture_name), start_byte, end_byte);
#endif
            array_insert(colors, insert_at++, { start_byte, end_byte, *color });

            if (parent_index < colors->count && parent_index != insert_at-1) {
                if (colors->at(parent_index).end> start_byte) {
                    auto parent_end = colors->at(parent_index).end;
                    colors->at(parent_index).end = start_byte;

                    if (parent_end - end_byte > 0) {
                        array_insert(colors, insert_at++, { end_byte, parent_end, colors->at(parent_index).color });
                        parent_index = insert_at-1;
                    }

                }
            }
        }
    }
}

void buffer_history(BufferId buffer_id, BufferHistory entry)
{
    Buffer *buffer = get_buffer(buffer_id);
    ASSERT(buffer);

    if (entry.type == BUFFER_GROUP_END) {
        if (buffer->history.count > 2 &&
            buffer->history[buffer->history.count-1].type == BUFFER_CURSOR &&
            buffer->history[buffer->history.count-2].type == BUFFER_CURSOR &&
            buffer->history[buffer->history.count-3].type == BUFFER_GROUP_START)
        {
            buffer->history.count -= 3;
            buffer->history_index = MIN(buffer->history_index, buffer->history.count-1);
            return;
        } else if (buffer->history.count > 0 &&
                   buffer->history[buffer->history.count-1].type == BUFFER_GROUP_START)
        {
            buffer->history.count -= 1;
            buffer->history_index = MIN(buffer->history_index, buffer->history.count-1);
            return;
        }

        // TODO(jesper): ideally this collapsing should be happening at the insertion of the nodes instead of a post-cleanup
        for (i32 i = buffer->history.count-1; i >= 0; i--) {
            if (buffer->history[i].type != BUFFER_GROUP_END) break;
            array_remove(&buffer->history, i--);

            for (auto it : reverse(slice(buffer->history, 0, i))) {
                if (it->type == BUFFER_GROUP_START) {
                    array_remove(&buffer->history, it.index); i = it.index;
                    break;
                }

            }
        }

        i32 prev_cursor = -1, last_cursor = -1;
        for (auto it : reverse(buffer->history)) {
            if (it->type == BUFFER_GROUP_START) break;
            if (it->type == BUFFER_CURSOR) {
                if (last_cursor == -1) {
                    last_cursor = it.index;
                } else if (prev_cursor != -1) {
                    array_remove(&buffer->history, prev_cursor);
                    prev_cursor = it.index;
                } else {
                    prev_cursor = it.index;
                }
            }
        }

        i32 prev_insert = -1;
        for (auto it : reverse(buffer->history)) {
            if (it->type == BUFFER_GROUP_START) break;
            if (it->type == BUFFER_REMOVE) break;
            if (it->type == BUFFER_INSERT) {
                if (prev_insert != -1) {
                    auto *prev = &buffer->history[prev_insert];
                    if (it->offset+it->text.length == prev->offset) {
                        it->text.data = REALLOC_ARR(mem_dynamic, char, it->text.data, it->text.length, it->text.length+prev->text.length);
                        memcpy(it->text.data+it->text.length, prev->text.data, prev->text.length);
                        it->text.length += prev->text.length;
                        array_remove(&buffer->history, prev_insert);
                    }
                }

                prev_insert = it.index;
            }
        }

        buffer->history_index = MIN(buffer->history_index, buffer->history.count-1);
    }

    // TODO(jesper): history allocator. We're leaking the memory of the history strings
    // that we're overwriting after this. We should probably have some kind of per buffer
    // circular buffer allocator for these with some intrusive linked list type situation
    if (buffer->history_index+1 < buffer->history.count) {
        buffer->history.count = buffer->history_index+1;
    }

    switch (entry.type) {
    case BUFFER_REMOVE:
    case BUFFER_INSERT:
        entry.text = duplicate_string(entry.text, mem_dynamic);
        break;
    case BUFFER_GROUP_START:
    case BUFFER_GROUP_END:
    case BUFFER_CURSOR:
        break;
    }


    array_add(&buffer->history, entry);
    buffer->history_index = buffer->history.count-1;
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

void calculate_num_visible_lines(View *view)
{
    i32 lines_visible = (i32)ceilf(view->rect.size().y / (f32)app.mono.line_height);
    view->lines_dirty = view->lines_dirty || lines_visible != view->lines_visible;
    view->lines_visible = lines_visible;
}

BufferId create_buffer(String file)
{
    SArena scratch = tl_scratch_arena();

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
    } else if (ext == ".sh") {
        b.language = LANGUAGE_BASH;
    } else if (ext == ".cs") {
        b.language = LANGUAGE_CS;
    } else if (ext == ".lua") {
        b.language = LANGUAGE_LUA;
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

        ts_parse_buffer(&b);
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

void view_set_buffer(View *view, BufferId buffer)
{
    if (view->buffer == buffer) return;

    auto find_buffer_state = [](View *view, BufferId buffer) -> ViewBufferState*
    {
        for (auto &state : view->saved_buffer_state) {
            if (state.buffer == buffer) return &state;
        }
        return nullptr;
    };

    ViewBufferState state{
        .buffer = view->buffer,
        .caret = view->caret,
        .mark = view->mark,
        .voffset = view->voffset,
        .line_offset = view->line_offset,
    };

    ViewBufferState *it = find_buffer_state(view, view->buffer);
    if (!it) array_add(&view->saved_buffer_state, state);
    else *it = state;

    it = find_buffer_state(view, buffer);
    if (it) {
        view->caret = it->caret;
        view->mark = it->mark;
        view->voffset = it->voffset;
        view->line_offset = it->line_offset;
    } else {
        view->caret = view->mark = {};
        view->voffset = 0;
        view->line_offset = 0;
    }

    view->buffer = buffer;

    i32 idx = array_find_index(view->buffers, view->buffer);
    if (idx >= 0) array_remove(&view->buffers, idx);
    array_insert(&view->buffers, 0, view->buffer);

    // NOTE(jesper): not storing line wrap data (for now). Computing it is super quick, and storing
    // it will add up pretty quick. Especially if we start serialising the views and their state
    // to disk
    view->lines_dirty = true;
}

int app_main(Array<String> args)
{
    SArena scratch = tl_scratch_arena();

    Vector2i resolution{ 1280, 720 };
    if (i32 data[2];
        parse_cmd_argument(args.data, args.count, "--resolution", data))
    {
        resolution[0] = data[0];
        resolution[1] = data[1];
    }

    app.wnd = create_window({"mimir", resolution.x, resolution.y });

    fzy_init_table();

    String exe_folder = get_exe_folder(scratch);

    String asset_folders[] = {
        "./",
        exe_folder,
        join_path(exe_folder, "assets", scratch),
        ASSETS_DIR,
    };

    extern ASSET_LOAD_PROC(gfx_load_texture_asset);
    extern ASSET_LOAD_PROC(gfx_load_shader_asset);
    extern ASSET_LOAD_PROC(load_string_asset);

    init_assets(
        { asset_folders, ARRAY_COUNT(asset_folders) },
        AssetTypesDesc{ .types = {
            { ".scm",  typeid(String),       &load_string_asset,      nullptr },
            { ".png",  typeid(TextureAsset), &gfx_load_texture_asset, nullptr },
            { ".glsl", typeid(ShaderAsset),  &gfx_load_shader_asset,  nullptr },
        }});

    init_gfx(get_client_resolution(app.wnd));
    init_gui();

    init_input_map(app.input.edit, {
        { INSERT_MODE,     EDGE_DOWN, KEYBOARD, .keyboard = { KC_I } },
        { CANCEL,          EDGE_DOWN, KEYBOARD, .keyboard = { KC_ESC } },

        { UNDO,            EDGE_DOWN, KEYBOARD, .keyboard = { KC_U } },
        { REDO,            EDGE_DOWN, KEYBOARD, .keyboard = { KC_R } },

        { SAVE,            EDGE_DOWN, KEYBOARD, .keyboard = { KC_S, MF_CTRL } },

        { FUZZY_FIND_FILE, EDGE_DOWN, KEYBOARD, .keyboard = { KC_O } },

        { PASTE,           EDGE_DOWN, KEYBOARD, .keyboard = { KC_P } },
        { COPY_RANGE,      EDGE_DOWN, KEYBOARD, .keyboard = { KC_Y } },
        { CUT_RANGE,       EDGE_DOWN, KEYBOARD, .keyboard = { KC_X } },
        { DELETE_RANGE,    EDGE_DOWN, KEYBOARD, .keyboard = { KC_D } },
    });

    init_input_map(app.input.insert, {
        { EDIT_MODE, EDGE_DOWN, KEYBOARD, .keyboard = { KC_ESC } },
        { TEXT_INPUT, TEXT },
    });

    app.mode = app.next_mode = MODE_EDIT;
    //set_input_map(app.input.edit);

    map_set(&app.language_map, "cpp", LANGUAGE_CPP);
    map_set(&app.language_map, "comment", LANGUAGE_COMMENT);

    app.languages[LANGUAGE_CPP] = tree_sitter_cpp();
    app.languages[LANGUAGE_CS] = tree_sitter_c_sharp();
    app.languages[LANGUAGE_RUST] = tree_sitter_rust();
    app.languages[LANGUAGE_BASH] = tree_sitter_bash();
    app.languages[LANGUAGE_LUA] = tree_sitter_lua();
    app.languages[LANGUAGE_COMMENT] = tree_sitter_comment();

    app.highlights[LANGUAGE_CPP] = ts_create_query(app.languages[LANGUAGE_CPP], "queries/cpp/highlights.scm");
    app.highlights[LANGUAGE_RUST] = ts_create_query(app.languages[LANGUAGE_RUST], "queries/rust/highlights.scm");
    app.highlights[LANGUAGE_BASH] = ts_create_query(app.languages[LANGUAGE_BASH], "queries/bash/highlights.scm");
    app.highlights[LANGUAGE_CS] = ts_create_query(app.languages[LANGUAGE_CS], "queries/cs/highlights.scm");
    app.highlights[LANGUAGE_LUA] = ts_create_query(app.languages[LANGUAGE_LUA], "queries/lua/highlights.scm");
    app.highlights[LANGUAGE_COMMENT] = ts_create_query(app.languages[LANGUAGE_COMMENT], "queries/comment/highlights.scm");

    app.injections[LANGUAGE_CPP] = ts_create_query(app.languages[LANGUAGE_CPP], "queries/cpp/injections.scm");

    ts_custom_alloc = vm_freelist_allocator(5*1024*1024*1024ull);
    ts_set_allocator(ts_custom_malloc, ts_custom_calloc, ts_custom_realloc, ts_custom_free);

    u32 fg = bgr_pack(app.fg);
    map_set(&app.syntax_colors, "unused", fg);
    map_set(&app.syntax_colors, "_parent", fg);
    map_set(&app.syntax_colors, "label", fg);
    map_set(&app.syntax_colors, "parameter", fg);
    map_set(&app.syntax_colors, "property", fg);
    map_set(&app.syntax_colors, "variable", fg);
    map_set(&app.syntax_colors, "identifier", fg);

    map_set(&app.syntax_colors, "text.warning", 0xff0000u);

    map_set(&app.syntax_colors, "preproc", 0xFE8019u);
    map_set(&app.syntax_colors, "include", 0xFE8019u);
    map_set(&app.syntax_colors, "string", 0x689D6Au);
    map_set(&app.syntax_colors, "comment", 0x8EC07Cu);
    map_set(&app.syntax_colors, "function", 0xccb486u);
    map_set(&app.syntax_colors, "operator", 0xfcedcfu);
    map_set(&app.syntax_colors, "punctuation", 0xfcedcfu);
    map_set(&app.syntax_colors, "type", 0xbcbf91u);
    map_set(&app.syntax_colors, "constant", 0xe9e4c6u);
    map_set(&app.syntax_colors, "keyword", 0xd36e2au);
    map_set(&app.syntax_colors, "namespace", 0xd36e2au);
    map_set(&app.syntax_colors, "preproc.identifier", 0x84a89au);
    map_set(&app.syntax_colors, "attribute", 0x84a89au);

    // TODO(jesper): this is kinda neat but I think I might want this as some kind of underline or background
    // style instead of foreground colour?
    //set(&app.syntax_colors, "error", 0xFFFF0000);

    // TODO(jesper): I definitely need to figure this shit out
    for (i32 i = 0; i < app.syntax_colors.capacity; i++) {
        if (!app.syntax_colors.slots[i].occupied) continue;
        Vector3 c = bgr_unpack(app.syntax_colors.slots[i].value);
        app.syntax_colors.slots[i].value = bgr_pack(linear_from_sRGB(c));
    }

    // if (auto a = find_asset("textures/build_16x16.png"); a) app.icons.build = gfx_load_texture(a->data, a->size);

    app.mono = create_font("fonts/UbuntuMono/UbuntuMono-Regular.ttf", 18, true);

    for (auto it : iterator(app.views)) {
        it->gui_id = gui_gen_id(it.index);
        it->lines_dirty = true;
        glGenBuffers(1, &it->glyph_data_ssbo);
    }
    app.current_view->id = 0;

    if (args.count > 0) {
	    bool is_dir = is_directory(args[0]);
	    String dir = is_dir ? args[0] : directory_of(args[0]);
		set_working_dir(dir);

		if (!is_dir) view_set_buffer(app.current_view, create_buffer(args[0]));
    }

    Array<String> files = list_files(get_working_dir(scratch), scratch);
    for (String s : files) {
#ifdef _WIN32
        if (extension_of(s) == ".bat") {
            String full = absolute_path(s, mem_dynamic);
            array_add(&app.process_command_names, filename_of(full));
            array_add(&app.process_commands, { .exe = full });
        }
#endif

#ifdef __linux__
        if (extension_of(s) == ".sh") {
            String full = absolute_path(s, mem_dynamic);
            array_add(&app.process_command_names, filename_of(full));
            array_add(&app.process_commands, { .exe = full });
        }
#endif
    }

    debug.buffer_history.wnd = gui_create_window({ "history", .position = { 0, 40 }, .size = { 300, 200 } });


    u64 last_time, current_time = wall_timestamp();
    while (true) {
        RESET_ALLOC(mem_frame);

        gfx_begin_frame();

        last_time = current_time;
        current_time = wall_timestamp();
        //f32 dt = wall_duration_s(last_time, current_time);

        app_gather_input(app.wnd);
        update_and_render();
        present_window(app.wnd);
    }

    return 0;
}

bool app_change_resolution(Vector2 resolution)
{
    if (!gfx_change_resolution(resolution)) return false;
    for (View &view : app.views) view.lines_dirty = true;
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

void recalc_line_wrap(View *view, DynamicArray<BufferLine> *lines, i32 start_line, i32 end_line, BufferId buffer_id)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return;

    SArena scratch = tl_scratch_arena(lines->alloc);

    Rect r = view->rect;
    f32 base_x = r.tl.x;

    start_line = MAX(start_line, 0);
    end_line = MIN(end_line, lines->count);

    i64 start = line_start_offset(start_line, *lines);
    i64 end = line_end_offset(end_line, *lines, buffer);

    i32 prev_c = ' ';
    i64 vcolumn = 0;

    i64 p = start;
    i64 word_start = p;
    i64 line_start = p;

    DynamicArray<BufferLine> tmp_lines{ .alloc = scratch };
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
        if (x1 >= r.br.x) {
            if (!is_word_boundary(c) && line_start != word_start) {
                p = line_start = word_start;
            } else {
                p = line_start = word_start = pn;
            }
            vcolumn = 0;

            array_add(new_lines, { (i64)line_start, .wrapped = true });
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
    SArena scratch = tl_scratch_arena();

    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return false;

    switch (buffer->type) {
    case BUFFER_FLAT: {
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

            // TODO(jesper): multi-view support
            for (View &view : app.views) {
                if (view.buffer != buffer_id) continue;

                i32 line = wrapped_line_from_offset(byte_start, view.lines, view.caret.wrapped_line);
                i64 start_offset = view.lines[line].offset;
                for (i32 i = line+1; i < view.lines.count; i++) {
                    view.lines[i].offset -= num_bytes;

                    if (view.lines[i].offset <= start_offset) array_remove(&view.lines, i--);
                }

                recalc_line_wrap(
                    &view,
                    &view.lines,
                    prev_unwrapped_line(line, view.lines),
                    next_unwrapped_line(line, view.lines),
                    view.buffer);
            }

            ts_update_buffer(buffer, {
                .start_byte = (u32)byte_start,
                .old_end_byte = (u32)byte_end,
                .new_end_byte = (u32)byte_start,
            });
        } break;
    }


    return true;
}

String buffer_read(BufferId buffer_id, i64 byte_start, i64 byte_end, Allocator mem)
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
    SArena scratch = tl_scratch_arena();

    if (in_text.length <= 0) return offset;

    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return offset;

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
        text.data = ALLOC_ARR(*scratch, char, required_extra_space);
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

            for (View &view : app.views) {
                if (view.buffer != buffer_id) continue;

                i32 line = wrapped_line_from_offset(offset, view.lines, view.caret.wrapped_line);
                for (i32 i = line+1; i < view.lines.count; i++) {
                    view.lines[i].offset += required_extra_space;
                }

                recalc_line_wrap(
                    &view,
                    &view.lines,
                    calc_unwrapped_line(line, view.lines),
                    next_unwrapped_line(line, view.lines),
                    view.buffer);
            }

            ts_update_buffer(buffer, {
                .start_byte = (u32)offset,
                .old_end_byte = (u32)offset,
                .new_end_byte = (u32)(offset+required_extra_space),
            });
        } break;
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
        BufferHistory h = buffer->history[buffer->history_index--];
        switch (h.type) {
        case BUFFER_INSERT:
            buffer_remove(buffer_id, h.offset, h.offset+h.text.length, false);
            break;
        case BUFFER_REMOVE:
            buffer_insert(buffer_id, h.offset, h.text, false);
            break;
        case BUFFER_CURSOR:
            // TODO(jesper): this is broken if views change between history
            // entries
            if (app.views[h.view_id].buffer != buffer_id) continue;

            app.views[h.view_id].caret.byte_offset = h.caret;
            app.views[h.view_id].mark.byte_offset = h.mark;
            app.views[h.view_id].caret_dirty = true;
            break;
        case BUFFER_GROUP_START:
            group_level++;
            break;
        case BUFFER_GROUP_END:
            group_level--;
            break;
        }
    } while (group_level < 0 && buffer->history_index > 0);

    // TODO(jesper): maybe we should just store the view coordinates in the buffer history?
    for (View &view : app.views) {
        if (view.buffer == buffer_id) move_view_to_caret(&view);
    }
}

void buffer_redo(BufferId buffer_id)
{
    Buffer *buffer = get_buffer(buffer_id);
    if (!buffer) return;

    if (buffer->history_index+1 == buffer->history.count) return;

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
        case BUFFER_CURSOR:
            // TODO(jesper): this is broken if views change between history
            // entries
            if (app.views[h.view_id].buffer != buffer_id) continue;

            app.views[h.view_id].caret.byte_offset = h.caret;
            app.views[h.view_id].mark.byte_offset = h.mark;
            app.views[h.view_id].caret_dirty = true;
            break;
        case BUFFER_GROUP_START:
            group_level++;
            break;
        case BUFFER_GROUP_END:
            group_level--;
            break;
        }
    } while (group_level > 0 && buffer->history_index+1 < buffer->history.count);

    // TODO(jesper): maybe we should just store the view coordinates in the buffer history?
    for (View &view : app.views) {
        if (view.buffer == buffer_id) move_view_to_caret(&view);
    }
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

String get_indent_for_line(BufferId buffer_id, Array<BufferLine> lines, i32 line, Allocator mem)
{
    line = seek_non_empty_line_back(buffer_id, lines, line);
    i64 line_start = buffer_seek_beginning_of_line(buffer_id, lines, line);
    i64 abs_start = line_start_offset(line, lines);

    return buffer_read(buffer_id, abs_start, line_start, mem);
}

bool app_needs_render()
{
    if (app.next_mode != app.mode || app.animating) return true;
    for (View &view : app.views) if (view.lines_dirty || view.caret_dirty) return true;
    return false;
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

void move_view_to_caret(View *view)
{
    view->defer_move_view_to_caret = 1;
}

// NOTE(jesper): this is poorly named. It starts/stops a history group and records the active view's
// caret and mark appropriately so that undo/redo also moves the carets appropriately
struct BufferHistoryScope {
    BufferId buffer;

    BufferHistoryScope(BufferId buffer) : buffer(buffer)
    {
        buffer_history(buffer, { .type = BUFFER_GROUP_START });
        for (View &view : app.views) {
            if (view.buffer != buffer) continue;
            buffer_history(buffer, { .type = BUFFER_CURSOR, .view_id = view.id, .caret = view.caret.byte_offset, .mark = view.mark.byte_offset });
        }
    }

    ~BufferHistoryScope()
    {
        for (View &view : app.views) {
            if (view.buffer != buffer) continue;
            buffer_history(buffer, { .type = BUFFER_CURSOR, .view_id = view.id, .caret = view.caret.byte_offset, .mark = view.mark.byte_offset });
        }
        buffer_history(buffer, { .type = BUFFER_GROUP_END });
    }
};

void view_seek_line(View *view, i32 wrapped_line)
{
    ASSERT(!view->lines_dirty);
    Buffer *buffer = get_buffer(view->buffer);

    wrapped_line = CLAMP(wrapped_line, 0, view->lines.count-1);
    if (buffer && wrapped_line != view->caret.wrapped_line) {
        i32 l = CLAMP(view->caret.wrapped_line, 0, view->lines.count-1);
        while (l > 0 && l > wrapped_line) {
            if (!view->lines[l--].wrapped) view->caret.line--;
        }

        while (l < view->lines.count-1 && l < wrapped_line) {
            if (!view->lines[++l].wrapped) view->caret.line++;
        }

        view->caret.wrapped_line = l;
        view->caret.wrapped_column = calc_wrapped_column(view->caret.wrapped_line, view->caret.preferred_column, view->lines, buffer);
        view->caret.column = calc_unwrapped_column(view->caret.wrapped_line, view->caret.wrapped_column, view->lines, buffer);
        view->caret.byte_offset = calc_byte_offset(view->caret.wrapped_column, view->caret.wrapped_line, view->lines, buffer);

        move_view_to_caret(view);

        app.animating = true;
    }
}

void view_seek_byte_offset(View *view, i64 byte_offset)
{
    byte_offset = CLAMP(byte_offset, 0, buffer_end(view->buffer));

    if (byte_offset != view->caret.byte_offset) {
        view->caret.byte_offset = byte_offset;

        ASSERT(!view->lines_dirty);
        view->caret = recalculate_caret(view->caret, view->buffer, view->lines);
        view->mark = recalculate_caret(view->mark, view->buffer, view->lines);

        move_view_to_caret(view);

        app.animating = true;
    }
}

void write_string(View *view, BufferId buffer_id, String str, u32 flags = 0)
{
    if (!buffer_valid(buffer_id)) return;
    if (flags & VIEW_SET_MARK) view->mark = view->caret;

    BufferHistoryScope h(buffer_id);
    view->caret.byte_offset = buffer_insert(buffer_id, view->caret.byte_offset, str);
    ASSERT(!view->lines_dirty);
    view->caret = recalculate_caret(view->caret, view->buffer, view->lines);
    view->mark = recalculate_caret(view->mark, view->buffer, view->lines);

    move_view_to_caret(view);

    app.animating = true;
}

void set_caret_xy(View *view, i64 x, i64 y)
{
    Buffer *buffer = get_buffer(view->buffer);
    if (!buffer) return;

    ASSERT(!view->lines_dirty);
    i64 wrapped_line = view->line_offset + (y - view->text_rect.tl.y + view->voffset) / app.mono.line_height;
    wrapped_line = CLAMP(wrapped_line, 0, view->lines.count-1);

    while (view->caret.wrapped_line > wrapped_line) {
        if (!view->lines[view->caret.wrapped_line].wrapped) view->caret.line--;
        view->caret.wrapped_line--;
    }

    while (view->caret.wrapped_line < wrapped_line) {
        if (!view->lines[view->caret.wrapped_line].wrapped) view->caret.line++;
        view->caret.wrapped_line++;
    }

    i64 wrapped_column = (x - view->text_rect.tl.x) / app.mono.space_width;
    view->caret.wrapped_column = calc_wrapped_column(view->caret.wrapped_line, wrapped_column, view->lines, buffer);
    view->caret.preferred_column = view->caret.wrapped_column;
    view->caret.column = calc_unwrapped_column(view->caret.wrapped_line, view->caret.wrapped_column, view->lines, buffer);
    view->caret.byte_offset = calc_byte_offset(view->caret.wrapped_column, view->caret.wrapped_line, view->lines, buffer);
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

void app_gather_input(AppWindow *wnd) INTERNAL
{
    SArena scratch = tl_scratch_arena();

    input_begin_frame();
    if (!app_needs_render()) wait_for_next_event(wnd);

    for (WindowEvent event; next_event(wnd, &event);) {
        gui_input(event);


        View *view = app.current_view;

        switch (event.type) {
        case WE_QUIT:
            exit(0);
            break;

        case WE_RESIZE:
            if (app_change_resolution({ (f32)event.resize.width, (f32)event.resize.height })) {
                glViewport(0, 0, event.resize.width, event.resize.height);

                // TODO(jesper): I don't really understand why I can't just set this to true
                // and let the main loop handle it. It appears as if we get stuck in WM_SIZE message
                // loop until you stop resizing
                //app.animating = true;

                // TODO(jesper): this feels super iffy. I kind of want some arena push/restore
                // points or something that can be used for local reset of the allocators
                RESET_ALLOC(mem_frame);

                update_and_render();
                present_window(wnd);
            }
            break;

        case INSERT_MODE:
            app.next_mode = MODE_INSERT;
            app.current_view->mark = app.current_view->caret;
            break;

        case EDIT_MODE:
            app.next_mode = MODE_EDIT;
            break;

        case SAVE:
            buffer_save(app.current_view->buffer);
            break;

        case FUZZY_FIND_FILE:
            app.lister.active = true;
            app.lister.selected_item = 0;

            for (String s : app.lister.values) FREE(mem_dynamic, s.data);
            app.lister.values.count = 0;

            list_files(&app.lister.values, "./", mem_dynamic, FILE_LIST_RECURSIVE);
            array_copy(&app.lister.filtered, app.lister.values);
            break;

        case WE_KEY_PRESS:
            if (gui.focused != view->gui_id) break;
            app.animating = true;
            if (app.mode == MODE_EDIT) {
                switch (event.key.keycode) {
                case KC_SLASH:
                    app.incremental_search.start_caret = view->caret.byte_offset;
                    app.incremental_search.start_mark = view->mark.byte_offset;
                    app.incremental_search.active = true;
                    app.incremental_search.set_mark = event.key.modifiers != MF_SHIFT;
                    break;

                case KC_GRAVE:
                    // TODO(jesper): make a scope matching query for when tree sitter languages are available. This is the
                    // textual fall-back version which doesn't handle comments or strings
                    if (auto buffer = get_buffer(view->buffer); buffer) {
                        i64 f = buffer_seek_first_forward(view->buffer, {'{','}', '[',']', '(',')'}, view->caret.byte_offset-1);
                        i64 b = buffer_seek_first_back(view->buffer, {'{','}', '[',']', '(',')'}, view->caret.byte_offset+1);

                        if (f != -1 && f != view->caret.byte_offset &&
                            (b < line_start_offset(view->caret.wrapped_line, view->lines) ||
                             b > line_end_offset(view->caret.wrapped_line, view->lines, view->buffer)))
                        {
                            view->caret.byte_offset = f;
                            view->caret = recalculate_caret(view->caret, view->buffer, view->lines);
                            if (event.key.modifiers != MF_SHIFT) view->mark = view->caret;
                            break;
                        }

                        i64 offset = b == -1 || f-view->caret.byte_offset < view->caret.byte_offset-b ? f : b;
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
                                buffer_seek_first_forward(view->buffer, {l, r}, offset) :
                                buffer_seek_first_back(view->buffer, {l, r}, offset);

                            char c = char_at(buffer, offset);
                            if (c == l) level++;
                            else if (c == r) level--;
                        }

                        if (offset >= 0) {
                            view->caret.byte_offset = offset;
                            view->caret = recalculate_caret(view->caret, view->buffer, view->lines);
                            if (event.key.modifiers != MF_SHIFT) view->mark = view->caret;
                        }
                    } break;

                case KC_ESC:
                    FREE(mem_dynamic, app.incremental_search.str.data);
                    app.incremental_search.str = {};
                    break;

                case KC_F5: exec_process_command(); break;

                case KC_Q:
                    if (event.key.modifiers != MF_SHIFT) view->mark = view->caret;
                    ASSERT(!view->lines_dirty);
                    view_seek_byte_offset(view, buffer_seek_beginning_of_line(view->buffer, view->lines, view->caret.wrapped_line));
                    break;

                case KC_E:
                    if (event.key.modifiers != MF_SHIFT) view->mark = view->caret;
                    ASSERT(!view->lines_dirty);
                    view_seek_byte_offset(view, buffer_seek_end_of_line(view->buffer, view->lines, view->caret.wrapped_line));
                    break;

                case KC_RBRACKET:
                    if (event.key.modifiers != MF_SHIFT) view->mark = view->caret;
                    ASSERT(!view->lines_dirty);
                    view_seek_line(view, buffer_seek_next_empty_line(view->buffer, view->lines, view->caret.wrapped_line));
                    break;

                case KC_LBRACKET:
                    if (event.key.modifiers != MF_SHIFT) view->mark = view->caret;
                    ASSERT(!view->lines_dirty);
                    view_seek_line(view, buffer_seek_prev_empty_line(view->buffer, view->lines, view->caret.wrapped_line));
                    break;

                case KC_M:
                    if (event.key.modifiers == MF_CTRL) SWAP(view->mark, view->caret);
                    else view->mark = view->caret;
                    break;

                case KC_N:
                    if (app.incremental_search.str.length > 0) {
                        i64 offset = event.key.modifiers == MF_CTRL ?
                            buffer_seek_back(view->buffer, app.incremental_search.str, view->caret.byte_offset) :
                            buffer_seek_forward(view->buffer, app.incremental_search.str, view->caret.byte_offset);

                        if (offset != view->caret.byte_offset) {
                            view->caret.byte_offset = offset;
                            view->caret = recalculate_caret(view->caret, view->buffer, view->lines);
                            if (app.incremental_search.set_mark) view->mark = view->caret;

                            move_view_to_caret(view);
                        }
                    } break;

                case KC_W:
                    if (event.key.modifiers != MF_SHIFT) view->mark = view->caret;
                    view_seek_byte_offset(view, buffer_seek_next_word(view->buffer, view->caret.byte_offset));
                    break;

                case KC_B:
                    if (event.key.modifiers != MF_SHIFT) view->mark = view->caret;
                    view_seek_byte_offset(view, buffer_seek_prev_word(view->buffer, view->caret.byte_offset));
                    break;

                case KC_J:
                    if (event.key.modifiers != MF_SHIFT) view->mark = view->caret;
                    view_seek_line(view, view->caret.wrapped_line+1);
                    break;

                case KC_K:
                    if (event.key.modifiers != MF_SHIFT) view->mark = view->caret;
                    view_seek_line(view, view->caret.wrapped_line-1);
                    break;

                case KC_L:
                    if (event.key.modifiers != MF_SHIFT) view->mark = view->caret;
                    view->caret.byte_offset = buffer_next_offset(view->buffer, view->caret.byte_offset);
                    ASSERT(!view->lines_dirty);
                    view->caret = recalculate_caret(view->caret, view->buffer, view->lines);
                    move_view_to_caret(view);
                    break;

                case KC_H:
                    if (event.key.modifiers != MF_SHIFT) view->mark = view->caret;
                    view->caret.byte_offset = buffer_prev_offset(view->buffer, view->caret.byte_offset);
                    ASSERT(!view->lines_dirty);
                    view->caret = recalculate_caret(view->caret, view->buffer, view->lines);
                    move_view_to_caret(view);
                    break;

                case KC_TAB:
                    if (auto buffer = get_buffer(view->buffer); buffer) {
                        i32 line = calc_unwrapped_line(view->caret.wrapped_line, view->lines);
                        Range_i32 r{ line, line };
                        if (event.key.modifiers & MF_CTRL) {
                            r = range_i32(line, calc_unwrapped_line(view->mark.wrapped_line, view->lines));
                        }

                        bool recalc_caret = false, recalc_mark = false;
                        defer {
                            if (recalc_caret) {
                                view->caret = recalculate_caret(view->caret, view->buffer, view->lines);
                                move_view_to_caret(view);
                            }

                            if (recalc_mark) {
                                view->mark = recalculate_caret(view->mark, view->buffer, view->lines);
                            }
                        };

                        BufferHistoryScope h(view->buffer);

                        if (event.key.modifiers & MF_SHIFT) {
                            struct Edit { i64 offset; i32 bytes_to_remove; };
                            DynamicArray<Edit> edits{ .alloc = scratch };
                            for (i32 l = r.start; l <= r.end; l = next_unwrapped_line(l, view->lines)) {
                                i64 line_start = line_start_offset(l, view->lines);

                                String current_indent = get_indent_for_line(view->buffer, view->lines, l, scratch);

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

                                if (buffer_remove(view->buffer, edit.offset, edit.offset+edit.bytes_to_remove)) {
                                    if (view->caret.byte_offset >= edit.offset) {
                                        view->caret.byte_offset -= edit.bytes_to_remove;
                                        recalc_caret = true;
                                    }

                                    if (view->mark.byte_offset > edit.offset) {
                                        view->mark.byte_offset -= edit.bytes_to_remove;
                                        recalc_mark = true;
                                    }
                                }
                            }

                        } else {
                            for (i32 l = r.end; l >= r.start; l = prev_unwrapped_line(l, view->lines)) {
                                i64 line_start = line_start_offset(l, view->lines);

                                i64 new_offset;
                                if (buffer->indent_with_tabs) new_offset = buffer_insert(view->buffer, line_start, "\t");
                                else {
                                    String current_indent = get_indent_for_line(view->buffer, view->lines, l, scratch);

                                    i32 vcol = 0;
                                    for (i32 i = 0; i < current_indent.length; i++) {
                                        if (current_indent[i] == '\t') vcol += buffer->tab_width - vcol % buffer->tab_width;
                                        else vcol += 1;
                                    }

                                    i32 w = buffer->tab_width - vcol % buffer->tab_width;
                                    char *spaces = ALLOC_ARR(*scratch, char, w);
                                    memset(spaces, ' ', w);
                                    new_offset = buffer_insert(view->buffer, line_start, { spaces, w });
                                }

                                if (view->caret.byte_offset >= line_start) {
                                    view->caret.byte_offset += new_offset - line_start;
                                    recalc_caret = true;
                                }

                                if (view->mark.byte_offset > line_start) {
                                    view->mark.byte_offset += new_offset - line_start;
                                    recalc_mark = true;
                                }
                            }
                        }
                    } break;
                default: break;

                }
            } else if (app.mode == MODE_INSERT) {
                switch (event.key.keycode) {
                case KC_LEFT:
                    view->caret.byte_offset = buffer_prev_offset(view->buffer, view->caret.byte_offset);
                    ASSERT(!view->lines_dirty);
                    view->caret = recalculate_caret(view->caret, view->buffer, view->lines);
                    move_view_to_caret(view);
                    break;

                case KC_RIGHT:
                    view->caret.byte_offset = buffer_next_offset(view->buffer, view->caret.byte_offset);
                    ASSERT(!view->lines_dirty);
                    view->caret = recalculate_caret(view->caret, view->buffer, view->lines);
                    move_view_to_caret(view);
                    break;

                case KC_ENTER:
                    if (buffer_valid(view->buffer)) {
                        BufferHistoryScope h(view->buffer);
                        ASSERT(!view->lines_dirty);
                        String indent = get_indent_for_line(view->buffer, view->lines, view->caret.wrapped_line, scratch);
                        write_string(view, view->buffer, buffer_newline_str(view->buffer));
                        write_string(view, view->buffer, indent);
                    } break;

                case KC_TAB:
                    if (Buffer *buffer = get_buffer(view->buffer); buffer) {
                        if (buffer->indent_with_tabs) write_string(view, view->buffer, "\t");
                        else {
                            String current_indent = get_indent_for_line(view->buffer, view->lines, view->caret.wrapped_line, scratch);

                            i32 vcol = 0;
                            for (i32 i = 0; i < current_indent.length; i++) {
                                if (current_indent[i] == '\t') vcol += buffer->tab_width - vcol % buffer->tab_width;
                                else vcol += 1;
                            }


                            i32 w = buffer->tab_width - vcol % buffer->tab_width;
                            char *spaces = ALLOC_ARR(*scratch, char, w);
                            memset(spaces, ' ', w);
                            write_string(view, view->buffer, { spaces, w });
                        }

                    } break;

                case KC_BACKSPACE:
                    if (buffer_valid(view->buffer)) {
                        BufferHistoryScope h(view->buffer);
                        i64 start = buffer_prev_offset(view->buffer, view->caret.byte_offset);
                        if (buffer_remove(view->buffer, start, view->caret.byte_offset)) {
                            view->caret.byte_offset = start;
                            view->caret = recalculate_caret(view->caret, view->buffer, view->lines);
                            move_view_to_caret(view);
                        }
                    } break;

                case KC_DELETE:
                    if (buffer_valid(view->buffer)) {
                        BufferHistoryScope h(view->buffer);
                        i64 end = buffer_next_offset(view->buffer, view->caret.byte_offset);
                        if (buffer_remove(view->buffer, view->caret.byte_offset, end)) {
                            view->caret = recalculate_caret(view->caret, view->buffer, view->lines);
                            move_view_to_caret(view);
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
                    if (event.text.modifiers != MF_SHIFT) view->mark = view->caret;
                    view_seek_line(view, view->caret.wrapped_line+view->lines_visible-1);
                    break;
                case KC_PAGE_UP:
                    // TODO(jesper: page up/down should take current caret line into account when moving
                    // view to caret, so that it remains where it was proportionally
                    if (event.text.modifiers != MF_SHIFT) view->mark = view->caret;
                    view_seek_line(view, view->caret.wrapped_line-view->lines_visible-1);
                    break;
                default: break;
                }
            }
            break;

        case WE_MOUSE_MOVE:
            if (gui.hot == view->gui_id &&
                event.mouse.button == MB_PRIMARY &&
                point_in_rect({ (f32)event.mouse.x, (f32)event.mouse.y }, view->rect))
            {
                set_caret_xy(view, event.mouse.x, event.mouse.y);
                app.animating = true;
            } break;

        case WE_MOUSE_PRESS:
            if (gui.hot == view->gui_id &&
                event.mouse.button == MB_PRIMARY &&
                point_in_rect({ (f32)event.mouse.x, (f32)event.mouse.y }, view->rect))
            {
                set_caret_xy(view, event.mouse.x, event.mouse.y);
                view->mark = view->caret;
                app.animating = true;
            } break;

        case WE_MOUSE_WHEEL:
            app.animating = true;
            break;
        }
    }

    View *view = app.current_view;
    if (get_input_edge(UNDO, app.input.edit)) buffer_undo(view->buffer);
    if (get_input_edge(REDO, app.input.edit)) buffer_redo(view->buffer);

    if (get_input_edge(PASTE, app.input.edit)) {
        write_string(view, view->buffer, read_clipboard_str(scratch), VIEW_SET_MARK);
    }

    for (TextEvent text; get_input_text(TEXT_INPUT, &text, app.input.insert);) {
        write_string(view, view->buffer, String{ (char*)&text.c[0], text.length });
        app.animating = true;
    }

    if (get_input_edge(COPY_RANGE, app.input.edit)) {
        ASSERT(!view->lines_dirty);
        Range_i64 r = caret_range(view->caret, view->mark, view->lines, view->buffer, false);
        set_clipboard_data(buffer_read(view->buffer, r.start, r.end, scratch));
    }

    if (get_input_edge(DELETE_RANGE, app.input.edit)) {
        ASSERT(!view->lines_dirty);
        Range_i64 r = caret_range(view->caret, view->mark, view->lines, view->buffer, false);

        if (r.end != r.start) {
            BufferHistoryScope h(view->buffer);
            if (buffer_remove(view->buffer, r.start, r.end)) {
                view->caret = view->caret.byte_offset > view->mark.byte_offset ? view->mark : view->caret;
                view->caret = recalculate_caret(view->caret, view->buffer, view->lines);
                view->mark = view->caret;
                move_view_to_caret(view);
            }
        }
    }

    if (get_input_edge(CUT_RANGE, app.input.edit)) {
        BufferHistoryScope h(view->buffer);

        ASSERT(!view->lines_dirty);
        Range_i64 r = caret_range(view->caret, view->mark, view->lines, view->buffer, false);
        String str = buffer_read(view->buffer, r.start, r.end, scratch);
        set_clipboard_data(str);

        if (buffer_remove(view->buffer, r.start, r.end)) {
            view->caret = view->caret.byte_offset > view->mark.byte_offset ? view->mark : view->caret;
            view->caret = recalculate_caret(view->caret, view->buffer, view->lines);
            view->mark = view->caret;
            move_view_to_caret(view);
        }
    }
}

void update_and_render() INTERNAL
{
    SArena scratch = tl_scratch_arena();

    //LOG_INFO("------- frame --------");
    //defer { LOG_INFO("-------- frame end -------\n\n"); };

    // NOTE(jesper): this is something of a hack because WM_CHAR messages come after the WM_KEYDOWN, and
    // we're listening to WM_KEYDOWN to determine whether to switch modes, so the actual mode switch has to
    // be deferred.
    if (app.mode != app.next_mode) {
        if (app.next_mode == MODE_INSERT) {
            buffer_history(app.current_view->buffer, { .type = BUFFER_GROUP_START });
        } else if (app.mode == MODE_INSERT) {
            buffer_history(app.current_view->buffer, { .type = BUFFER_GROUP_END });
            if (app.current_view->mark.byte_offset > app.current_view->caret.byte_offset) app.current_view->mark = app.current_view->caret;
        }

        app.mode = app.next_mode;
    }

    gfx_begin_frame();
    gui_begin_frame();

    GfxCommandBuffer debug_gfx = gfx_command_buffer();

    gui_menu() {
        gui_menu("file") {
            // TODO(jesper): save all?
            if (gui_button("save")) buffer_save(app.current_view->buffer);

            if (gui_button("select working dir...")) {
                String wd = select_folder_dialog(scratch);
                if (wd.length > 0) set_working_dir(wd);
            }
        }

        gui_menu("debug") {
            if (gui_button("history")) gui_window_toggle(debug.buffer_history.wnd);

        }

        if (app.process_commands.count > 0) {
            app.selected_process = gui_dropdown(app.process_command_names, app.selected_process);
            if (gui_icon_button("󰣪")) exec_process_command();
        }
    }

    gui_window_id(debug.buffer_history.wnd) {
        if (auto buffer = get_buffer(app.current_view->buffer); buffer) {
            i32 indent = 0;
            for (auto it : iterator(buffer->history)) {
                SArena scartch = tl_scratch_arena();

                Rect r = split_row({ gui.fonts.base.line_height*1.2f });
                if (it.index == buffer->history_index) gui_draw_rect(r, gui.style.accent_bg);

                switch (it->type) {
                case BUFFER_INSERT:
                    gui_textbox(stringf(scratch, "%*sINSERT (%lld): '%.*s'", indent*2, "", it->offset, STRFMT(it->text)), r);
                    break;
                case BUFFER_REMOVE:
                    gui_textbox(stringf(scratch, "%*sREMOVE (%lld): '%.*s'", indent*2, "", it->offset, STRFMT(it->text)), r);
                    break;
                case BUFFER_CURSOR:
                    gui_textbox(stringf(scratch, "%*sCURSOR: [%lld, %lld]", indent*2, "", it->caret, it->mark), r);
                    break;
                case BUFFER_GROUP_START:
                    gui_textbox(stringf(scratch, "%*sGROUP_START", indent*2, ""), r);
                    indent++;
                    break;
                case BUFFER_GROUP_END:
                    indent--;
                    gui_textbox(stringf(scratch, "%*sGROUP_END", indent*2, ""), r);
                    break;
                }
            }
        }
    }

    f32 lister_w = gfx.resolution.x*0.7f;
    lister_w = MAX(lister_w, 500.0f);
    lister_w = MIN(lister_w, gfx.resolution.x-10);

    Vector2 lister_p = gfx.resolution*0.5f;

    gui_window({ "open file", lister_p, { lister_w, 200.0f }, .anchor = { 0.5f, 0.5f } }, &app.lister.active) {
        GuiId id = GUI_ID;

        GuiAction edit_action = gui_editbox_id(id, "");
        if (edit_action & (GUI_CHANGE)) {
            app.lister.selected_item = 0;
            String needle{ gui.edit.buffer, gui.edit.length };
            if (needle.length == 0) {
                array_copy(&app.lister.filtered, app.lister.values);
            } else {
                String lneedle = to_lower(needle, scratch);

                DynamicArray<fzy_score_t> scores{ .alloc = scratch };
                array_reserve(&scores, app.lister.values.count);

                app.lister.filtered.count = 0;

                DynamicArray<fzy_score_t> D{ .alloc = scratch };
                DynamicArray<fzy_score_t> M{ .alloc = scratch };
                DynamicArray<fzy_score_t> match_bonus{ .alloc = scratch };

                i32 lower_buffer_size = 100;
                char *lower_buffer = ALLOC_ARR(*scratch, char, lower_buffer_size);

                for (String s : app.lister.values) {
                    fzy_score_t match_score;

                    array_resize(&D, needle.length*s.length);
                    array_resize(&M, needle.length*s.length);

                    memset(D.data, 0, D.count*sizeof *D.data);
                    memset(M.data, 0, M.count*sizeof *M.data);

                    array_resize(&match_bonus, s.length);

                    if (s.length >= lower_buffer_size) {
                        lower_buffer = REALLOC_ARR(*scratch, char, lower_buffer, lower_buffer_size, s.length);
                        lower_buffer_size = s.length;
                    }
                    memcpy(lower_buffer, s.data, s.length);

                    String ls{ lower_buffer, s.length };
                    ls = to_lower(ls, scratch);

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
                quick_sort_desc(scores, app.lister.filtered);
            }
        }

        if (edit_action == GUI_END &&
            (app.lister.selected_item < 0 || app.lister.selected_item >= app.lister.filtered.count))
        {
            gui_focus(id);
        }

        GuiAction lister_action = gui_lister_id(id, app.lister.filtered, &app.lister.selected_item);
        if (lister_action == GUI_END || edit_action == GUI_END) {
            if (app.lister.selected_item >= 0 && app.lister.selected_item < app.lister.filtered.count) {
                String file = app.lister.filtered[app.lister.selected_item];
                String path = absolute_path(file, scratch);

                BufferId buffer = find_buffer(path);
                if (!buffer) buffer = create_buffer(path);

                view_set_buffer(app.current_view, buffer);

                app.lister.active = false;
                gui_focus(GUI_ID_INVALID);
            }
        } else if (lister_action == GUI_CANCEL) {
            // TODO(jesper): this results in 1 frame of empty lister window being shown because
            // we don't really have a good way to close windows from within the window
            app.lister.active = false;
            gui_focus(GUI_ID_INVALID);
        }

        if (!app.lister.active) {
            for (String s : app.lister.values) FREE(mem_dynamic, s.data);
            app.lister.values.count = 0;
        }
    }

    DynamicArray<RangeColor> colors{ .alloc = scratch };
    for (View &view : app.views) {
        if (view.id == -1) continue;

        gui_push_id(view.gui_id);
        defer { gui_pop_id(); };

        // TODO(jesper): put split direction/ratio/stuff in view
        view.rect = split_rect({});
        view.caret_dirty |= view.lines_dirty;
        calculate_num_visible_lines(&view);

        gui_handle_focus_grabbing(view.gui_id);
        if (gui_clicked(view.gui_id, view.rect)) gui_focus(view.gui_id);

        switch (app.mode) {
        case MODE_EDIT: gui_input_layer(view.gui_id, app.input.edit); break;
        case MODE_INSERT: gui_input_layer(view.gui_id, app.input.insert); break;
        }

        gui_push_layout({ view.rect });
        defer { gui_pop_layout(); };

        BufferId active_buffer = view.buffer;

        // TODO(jesper): kind of only want to do the tabs if there are more than 1 file open, but then I'll
        // need to figure out reasonable ways of visualising unsaved changes as well as which file is open.
        // Likely do-able with the window titlebar decoration on most/all OS?
        if (view.buffers.count > 0) {
            LayoutRect row = split_top({ gui.fonts.base.line_height+2 });
            gui_draw_rect(row, gui.style.bg_dark1);

            GUI_LAYOUT(row) {
                FontAtlas *font = &gui.fonts.base;
                i32 current_tab = 0;

                //i32 cutoff = -1;
                for (i32 i = 0; i < view.buffers.count; i++) {
                    Buffer *b = get_buffer(view.buffers[i]);
                    GuiId id = gui_gen_id(i);

                    GlyphsData td = calc_glyphs_data(b->name, font, scratch);
                    Rect rect = split_rect({ td.bounds.x+6, td.bounds.y+16, .margin = gui.style.margin });

                    // if (gui_current_layout()->available_space.x < size.x) {
                    //     cutoff = i;
                    //     break;
                    // }

                    if (gui_clicked(id, rect)) active_buffer = b->id;

                    if (i == current_tab) {
                        gui_draw_accent_button(id, rect);
                        gui_draw_text(td, calc_center(rect.tl, rect.br, { td.bounds.x, td.font->line_height }), gui.style.fg);
                    } else {
                        gui_draw_button(id, rect);
                        gui_draw_text(td, calc_center(rect.tl, rect.br, { td.bounds.x, td.font->line_height }), gui.style.fg);
                    }

                    // if (b->saved_at != b->history_index) {
                    //     gui_draw_rect(
                    //         rect.tl + Vector2{ rect.size().x-4-1, 1 },
                    //         { 4, 4 },
                    //         bgr_unpack(0xFF990000));
                    // }
                }

                // if (cutoff >= 0) {
                //     // TODO(jesper): this is essentially a dropdown with an icon button as the trigger button
                //     // instead of a label button
                //
                //     GuiId mid = GUI_ID(0);
                //     gui_hot_rect(mid, mpos, msize);
                //     if (gui_pressed(mid)) {
                //         if (gui.focused == mid) {
                //             gui_focus(GUI_ID_INVALID);
                //         } else {
                //             gui_focus(mid);
                //         }
                //     }
                //
                //     Vector3 mbg = gui.hot == mid ? gui.style.accent_bg_hot : gui.style.accent_bg;
                //     gui_draw_rect(mpos, msize, wnd->clip_rect, mbg);
                //
                //     Vector2 icon_s{ 16, 16 };
                //     Vector2 icon_p = mpos + 0.5f*(msize - icon_s);
                //     gui_draw_rect(icon_p, icon_s, gui.icons.down);
                //
                //     if (gui.focused == mid) {
                //         i32 old_window = gui.current_window;
                //         gui.current_window = GUI_OVERLAY;
                //         defer { gui.current_window = old_window; };
                //
                //         GuiWindow *overlay = &gui.windows[GUI_OVERLAY];
                //
                //         Vector3 border_col = bgr_unpack(0xFFCCCCCC);
                //         Vector3 bg = bgr_unpack(0xFF1d2021);
                //         Vector3 bg_hot = bgr_unpack(0xFF3A3A3A);
                //
                //         i32 bg_index = overlay->command_buffer.commands.count;
                //
                //         // TODO(jesper): figure out a reasonable way to make this layout anchored to the right and expand to the
                //         // left with the subsequent labels. Right now I'm just hoping that the width is wide enough
                //         Vector2 s{ 200.0f, 0 };
                //         Vector2 p{ mpos.x-s.x+msize.x, mpos.y + msize.y };
                //         Vector2 border{ 1, 1 };
                //
                //         gui_begin_layout({ .type = GUI_LAYOUT_ROW, .flags = GUI_LAYOUT_EXPAND_Y, .pos = p, .size = s, .margin = { 1, 1 } });
                //         defer { gui_end_layout(); };
                //
                //         for (i32 i = cutoff; i < view.buffers.count; i++) {
                //             Buffer *b = get_buffer(view.buffers[i]);
                //
                //             Vector2 rect_s{ 0, font->line_height };
                //             Vector2 rect_p = gui_layout_widget(&rect_s);
                //
                //             GuiId lid = GUI_ID_INDEX(parent, i);
                //             if (gui.hot == lid) gui_draw_rect(rect_p, rect_s, bg_hot);
                //             gui_textbox(b->name, rect_p);
                //
                //             // TODO(jesper): due to changes in gui_hot to not allow a new hot widget
                //             // if one is pressed, this doesn't behave the way I want for dropdowns where
                //             // I can press the trigger button, drag to an item, then release to select it
                //             gui_hot_rect(lid, rect_p, rect_s);
                //             if ((gui.hot == lid && gui.pressed == mid && !gui.mouse.left_pressed) ||
                //                 gui_clicked(lid, rect_p, rect_s))
                //             {
                //                 active_buffer = b->id;
                //                 gui_focus(GUI_ID_INVALID);
                //             }
                //         }
                //
                //         GuiLayout *cl = gui_current_layout();
                //         gui_draw_rect(cl->pos, cl->size, overlay->clip_rect, border_col, &overlay->command_buffer, bg_index);
                //         gui_draw_rect(cl->pos + border, cl->size - 2*border, overlay->clip_rect, bg, &overlay->command_buffer, bg_index+1);
                //
                //     }
                // }
            }

            // TODO(jesper): why is this herer
            view_set_buffer(&view, active_buffer);
        }

        if (auto buffer = get_buffer(view.buffer); buffer) {
            if (app.incremental_search.active) {
                // Vector2 size{ 0, 25.0f };
                // Vector2 pos = gui_layout_widget(&size, GUI_ANCHOR_TOP);;
                // gui_begin_layout({ .type = GUI_LAYOUT_ROW, .pos = pos, .size = size });
                // defer { gui_end_layout(); };
                //
                // auto action = gui_editbox("");
                // if (action & GUI_EDITBOX_CHANGE) {
                //     String needle{ gui.edit.buffer, gui.edit.length };
                //     string_copy(&app.incremental_search.str, needle, mem_dynamic);
                //
                //     view.caret.byte_offset = buffer_seek_forward(view.buffer, needle, app.incremental_search.start_caret);
                //     view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                //     if (app.incremental_search.set_mark) view.mark = view.caret;
                //     move_view_to_caret(&view);
                // }
                //
                // if (action & GUI_EDITBOX_FINISH) {
                //     app.incremental_search.active = false;
                // }
                //
                // if (action & GUI_EDITBOX_CANCEL) {
                //     FREE(mem_dynamic, app.incremental_search.str.data);
                //     app.incremental_search.str = {};
                //
                //     view.caret.byte_offset = app.incremental_search.start_caret;
                //     view.caret = recalculate_caret(view.caret, view.buffer, view.lines);
                //     if (app.incremental_search.set_mark) {
                //         view.mark.byte_offset = app.incremental_search.start_mark;
                //         view.mark = recalculate_caret(view.mark, view.buffer, view.lines);
                //     }
                //     app.incremental_search.active = false;
                // }
            } else if (app.incremental_search.str.length > 0) {
            }

            GUI_LAYOUT(split_bottom({ 15 })) {

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
            // NOTE(jesper): the scrollbar is 1 frame delayed if the reflowing results in a different number of lines,
            // which we are doing afterwards to allow the gui layout system trigger line reflowing
            // We could move it to after the reflow handling with some more layout plumbing, by letter caller deal with
            // the layouting and passing in absolute positions and sizes to the scrollbar
            gui_scrollbar(
                &view.voffset,
                &view.line_offset,
                app.mono.line_height,
                view.lines.count,
                view.lines_visible);

            if (view.lines_dirty) {
                recalc_line_wrap(&view, &view.lines, 0, view.lines.count, view.buffer);
                view.lines_dirty = false;
            }
        }

        view.text_rect = *gui_current_layout();
        gui_hot_rect(view.gui_id, view.text_rect);
        if (gui_clicked(view.gui_id, view.text_rect)) gui_focus(view.gui_id);

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


        // draw caret
        if (view.caret.wrapped_line >= view.line_offset &&
            view.caret.wrapped_line < view.line_offset + view.lines_visible)
        {
            Rect rect = view.text_rect;
            i32 y = view.caret.wrapped_line - view.line_offset;
            f32 w = app.mono.space_width;
            f32 h = app.mono.line_height+1;

            Vector2 p0{
                rect.tl.x + view.caret.wrapped_column*app.mono.space_width,
                rect.tl.y + y*app.mono.line_height - view.voffset,
            };
            Vector2 p1{ p0.x, p0.y + h };

            gui_draw_rect({ rect.tl.x, p0.y }, { rect.size().x, h }, app.line_bg, &gfx.frame_cmdbuf);

            gui_draw_rect(p0, { w, h }, app.caret_bg, &gfx.frame_cmdbuf);
            gui_draw_rect(p0, { 1.0f, h }, app.caret_fg, &gfx.frame_cmdbuf);
            gui_draw_rect(p0, { w, 1.0f }, app.caret_fg, &gfx.frame_cmdbuf);
            gui_draw_rect(p1, { w, 1.0f }, app.caret_fg, &gfx.frame_cmdbuf);
        }

        // draw caret anchor
        if (view.mark.wrapped_line >= view.line_offset &&
            view.mark.wrapped_line < view.line_offset + view.lines_visible)
        {
            Rect rect = view.text_rect;
            i32 y = view.mark.wrapped_line - view.line_offset;
            f32 w = app.mono.space_width/2;
            f32 h = app.mono.line_height - 1.0f;

            Vector2 p0{
                rect.tl.x + view.mark.wrapped_column*app.mono.space_width,
                rect.tl.y + y*app.mono.line_height - view.voffset + 1,
            };
            Vector2 p1{ p0.x, p0.y + h };

            gui_draw_rect(p0, { 1.0f, h }, app.mark_fg, &gfx.frame_cmdbuf);
            gui_draw_rect(p0, { w, 1.0f }, app.mark_fg, &gfx.frame_cmdbuf);
            gui_draw_rect(p1, { w, 1.0f }, app.mark_fg, &gfx.frame_cmdbuf);
        }

        if (Buffer *buffer = get_buffer(view.buffer); buffer) {
            FontAtlas *font = &app.mono;

            i32 columns = (i32)ceilf(view.rect.size().x / font->space_width);
            i32 rows = view.lines_visible;

            i64 byte_start = line_start_offset(view.line_offset, view.lines);
            i64 byte_end = line_end_offset(view.line_offset+rows, view.lines, view.buffer);

            if (DEBUG_TREE_SITTER_COLORS) LOG_INFO("-- highlight query start --");
            colors.count = 0;

#if DEBUG_TREE_SITTER_COLORS
            String l = string_from_enum(buffer->language);
            LOG_INFO("highlight colors for language '%.*s'", STRFMT(l));
#endif
            get_syntax_colors(&colors, byte_start, byte_end, buffer->syntax_tree, buffer->language);

            for (auto st : buffer->subtrees) {
#if DEBUG_TREE_SITTER_COLORS
                String l = string_from_enum(st.language);
                LOG_INFO("highlight colors for language '%.*s'", STRFMT(l));
#endif
                get_syntax_colors(&colors, byte_start, byte_end, st.tree, st.language);
            }

            if (DEBUG_TREE_SITTER_COLORS) for (auto c : colors) LOG_INFO("color range [%d, %d]", c.start, c.end);

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
            glyphs = { .data = (decltype(glyphs.data))mapped, .count = columns*rows};
            defer { glUnmapNamedBuffer(view.glyph_data_ssbo); };

            Vector3 fg = linear_from_sRGB(app.fg);

            for (auto &g : glyphs) {
                g.glyph_index = 0xFFFFFFFF;
                g.fg = bgr_pack(fg);
            }

            i32 current_color = 0;
            for (i32 line_index = view.line_offset;
                 line_index < MIN(view.lines.count, view.line_offset+rows);
                 line_index++)
            {
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


            Rect rect = *gui_current_layout();

            GfxCommand cmd{
                .type = GFX_COMMAND_MONO_TEXT,
                .mono_text = {
                    .vbo = gfx.vbos.frame,
                    .vbo_offset = gfx.frame_vertices.count,
                    .glyph_ssbo = view.glyph_data_ssbo,
                    .glyph_atlas = font->texture,
                    .cell_size = { app.mono.space_width, app.mono.line_height },
                    .pos = rect.tl,
                    .offset = view.voffset,
                    .line_offset = view.line_offset,
                    .columns = columns,
                }
            };

            array_add(
                &gfx.frame_vertices,
                {
                    rect.br.x, rect.tl.y,
                    rect.tl.x, rect.tl.y,
                    rect.tl.x, rect.br.y,
                    rect.tl.x, rect.br.y,
                    rect.br.x, rect.br.y,
                    rect.br.x, rect.tl.y,
                });

            gfx_push_command(cmd, &gfx.frame_cmdbuf);
        }
    }

    gui_end_frame();

    Vector3 clear_color = linear_from_sRGB(app.bg);
    glClearColor(clear_color.r, clear_color.g, clear_color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    app.animating = text_input_enabled();

    Matrix3 view = orthographic3(0, gfx.resolution.x, gfx.resolution.y, 0, 1);

    gfx_flush_transfers();
    gfx_submit_commands(gfx.frame_cmdbuf, view);
    gfx_submit_commands(debug_gfx, view);

    gui_render();
}
