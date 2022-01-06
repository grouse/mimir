#include "input.h"

#include "maths.cpp"
#include "gui.cpp"
#include "string.cpp"
#include "allocator.cpp"
#include "core.cpp"
#include "gfx_opengl.cpp"

struct BufferLine {
    i64 offset : 63;
    i64 wrapped : 1;
};

struct Application {
    Font mono;
    //String content;
    i32 tab_width = 4;
    bool animating = true;
};

enum BufferType {
    BUFFER_FLAT,
};

struct Buffer {
    BufferType type;
    union {
        String flat;
    };
};

struct BufferId {
    i32 index = -1;
};


struct Caret {
    i64 byte_offset = 0;
    
    // TODO(jesper): do we need both wrapped and unwrapped column? Unwrapped is used any time column is displayed, wrapped seems
    // to be used for most (all?) operations.
    i64 wrapped_column = 0;
    i64 column = 0;
    i64 preferred_column = 0;

    // TODO(jesper): do we need both wrapped and unwrapped line? Unwrapped line is used any time line number is displayed, wrapped
    // one is used for most navigation operations, unclear whether I prefer unwrapped or wrapped as default choice for operations,
    // e.g. delete line.
    // NOTE(jesper): i32 because our array size, used for the array of (wrapped) lines is an i32
    i32 line = 0;
    i32 wrapped_line = 0;
    
};

struct View {
    f32 voffset = 0;
    i32 line_offset = 0;
    i32 lines_visible = 10;
    DynamicArray<BufferLine> lines;
    Rect rect;
    
    BufferId buffer;
    
    Caret caret;
    
    struct {
        u64 lines_dirty : 1 = true;
        u64 caret_dirty : 1 = true;
    };

};

Application app{};
View view{};
DynamicArray<Buffer> buffers;

char char_at(Buffer *b, i64 i)
{
    switch (b->type) {
    case BUFFER_FLAT: return b->flat[i];
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

BufferId create_buffer(String file)
{
    Buffer b{ .type = BUFFER_FLAT };
    
    FileInfo f = read_file("../lorem.txt", mem_dynamic);
    b.flat = String{ (char*)f.data, f.size };
    
    
    BufferId id{};
    id.index = array_add(&buffers, b);
    return id;
}

void init_app()
{
    String assets_dir{ "../assets" };
    init_gui(assets_dir);
    
    gui.style.text.color = Vector3{ 0.0f, 0.0f, 0.0f };
    app.mono = load_font(join_path(assets_dir, "fonts/Cousine/Cousine-Regular.ttf"));
    
    view.buffer = create_buffer("../lorem.txt");
    calculate_num_visible_lines();
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
    case BUFFER_FLAT: return utf8_incr(buffer->flat, i);
    }
}

i64 utf8_decr(Buffer *buffer, i64 i)
{
    switch (buffer->type) {
    case BUFFER_FLAT: return utf8_decr(buffer->flat, i);
    }
}

i32 utf32_it_next(Buffer *buffer, i64 *byte_offset)
{
    switch (buffer->type) {
    case BUFFER_FLAT: return utf32_it_next(buffer->flat, byte_offset);
    }
}

i64 buffer_end(Buffer *buffer)
{
    switch (buffer->type) { 
    case BUFFER_FLAT: return buffer->flat.length;
    }
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
    case BUFFER_FLAT: return (wrapped_line+1) < lines.count ? lines[wrapped_line+1].offset : buffer->flat.length;
    }
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

    for (i32 l = wrapped_line-1; l > 0; l--) {
        column += wrapped_column_count(l, lines, buffer);
        if (!lines[l].wrapped) break;
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
    return app.animating || view.lines_dirty || view.caret_dirty;
}

void app_event(InputEvent event)
{
    Buffer *buffer = &buffers[view.buffer.index];
    
    switch (event.type) {
    case IE_KEY_PRESS: 
        switch (event.key.code) {
        case IK_LEFT: 
            if (view.caret.byte_offset > 0) { 
                char c = char_at(buffer, prev_byte(buffer, view.caret.byte_offset));
                if (c == '\n' || c == '\r') {
                    view.caret.byte_offset = prev_byte(buffer, view.caret.byte_offset);
                    char prev_c = char_at(buffer, prev_byte(buffer, view.caret.byte_offset));
                    
                    if (view.caret.byte_offset > 0 &&
                        ((c == '\n' && prev_c == '\r') ||
                         (c == '\r' && prev_c == '\n')))
                    {
                        view.caret.byte_offset = prev_byte(buffer, view.caret.byte_offset);
                    }

                    view.caret.line--;
                    view.caret.column = column_count(view.caret.wrapped_line-1, view.lines, buffer);
                } else {
                    view.caret.byte_offset = utf8_decr(buffer, view.caret.byte_offset);
                    
                    if (char_at(buffer, view.caret.byte_offset) == '\t') {
                        i32 w = app.tab_width - view.caret.wrapped_column % app.tab_width;
                        view.caret.column -= w;
                        view.caret.wrapped_column -= w;
                    } else {
                        view.caret.column--;
                        view.caret.wrapped_column--;
                    }
                }
                
                if (view.caret.wrapped_line > 0 &&
                    view.caret.byte_offset < view.lines[view.caret.wrapped_line].offset) 
                {
                    view.caret.wrapped_line--;
                    view.caret.wrapped_column = wrapped_column_count(view.caret.wrapped_line, view.lines, buffer);
                }
                
                view.caret.preferred_column = view.caret.wrapped_column;

                LOG_INFO("caret: %lld, column: %lld, wrapped_column: %lld, line: %d, wrapped_line: %d", 
                         view.caret.byte_offset, 
                         view.caret.column+1, view.caret.wrapped_column+1,
                         view.caret.line+1, view.caret.wrapped_line+1);
                app.animating = true;
            } break;
        case IK_RIGHT: 
            if (view.caret.byte_offset < buffer_end(buffer)) {
                char c = char_at(buffer, view.caret.byte_offset);
                if (c == '\n' || c == '\r') {
                    view.caret.byte_offset = next_byte(buffer, view.caret.byte_offset);
                    char nc = char_at(buffer, view.caret.byte_offset);
                    if (view.caret.byte_offset < buffer_end(buffer) &&
                        ((c == '\n' && nc == '\r') ||
                         (c == '\r' && nc == '\n')))
                    {
                        view.caret.byte_offset = next_byte(buffer, view.caret.byte_offset);
                    }

                    view.caret.line++;
                    view.caret.column = 0;
                } else {
                    if (char_at(buffer, view.caret.byte_offset) == '\t') {
                        i32 w = app.tab_width - view.caret.wrapped_column % app.tab_width;
                        view.caret.column += w;
                        view.caret.wrapped_column += w;
                    } else {
                        view.caret.column++;
                        view.caret.wrapped_column++;
                    }
                    
                    view.caret.byte_offset = utf8_incr(buffer, view.caret.byte_offset);
                }
                
                if (view.caret.wrapped_line < view.lines.count-1 &&
                    view.caret.byte_offset >= view.lines[view.caret.wrapped_line+1].offset) 
                {
                    view.caret.wrapped_line++;
                    view.caret.wrapped_column = 0;
                }
                
                view.caret.preferred_column = view.caret.wrapped_column;

                LOG_INFO("caret: %lld, column: %lld, wrapped_column: %lld, line: %d, wrapped_line: %d", 
                         view.caret.byte_offset, 
                         view.caret.column+1, view.caret.wrapped_column+1, 
                         view.caret.line+1, view.caret.wrapped_line+1);
                app.animating = true;
            } break;
        case IK_UP:
            if (view.caret.wrapped_line > 0) {
                i32 wrapped_line = --view.caret.wrapped_line;
                if (!view.lines[view.caret.wrapped_line+1].wrapped) view.caret.line--;
                
                // NOTE(jesper): these are pretty wasteful and could be combined, and might cause
                // problems in edge case files with super long lines
                // TODO(jesper): notepad does a funky thing here with the caret when going down into a tab-character wherein
                // it rounds up or down to after or before the tab-character depending on the mid point of the current column.
                view.caret.wrapped_column = calc_wrapped_column(wrapped_line, view.caret.preferred_column, view.lines, buffer);
                view.caret.column = calc_unwrapped_column(wrapped_line, view.caret.wrapped_column, view.lines, buffer);
                view.caret.byte_offset = calc_byte_offset(view.caret.wrapped_column, view.caret.wrapped_line, view.lines, buffer);

                LOG_INFO("caret: %lld, column: %lld, wrapped_column: %lld, line: %d, wrapped_line: %d", 
                         view.caret.byte_offset, 
                         view.caret.column+1, view.caret.wrapped_column+1, 
                         view.caret.line+1, view.caret.wrapped_line+1);
            } break;
        case IK_DOWN:
            if (view.caret.wrapped_line < view.lines.count-1) {
                i32 wrapped_line = ++view.caret.wrapped_line;
                if (!view.lines[view.caret.wrapped_line].wrapped) view.caret.line++;
                
                // NOTE(jesper): these are pretty wasteful and could be combined, and might cause
                // problems in edge case files with super long lines
                // TODO(jesper): notepad does a funky thing here with the caret when going down into a tab-character wherein
                // it rounds up or down to after or before the tab-character depending on the mid point of the current column.
                view.caret.wrapped_column = calc_wrapped_column(wrapped_line, view.caret.preferred_column, view.lines, buffer);
                view.caret.column = calc_unwrapped_column(wrapped_line, view.caret.wrapped_column, view.lines, buffer);
                view.caret.byte_offset = calc_byte_offset(view.caret.wrapped_column, view.caret.wrapped_line, view.lines, buffer);
                
                LOG_INFO("caret: %lld, column: %lld, wrapped_column: %lld, line: %d, wrapped_line: %d", 
                         view.caret.byte_offset, 
                         view.caret.column+1, view.caret.wrapped_column+1, 
                         view.caret.line+1, view.caret.wrapped_line+1);
            } break;
        }
        break;
    case IE_KEY_RELEASE: break;
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
    }
}

bool is_word_boundary(i32 c)
{
    switch (c) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '.':
    case ',':
    case '(':
    case ')':
        return true;
    default: return false;
    }
}

void update_and_render(f32 dt)
{
    gfx_begin_frame();
    gui_begin_frame();
    
    GfxCommandBuffer debug_gfx = gfx_command_buffer();

    if (view.lines_dirty) {
        view.lines_dirty = false;
        
        view.lines.count = 0;
        array_add(&view.lines, BufferLine{ 0, 0 });
        
        // TODO(jesper): figure out what I wanna do here to re-flow the lines. Do I create persistent gui layouts
        // that I can use when reflowing? Do I reflow again if view_rect is different? How do I calculate the view_rect
        // before the scrollbar?
        Rect r{ .size = { gfx.resolution.x-gui.style.scrollbar.thickness, gfx.resolution.y } };
        
        Vector2 baseline{ r.pos.x, r.pos.y + (f32)app.mono.baseline };
        Vector2 pen = baseline;
        
        Buffer *buffer = &buffers[view.buffer.index];
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
        
        view.caret.wrapped_line = MIN(view.caret.wrapped_line, view.lines.count-1);
        while (view.caret.byte_offset < view.lines[view.caret.wrapped_line].offset) view.caret.wrapped_line--;
        while (view.caret.byte_offset >= line_end_offset(view.caret.wrapped_line, view.lines, buffer)) view.caret.wrapped_line++;
        
        view.caret.wrapped_column = calc_wrapped_column_from_byte_offset(view.caret.byte_offset, view.caret.wrapped_line, view.lines, buffer);
        view.caret.preferred_column = view.caret.wrapped_column;
        
        LOG_INFO("caret: %lld, column: %lld, wrapped_column: %lld, line: %d, wrapped_line: %d", 
                 view.caret.byte_offset, 
                 view.caret.column+1, view.caret.wrapped_column+1,
                 view.caret.line+1, view.caret.wrapped_line+1);
    }
    
    {
        // TODO(jesper): figure out what I wanna do here to re-flow the lines. Do I create persistent gui layouts
        // that I can use when reflowing? Do I reflow again if view_rect is different? How do I calculate the view_rect
        // before the scrollbar?
        gui_scrollbar(app.mono.line_height, &view.line_offset, view.lines.count, view.lines_visible, &view.voffset);
        view.rect = gui_layout_widget_fill();

        GfxCommand cmd = gui_command(GFX_COMMAND_GUI_TEXT);
        cmd.gui_text.font_atlas = app.mono.texture;
        cmd.gui_text.color = { 0.0f, 0.0f, 0.0f };
        
        f32 voffset = view.voffset;
        i32 line_offset = view.line_offset;
        
        Buffer *buffer = &buffers[view.buffer.index];

        for (i32 i = 0; i < MIN(view.lines.count - line_offset, view.lines_visible); i++)
        {
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
                vcolumn++;
            }
        }
        
        gui_push_command(&gfx.frame_cmdbuf, cmd);
    }
    
    if (view.caret.wrapped_line >= view.line_offset && 
        view.caret.wrapped_line < view.line_offset + view.lines_visible)
    {
        i32 i = view.caret.wrapped_line - view.line_offset;

        Vector2 pen{ 
            view.rect.pos.x + view.caret.wrapped_column*app.mono.space_width,
            view.rect.pos.y + i*app.mono.line_height + (f32)app.mono.baseline - view.voffset
        };

        // TODO(jesper): animate blinking?
        gui_draw_rect(&gfx.frame_cmdbuf, { pen.x, pen.y - app.mono.baseline }, { 1.0f, app.mono.line_height }, { 1.0f, 0.0f, 0.0f });

    }

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    gui_end_frame();

    gfx_flush_transfers();
    gfx_submit_commands(gfx.frame_cmdbuf);
    gfx_submit_commands(debug_gfx);

    gui_render();
    
    app.animating = false;
}