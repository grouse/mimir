#include "gui.h"

#include "core.h"
#include "file.h"

extern Allocator mem_frame;

GuiContext gui;

#define GIZMO_BAR_SIZE Vector2{ 48.0f, 4.0f }
#define GIZMO_TIP_SIZE Vector2{ 10.0f, 10.0f }
#define GIZMO_OUTLINE_COLOR Vector3{ 0.0f, 0.0f, 0.0f }

Font load_font(String path)
{
    Font font{};
    
    FileInfo f = read_file(path);
    if (f.data) {
        stbtt_fontinfo fi;
        stbtt_InitFont(&fi, f.data, 0);

        u8 *bitmap = (u8*)ALLOC(mem_dynamic, 1024*1024);
        stbtt_BakeFontBitmap(f.data, 0, 18.0f, bitmap, 1024, 1024, 0, 256, font.atlas);
        stbtt_GetFontVMetrics(&fi, &font.ascent, &font.descent, &font.line_gap);
        font.scale = stbtt_ScaleForPixelHeight(&fi, 18);
        font.line_height = font.scale * (font.ascent - font.descent + font.line_gap);
        font.baseline = (i32)(font.ascent*font.scale);
        
        // TODO(jesper): this is not really correct. The text layout code needs to query the codepoint
        // advance and codepoint kerning
        f32 x = 0, y = 0;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(font.atlas, 1024, 1024, ' ', &x, &y, &q, 1);
        font.space_width = x;

        glGenTextures(1, &font.texture);
        glBindTexture(GL_TEXTURE_2D, font.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1024, 1024, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap); 
    }

    return font;
}

void init_gui(String assets_dir)
{
    glGenBuffers(1, &gui.vbo);
    
    GuiWindow world_wnd{
        .id = GUI_ID_INVALID,
        .clip_rect = Rect{ { 0.0f, 0.0f }, { gfx.resolution.x, gfx.resolution.y } },
        .command_buffer.commands.alloc = mem_frame,
    };
    array_add(&gui.windows, world_wnd);
    
    GuiWindow root{
        .id = GUI_ID_INVALID,
        .clip_rect = Rect{ { 0.0f, 0.0f }, { gfx.resolution.x, gfx.resolution.y } },
        .command_buffer.commands.alloc = mem_frame,
    };
    array_add(&gui.windows, root);
    
    GuiWindow overlay = root;
    array_add(&gui.windows, overlay);

    gui.current_window = 1;

    gui.camera.projection = matrix4_identity();
    gui.camera.projection[0][0] = 2.0f / gfx.resolution.x;
    gui.camera.projection[1][1] = 2.0f / gfx.resolution.y;
    gui.camera.projection[3][1] = -1.0f;
    gui.camera.projection[3][0] = -1.0f;
    
    //TextureAsset *texture = find_texture_asset("textures/close.png");
    //PANIC_IF(!texture, "unable to find close button icon asset");
    //gui.style.icons.close = texture->texture_handle;
    gui.style.text.font = load_font(join_path(assets_dir, "fonts/Roboto-Regular.ttf"));
    gui.style.button.font = gui.style.text.font;
}

GuiLayout *gui_current_layout()
{
    ASSERT(gui.layout_stack.count > 0);
    return &gui.layout_stack[gui.layout_stack.count-1];
}

void gui_hot(GuiId id, i32 hot_z)
{
    gui.hot = gui.hot == GUI_ID_INVALID || gui.hot_z <= hot_z ? id : gui.hot;
    gui.hot_z = gui.hot_z <= hot_z ? hot_z : gui.hot_z;
}

void gui_hot(GuiId id)
{
    gui.hot = id;
    gui.hot_z = -1;
}

void gui_active(GuiId id)
{
    gui.active = id;
}

void gui_begin_frame()
{
    for (GuiWindow &wnd : gui.windows) {
        array_reset(&wnd.command_buffer.commands, mem_frame, wnd.command_buffer.commands.capacity);
    }
    array_reset(&gui.vertices, mem_frame, gui.vertices.count);
    array_reset(&gui.layout_stack, mem_frame, gui.layout_stack.count);
    
    // NOTE(jesper): when resolution changes the root and overlay "windows" have to adjust their
    // clip rects as well
    gui.windows[0].clip_rect.size = gfx.resolution;
    gui.windows[1].clip_rect.size = gfx.resolution;

    GuiLayout root_layout{
        .type = GUI_LAYOUT_ABSOLUTE,
        .pos = { 0.0f, 0.0f },
        .size = gfx.resolution
    };
    gui_begin_layout(root_layout);
    
    gui.last_active = gui.active;
}

void gui_end_frame()
{
    ASSERT(gui.id_stack.count == 0);
    ASSERT(gui.layout_stack.count == 1);

    glBindBuffer(GL_ARRAY_BUFFER, gui.vbo);
    glBufferData(GL_ARRAY_BUFFER, gui.vertices.count * sizeof gui.vertices[0], gui.vertices.data, GL_STREAM_DRAW);

    if (!gui.mouse.left_pressed) gui.active_press = GUI_ID_INVALID;
}

void gui_render()
{
    gfx_submit_commands(gui.windows[0].command_buffer);
    for (i32 i = 2; i < gui.windows.count; i++) {
        gfx_submit_commands(gui.windows[i].command_buffer);
    }
    gfx_submit_commands(gui.windows[1].command_buffer);
}

GfxCommand gui_command(GfxCommandType type)
{
    GfxCommand cmd{ .type = type };
    switch (type) {
    case GFX_COMMAND_GUI_PRIM_COLOR:
        cmd.gui_prim.vbo = gui.vbo;
        cmd.gui_prim.vbo_offset = gui.vertices.count;
        break;
    case GFX_COMMAND_GUI_TEXT:
        cmd.gui_text.vbo = gui.vbo;
        cmd.gui_text.vbo_offset = gui.vertices.count;
        break;
    case GFX_COMMAND_GUI_PRIM_TEXTURE:
        cmd.gui_prim_texture.vbo = gui.vbo;
        cmd.gui_prim_texture.vbo_offset = gui.vertices.count;
        break;
    case GFX_COMMAND_COLORED_LINE:
    case GFX_COMMAND_COLORED_PRIM:
    case GFX_COMMAND_TEXTURED_PRIM:
        LOG_ERROR("don't use this API for these");
        break;
    }
    return cmd;
}

void gui_push_command(GfxCommandBuffer *cmdbuf, GfxCommand cmd, i32 draw_index = -1)
{
    i32 index = draw_index != -1 ? draw_index : cmdbuf->commands.count-1;
    if (index >= 0 && cmdbuf->commands.count > index) {
        GfxCommand *existing = &cmdbuf->commands[index];
        
        if (existing->type == cmd.type) {
            switch (cmd.type) {
            case GFX_COMMAND_GUI_PRIM_COLOR:
                if (existing->gui_prim.vbo_offset + existing->gui_prim.vertex_count == cmd.gui_prim.vbo_offset) {
                    existing->gui_prim.vertex_count += cmd.gui_prim.vertex_count;
                    return;
                }
                break;
            case GFX_COMMAND_GUI_TEXT:
                if (existing->gui_text.vbo_offset + existing->gui_text.vertex_count == cmd.gui_text.vbo_offset &&
                    existing->gui_text.font_atlas == cmd.gui_text.font_atlas &&
                    existing->gui_text.color == cmd.gui_text.color) 
                {
                    existing->gui_text.vertex_count += cmd.gui_text.vertex_count;
                    return;
                }
                break;
            case GFX_COMMAND_GUI_PRIM_TEXTURE:
                if (existing->gui_prim_texture.vbo_offset + existing->gui_prim_texture.vertex_count == cmd.gui_prim_texture.vbo_offset &&
                    existing->gui_prim_texture.texture == cmd.gui_prim_texture.texture)
                {
                    existing->gui_prim_texture.vertex_count += cmd.gui_prim_texture.vertex_count;
                    return;
                }
                break;
            case GFX_COMMAND_COLORED_LINE:
            case GFX_COMMAND_COLORED_PRIM:
            case GFX_COMMAND_TEXTURED_PRIM:
                break;
            }
        }
    }
    
    if (draw_index >= 0) array_insert(&cmdbuf->commands, draw_index, cmd);
    else array_add(&cmdbuf->commands, cmd);
}

void gui_draw_rect(
    GfxCommandBuffer *cmdbuf,
    Vector2 pos, 
    Vector2 size, 
    Rect clip_rect,
    GLuint texture)
{
    f32 left = pos.x;
    f32 right = pos.x + size.x;
    f32 top = pos.y;
    f32 bottom = pos.y + size.y;

    if (left >= clip_rect.pos.x + clip_rect.size.x) return;
    if (top >= clip_rect.pos.y + clip_rect.size.y) return;

    right = MIN(right, clip_rect.pos.x + clip_rect.size.x);
    bottom = MIN(bottom, clip_rect.pos.y + clip_rect.size.y);

    f32 vertices[] = {
        right, top, 1.0f, 0.0f,
        left, top, 0.0f, 0.0f,
        left, bottom, 0.0f, 1.0f,

        left, bottom, 0.0f, 1.0f,
        right, bottom, 1.0f, 1.0f,
        right, top, 1.0f, 0.0f,
    };

    GfxCommand cmd = gui_command(GFX_COMMAND_GUI_PRIM_TEXTURE);
    cmd.gui_prim_texture.texture = texture;
    cmd.gui_prim_texture.vertex_count = ARRAY_COUNT(vertices);
    gui_push_command(cmdbuf, cmd);
    
    array_add(&gui.vertices, vertices, ARRAY_COUNT(vertices));
}

void gui_draw_rect(
    GfxCommandBuffer *cmdbuf,
    Vector2 pos, 
    Vector2 size, 
    Rect clip_rect,
    Vector3 color)
{
    f32 left = pos.x;
    f32 right = pos.x + size.x;
    f32 top = pos.y;
    f32 bottom = pos.y + size.y;

    if (left >= clip_rect.pos.x + clip_rect.size.x) return;
    if (top >= clip_rect.pos.y + clip_rect.size.y) return;

    right = MIN(right, clip_rect.pos.x + clip_rect.size.x);
    bottom = MIN(bottom, clip_rect.pos.y + clip_rect.size.y);

    color = linear_from_sRGB(color);

    f32 vertices[] = {
        right, top, color.r, color.g, color.b,
        left, top, color.r, color.g, color.b,
        left, bottom, color.r, color.g, color.b,

        left, bottom, color.r, color.g, color.b,
        right, bottom, color.r, color.g, color.b,
        right, top, color.r, color.g, color.b,
    };

    GfxCommand cmd = gui_command(GFX_COMMAND_GUI_PRIM_COLOR);
    cmd.gui_prim.vertex_count = ARRAY_COUNT(vertices);
    gui_push_command(cmdbuf, cmd);

    array_add(&gui.vertices, vertices, ARRAY_COUNT(vertices));
}

void gui_draw_rect(
    GfxCommandBuffer *cmdbuf,
    Vector2 pos, 
    Vector2 size, 
    Vector3 color,
    i32 draw_index = -1)
{
    f32 left = pos.x;
    f32 right = pos.x + size.x;
    f32 top = pos.y;
    f32 bottom = pos.y + size.y;

    color = linear_from_sRGB(color);

    f32 vertices[] = {
        right, top, color.r, color.g, color.b,
        left, top, color.r, color.g, color.b,
        left, bottom, color.r, color.g, color.b,

        left, bottom, color.r, color.g, color.b,
        right, bottom, color.r, color.g, color.b,
        right, top, color.r, color.g, color.b,
    };

    GfxCommand cmd = gui_command(GFX_COMMAND_GUI_PRIM_COLOR);
    cmd.gui_prim.vertex_count = ARRAY_COUNT(vertices);
    gui_push_command(cmdbuf, cmd, draw_index);

    array_add(&gui.vertices, vertices, ARRAY_COUNT(vertices));
}

TextQuadsAndBounds calc_text_quads_and_bounds(String text, Font *font, Allocator alloc = mem_tmp)
{
    TextQuadsAndBounds r{};
    array_reset(&r.quads, alloc, text.length);
    
    Vector2 cursor{ 0.0f, (f32)font->baseline };

    char *p = text.data;
    char *end = text.data+text.length;
    while (p < end) {
        i32 c = utf32_it_next(&p, end);
        if (c == 0) return r;

        if (c == ' ') {
            cursor.x += font->space_width;
            continue;
        }
        
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(font->atlas, 1024, 1024, c, &cursor.x, &cursor.y, &q, 1);
        
        array_add(&r.quads, q);
        
        r.bounds.pos.x = MIN(r.bounds.pos.x, MIN(q.x0, q.x1));
        r.bounds.pos.y = MIN(r.bounds.pos.y, MIN(q.y0, q.y1));
        
        r.bounds.size.x = MAX(r.bounds.size.x, MAX(q.x0, q.x1));
        r.bounds.size.y = MAX(r.bounds.size.y, MAX(q.y0, q.y1));
    }
    
    return r;
}

DynamicArray<TextQuad> calc_text_quads(String text, Font *font, Allocator alloc = mem_tmp)
{
    DynamicArray<TextQuad> quads{};
    array_reset(&quads, alloc, text.length);

    Vector2 cursor{ 0.0f, (f32)font->baseline };
    
    char *p = text.data;
    char *end = text.data+text.length;
    while (p < end) {
        i32 c = utf32_it_next(&p, end);
        if (c == 0) return quads;
        
        if (c == ' ') {
            cursor.x += font->space_width;
            continue;
        }

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(font->atlas, 1024, 1024, c, &cursor.x, &cursor.y, &q, 1);

        array_add(&quads, q);
    }

    return quads;
}

bool apply_clip_rect(TextQuad *q, Rect clip_rect)
{
    if (q->x0 > clip_rect.pos.x + clip_rect.size.x) return false;
    if (q->y0 > clip_rect.pos.y + clip_rect.size.y) return false;
    if (q->x1 < clip_rect.pos.x) return false;
    if (q->y1 < clip_rect.pos.y) return false;

    if (q->x1 > clip_rect.pos.x + clip_rect.size.x) {
        f32 w0 = q->x1 - q->x0;
        q->x1 = clip_rect.pos.x + clip_rect.size.x;
        f32 w1 = q->x1 - q->x0;
        q->s1 = q->s0 + (q->s1 - q->s0) * (w1/w0);
    }

    if (q->x0 < clip_rect.pos.x) {
        f32 w0 = q->x1 - q->x0;
        q->x0 = clip_rect.pos.x;
        f32 w1 = q->x1 - q->x0;
        q->s0 = q->s1 - (q->s1 - q->s0) * (w1/w0);
    }

    if (q->y1 > clip_rect.pos.y + clip_rect.size.y) {
        f32 h0 = q->y1 - q->y0;
        q->y1 = clip_rect.pos.y + clip_rect.size.y;
        f32 h1 = q->y1 - q->y0;
        q->t1 = q->t0 + (q->t1 - q->t0) * (h1/h0);
    }

    if (q->y0 < clip_rect.pos.y) {
        f32 h0 = q->y1 - q->y0;
        q->y0 = clip_rect.pos.y;
        f32 h1 = q->y1 - q->y0;
        q->t0 = q->t1 - (q->t1 - q->t0) * (h1/h0);
    }

    return true;
}

Vector2 gui_draw_text(
    GfxCommandBuffer *cmdbuf,
    Array<TextQuad> quads,
    Vector2 pos,
    Rect clip_rect,
    Vector3 color,
    Font *font)
{
    Vector2 cursor = pos;
    cursor.y += font->baseline;

    GfxCommand cmd = gui_command(GFX_COMMAND_GUI_TEXT);
    cmd.gui_text.font_atlas = font->texture;
    cmd.gui_text.color = color;

    for (TextQuad q : quads) {
        q.x0 += pos.x;
        q.x1 += pos.x;
        q.y0 += pos.y;
        q.y1 += pos.y;
        
        if (!apply_clip_rect(&q, clip_rect)) continue;

        f32 verts[] = { 
            q.x0, q.y0, q.s0, q.t0,
            q.x0, q.y1, q.s0, q.t1,
            q.x1, q.y1, q.s1, q.t1,

            q.x1, q.y1, q.s1, q.t1,
            q.x1, q.y0, q.s1, q.t0,
            q.x0, q.y0, q.s0, q.t0,
        };

        array_add(&gui.vertices, verts, ARRAY_COUNT(verts));
        cmd.gui_text.vertex_count += ARRAY_COUNT(verts);
    }
    
    gui_push_command(cmdbuf, cmd);
    return cursor;
}

Vector2 gui_draw_text(
    GfxCommandBuffer *cmdbuf,
    String text,
    Vector2 pos,
    Rect clip_rect,
    Vector3 color,
    Font *font)
{
    Vector2 cursor = pos;
    cursor.y += gui.style.text.font.baseline;

    GfxCommand cmd = gui_command(GFX_COMMAND_GUI_TEXT);
    cmd.gui_text.font_atlas = font->texture;
    cmd.gui_text.color = color;

    char *p = text.data;
    char *end = text.data+text.length;
    while (p < end) {
        i32 c = utf32_it_next(&p, end);
        if (c == 0) return cursor;
        
        if (c == ' ') {
            cursor.x += font->space_width;
            continue;
        }

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(font->atlas, 1024, 1024, c, &cursor.x, &cursor.y, &q, 1);
        
        if (!apply_clip_rect(&q, clip_rect)) continue;

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
    gui_push_command(cmdbuf, cmd);

    return cursor;
}

void gui_textbox(String str, Font *font = &gui.style.text.font)
{
    GuiWindow *wnd = &gui.windows[gui.current_window];
    GfxCommandBuffer *cmdbuf = &wnd->command_buffer;
    
    TextQuadsAndBounds td = calc_text_quads_and_bounds(str, font);
    Vector2 pos = gui_layout_widget({ td.bounds.size.x, MAX(td.bounds.size.y, font->line_height) });
    
    gui_draw_text(cmdbuf, td.quads, pos, wnd->clip_rect, gui.style.text.color, font);
}

void gui_textbox(String str, Vector2 pos, Font *font = &gui.style.text.font)
{
    GuiWindow *wnd = &gui.windows[gui.current_window];
    GfxCommandBuffer *cmdbuf = &wnd->command_buffer;

    TextQuadsAndBounds td = calc_text_quads_and_bounds(str, font);
    // TODO(jesper): should this be pushing a widget onto the layout to take up space in some way?
    gui_draw_text(cmdbuf, td.quads, pos, wnd->clip_rect, gui.style.text.color, font);
}


GuiWindow* push_window_to_top(i32 index)
{
    if (index != gui.windows.count-1) {
        GuiWindow window = gui.windows[index];
        array_remove_sorted(&gui.windows, index);
        array_add(&gui.windows, window);
    }
    gui.current_window = gui.windows.count-1;
    return &gui.windows[gui.windows.count-1];
}

bool gui_hot_rect(GuiId id, Vector2 pos, Vector2 size)
{
    // TODO(jesper): investigate whether this should short circuit if gui.active == id
    Vector2 mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };
    Vector2 rel = mouse - pos;

    if (gui.active == id ||
        (rel.x >= 0.0f && rel.x < size.x &&
         rel.y >= 0.0f && rel.y < size.y))
    {
        gui_hot(id, gui.current_window);
        return true;
    } else if (gui.hot == id) {
        gui_hot(GUI_ID_INVALID);
    }
    
    return false;
}

void gui_drag_start(Vector2 data)
{
    gui.drag_start_mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };
    gui.drag_start_data = data;
}

void gui_drag_start(Vector2 data, Vector2 data1)
{
    gui.drag_start_mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };
    gui.drag_start_data = data;
    gui.drag_start_data1 = data1;
}


bool gui_active_click(GuiId id)
{
    bool clicked = false;
    if (gui.hot == id && gui.mouse.left_pressed && !gui.mouse.left_was_pressed) {
        gui_active(id);
    } else if (gui.active == id && !gui.mouse.left_pressed) {
        clicked = gui.hot == id && gui.mouse.left_was_pressed;
        gui_active(GUI_ID_INVALID);
    }
    
    return clicked;
}

void gui_active_press(GuiId id)
{
    if (gui.hot == id && gui.mouse.left_pressed && !gui.mouse.left_was_pressed) {
        gui.active == id ? gui_active(GUI_ID_INVALID) : gui_active(id);
        gui.active_press = gui.active;
    } 
}

bool gui_active_drag(GuiId id, Vector2 data)
{
    if (gui.hot == id) {
        if (gui.active != id && gui.mouse.left_pressed && !gui.mouse.left_was_pressed)  {
            gui_active(id);
            gui_drag_start(data);
        } else if (gui.active == id && !gui.mouse.left_pressed) {
            gui_active(GUI_ID_INVALID);
        }
    }
    
    return gui.active == id;
}

bool gui_button_id(GuiId id, Vector2 pos, Vector2 size)
{
    GuiWindow *wnd = &gui.windows[gui.current_window];
    GfxCommandBuffer *cmdbuf = &wnd->command_buffer;

    gui_hot_rect(id, pos, size);
    bool clicked = gui_active_click(id);

    Vector3 btn_bg = gui.active == id ? gui.style.button.bg_active : gui.hot == id ? gui.style.button.bg_hot : gui.style.button.bg;
    Vector3 btn_bg_acc0 = gui.active == id ? gui.style.button.accent0_active : gui.style.button.accent0;
    Vector3 btn_bg_acc1 = gui.active == id ? gui.style.button.accent1_active : gui.style.button.accent1;
    Vector3 btn_bg_acc2 = gui.style.button.accent2;

    Rect clip_rect{ wnd->clip_rect };
    gui_draw_rect(cmdbuf, pos, { size.x - 1.0f, 1.0f }, clip_rect, btn_bg_acc0);
    gui_draw_rect(cmdbuf, pos, { 1.0f, size.y - 1.0f }, clip_rect, btn_bg_acc0);
    gui_draw_rect(cmdbuf, pos + Vector2{ size.x - 1.0f, 0.0f }, { 1.0f, size.y }, clip_rect, btn_bg_acc1);
    gui_draw_rect(cmdbuf, pos + Vector2{ 0.0f, size.y - 1.0f }, { size.x, 1.0f }, clip_rect, btn_bg_acc1);
    gui_draw_rect(cmdbuf, pos + Vector2{ size.x - 2.0f, 1.0f }, { 1.0f, size.y - 2.0f }, clip_rect, btn_bg_acc2);
    gui_draw_rect(cmdbuf, pos + Vector2{ 1.0f, size.y - 2.0f }, { size.x - 2.0f, 1.0f }, clip_rect, btn_bg_acc2);
    gui_draw_rect(cmdbuf, pos + Vector2{ 1.0f, 1.0f }, size - Vector2{ 3.0f, 3.0f }, clip_rect, btn_bg);
    return clicked;
}

bool gui_button_id(GuiId id, Font *font, TextQuadsAndBounds td, Vector2 size)
{
    GuiWindow *wnd = &gui.windows[gui.current_window];
    GfxCommandBuffer *cmdbuf = &wnd->command_buffer;
    
    Vector2 pos = gui_layout_widget(size);
    
    
    bool clicked = gui_button_id(id, pos, size);
    
    Vector3 btn_fg = rgb_unpack(0xFFFFFFFF);

    Rect clip_rect{ wnd->clip_rect };
    Vector2 text_center{ td.bounds.size.x * 0.5f, font->line_height*0.5f };
    Vector2 btn_center = size * 0.5f;
    Vector2 text_offset = btn_center - text_center;

    gui_draw_text(cmdbuf, td.quads, pos + text_offset, clip_rect, btn_fg, font);
    return clicked;
}

bool gui_button_id(GuiId id, String text, Vector2 size)
{
    TextQuadsAndBounds td = calc_text_quads_and_bounds(text, &gui.style.button.font);
    return gui_button_id(id, &gui.style.button.font, td, size);
}

bool gui_button_id(GuiId id, String text)
{
    TextQuadsAndBounds td = calc_text_quads_and_bounds(text, &gui.style.button.font);
    return gui_button_id(id, &gui.style.button.font, td, td.bounds.size + Vector2{ 24.0f, 12.0f });
}

void gui_checkbox_id(GuiId id, String label, bool *checked)
{
    id.internal = id.owner;
    id.owner = gui.current_id.owner;
    
    GuiWindow *wnd = &gui.windows[gui.current_window];
    GfxCommandBuffer *cmdbuf = &wnd->command_buffer;

    TextQuadsAndBounds td = calc_text_quads_and_bounds(label, &gui.style.text.font);

    Vector2 btn_margin{ 10.0f, 0.0f };
    Vector2 btn_size{ 16.0f, 16.0f };
    Vector2 inner_size{ 8.0f, 8.0f };
    Vector2 border_size{ 1.0f, 1.0f };
    Vector2 inner_margin = 0.5f*(btn_size - inner_size);
    Vector2 btn_offset = 0.5f*Vector2{ gui.style.text.font.line_height, gui.style.text.font.line_height } - 0.5f*btn_size;
    
    Vector2 size{ td.bounds.size.x + btn_size.x + btn_margin.x, MAX(btn_size.y, td.bounds.size.y) };
    Vector2 pos = gui_layout_widget(&size);

    Vector2 btn_pos = pos + btn_offset;

    gui_hot_rect(id, pos, size);
    if (gui_active_click(id)) *checked = !(*checked);
    
    Vector3 border_col = rgb_unpack(0xFFCCCCCC);
    Vector3 bg_col = gui.active == id ? rgb_unpack(0xFF2C2C2C) : gui.hot == id ? rgb_unpack(0xFF3A3A3A) : rgb_unpack(0xFF1d2021);
    Vector3 checked_col = rgb_unpack(0xFFCCCCCC);
    Vector3 label_col = rgb_unpack(0xFFFFFFFF);

    gui_draw_rect(cmdbuf, btn_pos, btn_size, wnd->clip_rect, border_col);
    gui_draw_rect(cmdbuf, btn_pos + border_size, btn_size - 2.0f*border_size, wnd->clip_rect, bg_col);
    if (*checked) gui_draw_rect(cmdbuf, btn_pos + inner_margin, inner_size, wnd->clip_rect, checked_col);
    gui_draw_text(cmdbuf, td.quads, pos + Vector2{ btn_size.x + btn_margin.x, 0.0 }, wnd->clip_rect, label_col, &gui.style.text.font);
}

GuiEditboxAction gui_editbox_id(GuiId id, String in_str, Vector2 pos, Vector2 size)
{
    u32 action = GUI_EDITBOX_NONE;
    
    GuiWindow *wnd = &gui.windows[gui.current_window];
    GfxCommandBuffer *cmdbuf = &wnd->command_buffer;
    
    Vector2 mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };
    Vector2 rel = mouse - pos;
    if ((rel.x >= 0.0f && rel.x < size.x &&
         rel.y >= 0.0f && rel.y < size.y))
    {
        gui_hot(id, gui.current_window);
    } else if (gui.hot == id) {
        gui_hot(GUI_ID_INVALID);
    }
    
    if (gui.mouse.left_pressed && !gui.mouse.left_was_pressed) {
        if (gui.hot == id) {
            gui_active(id);
            gui.text_input = true;
            gui.edit.selection = 0;
            gui.edit.cursor = 0;
            gui.edit.offset = 0;
            memcpy(gui.edit.buffer, in_str.data, in_str.length);
            gui.edit.length = in_str.length;
        } else if (gui.active == id) {
            gui_active(GUI_ID_INVALID);
            action |= GUI_EDITBOX_CANCEL;
        }
    }

    Vector3 selection_bg = rgb_unpack(0xFFCCCCCC);
    Vector3 edit_bg = rgb_unpack(0xFF1D2021);
    Vector3 edit_acc0 = rgb_unpack(0xFF404040);
    Vector3 edit_fg = rgb_unpack(0xFFFFFFFF);
    
    Vector2 edit_pos = pos + Vector2{ 1.0f, 1.0f };
    Vector2 edit_size = size - Vector2{ 2.0f, 2.0f };
    
    if (gui.active == id) {
        for (TextInput in : gui.text_input_queue) {
            switch (in.type) {
            case TEXT_INPUT_INVALID:
                LOG_ERROR("invalid text input event received");
                break;
            case TEXT_INPUT_CURSOR_LEFT:
                gui.edit.cursor = utf8_decr({ gui.edit.buffer, gui.edit.length }, gui.edit.cursor);
                gui.edit.cursor = MAX(0, gui.edit.cursor);
                gui.edit.selection = gui.edit.cursor;
                break;
            case TEXT_INPUT_CURSOR_RIGHT:
                gui.edit.cursor = utf8_incr({ gui.edit.buffer, gui.edit.length }, gui.edit.cursor);
                gui.edit.selection = gui.edit.cursor;
                break;
            case TEXT_INPUT_SELECT_ALL:
                gui.edit.selection = 0;
                gui.edit.cursor = gui.edit.length;
                break;
            case TEXT_INPUT_COPY: {
                    if (gui.edit.cursor != gui.edit.selection) {
                        i32 start = MIN(gui.edit.cursor, gui.edit.selection);
                        i32 end = MAX(gui.edit.cursor, gui.edit.selection);
                        
                        String selected = slice({ gui.edit.buffer, gui.edit.length }, start, end);
                        set_clipboard_data(selected);
                    }
                } break;
            case TEXT_INPUT_PASTE: {
                    i32 start = MIN(gui.edit.cursor, gui.edit.selection);
                    i32 end = MAX(gui.edit.cursor, gui.edit.selection);

                    i32 limit = sizeof gui.edit.buffer - (gui.edit.length - (end-start));
                    in.str.length = utf8_truncate(in.str, limit);
                    
                    if (in.str.length > (end-start)) { 
                        memmove(
                            gui.edit.buffer+end+in.str.length-(end-start), 
                            gui.edit.buffer+end, 
                            gui.edit.length-end);
                    }
                    
                    memcpy(gui.edit.buffer+start, in.str.data, in.str.length);
                    
                    if (in.str.length < (end-start)) {
                        memmove(
                            gui.edit.buffer+start+in.str.length, 
                            gui.edit.buffer+end, 
                            gui.edit.length-end);
                    }

                    gui.edit.cursor = gui.edit.selection = start+in.str.length;
                    gui.edit.length = gui.edit.length - (end-start) + in.str.length;
                    action |= GUI_EDITBOX_CHANGE;
                } break;
            case TEXT_INPUT_DEL:
                if (gui.edit.cursor != gui.edit.selection || gui.edit.cursor < gui.edit.length) {
                    i32 start = MIN(gui.edit.cursor, gui.edit.selection);
                    i32 end = MAX(gui.edit.cursor, gui.edit.selection);
                    if (start == end) end = utf8_incr({ gui.edit.buffer, gui.edit.length }, end);
                    
                    i32 new_length = gui.edit.length-(end-start);
                    memmove(gui.edit.buffer+start, gui.edit.buffer+end, new_length);
                    gui.edit.length = new_length;
                    gui.edit.cursor = gui.edit.selection = start;
                    
                    i32 new_offset = codepoint_index_from_byte_index({ gui.edit.buffer, gui.edit.length}, gui.edit.cursor);
                    gui.edit.offset = MIN(gui.edit.offset, new_offset);
                    action |= GUI_EDITBOX_CHANGE;
                } break;
            case TEXT_INPUT_BACKSPACE:
                if (gui.edit.cursor != gui.edit.selection || gui.edit.cursor > 0) {
                    i32 start = MIN(gui.edit.cursor, gui.edit.selection);
                    i32 end = MAX(gui.edit.cursor, gui.edit.selection);
                    if (start == end) start = utf8_decr({ gui.edit.buffer, gui.edit.length }, start);

                    memmove(gui.edit.buffer+start, gui.edit.buffer+end, gui.edit.length-(end-start));
                    gui.edit.length = gui.edit.length-(end-start);
                    gui.edit.cursor = gui.edit.selection = start;
                    gui.edit.offset = MIN(gui.edit.offset, codepoint_index_from_byte_index({ gui.edit.buffer, gui.edit.length}, gui.edit.cursor));
                    action |= GUI_EDITBOX_CHANGE;
                } 
                break;
            case TEXT_INPUT_CHAR: 
                if (gui.edit.cursor != gui.edit.selection) {
                    i32 start = MIN(gui.edit.cursor, gui.edit.selection);
                    i32 end = MAX(gui.edit.cursor, gui.edit.selection);
                    if (start == end) end = utf8_incr({ gui.edit.buffer, gui.edit.length }, end);

                    memmove(gui.edit.buffer+start, gui.edit.buffer+end, gui.edit.length-(end-start));
                    gui.edit.length -= end-start;
                    gui.edit.cursor = gui.edit.selection = start;
                    
                    i32 new_offset = codepoint_index_from_byte_index({ gui.edit.buffer, gui.edit.length }, gui.edit.cursor);
                    gui.edit.offset = MIN(gui.edit.offset, new_offset);
                    action |= GUI_EDITBOX_CHANGE;
                }
                
                if (gui.edit.length+in.length <= (i32)sizeof gui.edit.buffer) {
                    for (i32 i = gui.edit.length + in.length-1; i > gui.edit.cursor; i--) {
                        gui.edit.buffer[i] = gui.edit.buffer[i-in.length];
                    }
                    
                    memcpy(gui.edit.buffer+gui.edit.cursor, in.c, in.length);
                    gui.edit.length += in.length;
                    gui.edit.cursor += in.length;
                    gui.edit.selection = gui.edit.cursor;
                    action |= GUI_EDITBOX_CHANGE;
                }
                break;
            case TEXT_INPUT_ENTER:
                action |= GUI_EDITBOX_FINISH;
                gui_hot(GUI_ID_INVALID);
                gui_active(GUI_ID_INVALID);
                gui.text_input = false;
                break;
            case TEXT_INPUT_CANCEL:
                action |= GUI_EDITBOX_CANCEL;
                gui_hot(GUI_ID_INVALID);
                gui_active(GUI_ID_INVALID);
                gui.text_input = false;
                break;
            }
            
            if (gui.edit.length == sizeof gui.edit.buffer) break;
        }
        
        gui.text_input_queue.count = 0;
    } else if (gui.last_active == id) {
        gui.text_input = false;
        gui.text_input_queue.count = 0;
    }

    Rect text_clip_rect;
    text_clip_rect.pos.x = MAX(wnd->clip_rect.pos.x, edit_pos.x);
    text_clip_rect.pos.y = MAX(wnd->clip_rect.pos.y, edit_pos.y);
    text_clip_rect.size.x = MIN(edit_pos.x + edit_size.x, wnd->clip_rect.pos.x + wnd->clip_rect.size.x) - text_clip_rect.pos.x;
    text_clip_rect.size.y = MIN(edit_pos.y + edit_size.y, wnd->clip_rect.pos.y + wnd->clip_rect.size.y) - text_clip_rect.pos.y;
    
    String str = gui.active == id ? String{ gui.edit.buffer, gui.edit.length } : in_str;
    Array<TextQuad> quads = calc_text_quads(str, &gui.style.text.font);

    gui_draw_rect(cmdbuf, pos, size, wnd->clip_rect, edit_acc0);
    gui_draw_rect(cmdbuf, edit_pos, edit_size, wnd->clip_rect, edit_bg);

    Vector2 cursor_pos = pos + Vector2{ 1.0f, 0.0f };
    Vector2 text_offset{}; 
    
    if (quads.count > 0) {
        i32 offset = gui.active == id ? gui.edit.offset : 0;
        
        if (gui.active == id) {
            text_offset.x = gui.edit.offset < quads.count ? 
                -quads[gui.edit.offset].x0 : 
                -quads[gui.edit.offset-1].x1;

            if (gui.mouse.left_pressed && !gui.mouse.left_was_pressed) {
                i32 new_cursor;
                for (new_cursor = 0; new_cursor < quads.count; new_cursor++) {
                    TextQuad q = quads.data[new_cursor];
                    if (rel.x >= q.x0 + text_offset.x && rel.x <= q.x1 + text_offset.x) {
                        break;
                    }
                }
                gui.edit.cursor = byte_index_from_codepoint_index(str, new_cursor);
                gui.edit.selection = gui.edit.cursor;
            } else if (gui.mouse.left_pressed) {
                i32 new_selection;
                for (new_selection = 0; new_selection < quads.count; new_selection++) {
                    TextQuad q = quads.data[new_selection];
                    if (rel.x >= q.x0 + text_offset.x && rel.x <= q.x1 + text_offset.x) {
                        break;
                    }
                }
                gui.edit.selection = byte_index_from_codepoint_index(str, new_selection);
            }

            i32 cursor_ci = codepoint_index_from_byte_index(str, gui.edit.cursor);
            cursor_pos.x += cursor_ci < quads.count ? quads[cursor_ci].x0 : quads[cursor_ci-1].x1; 
            while ((cursor_pos.x + text_offset.x) > (text_clip_rect.pos.x + text_clip_rect.size.x)) {
                gui.edit.offset++;
                text_offset = { gui.edit.offset < quads.count ? -quads[gui.edit.offset].x0 : 0.0f, 0.0f };
            }

            while ((cursor_pos.x + text_offset.x < text_clip_rect.pos.x)) {
                gui.edit.offset--;
                text_offset = { gui.edit.offset < quads.count ? -quads[gui.edit.offset].x0 : 0.0f, 0.0f };
            }
            
            if (gui.edit.selection != gui.edit.cursor) {
                f32 x0 = cursor_pos.x;
                f32 x1 = pos.x + 1.0f;
                
                i32 selection_ci = codepoint_index_from_byte_index(str, gui.edit.selection);
                x1 += selection_ci < quads.count ? quads[selection_ci].x0 : quads[selection_ci-1].x1;
                
                if (x0 > x1) SWAP(x0, x1);

                gui_draw_rect(
                    cmdbuf, 
                    Vector2{ x0, cursor_pos.y } + text_offset, 
                    Vector2{ x1-x0, gui.style.text.font.line_height }, 
                    wnd->clip_rect, 
                    selection_bg);
            }
        }

        gui_draw_text(cmdbuf, slice(quads, offset), edit_pos + text_offset, text_clip_rect, edit_fg, &gui.style.text.font);
    }

    if (gui.active == id) {
        gui_draw_rect(
            cmdbuf, 
            cursor_pos + text_offset, 
            Vector2{ 1.0f, gui.style.text.font.line_height }, 
            wnd->clip_rect, edit_fg);
    }
    
    return (GuiEditboxAction)action;
}

GuiEditboxAction gui_editbox_id(GuiId id, String in_str, Vector2 size)
{
    Vector2 pos = gui_layout_widget(size);
    return gui_editbox_id(id, in_str, pos, size);
}

GuiEditboxAction gui_editbox_id(GuiId id, String label, String in_str, Vector2 size)
{
    f32 margin = 5.0f;
    
    GuiWindow *wnd = &gui.windows[gui.current_window];
    GfxCommandBuffer *cmdbuf = &wnd->command_buffer;

    TextQuadsAndBounds td = calc_text_quads_and_bounds(label, &gui.style.text.font);
    Vector2 pos = gui_layout_widget({ td.bounds.size.x + size.x + margin, MAX3(td.bounds.size.y, size.y, gui.style.text.font.line_height) });
    gui_draw_text(cmdbuf, td.quads, pos, wnd->clip_rect, { 1.0f, 1.0f, 1.0f }, &gui.style.text.font);
    return gui_editbox_id(id, in_str, { pos.x + td.bounds.size.x + margin, pos.y }, size);
}

GuiEditboxAction gui_editbox_id(GuiId id, f32 *value, Vector2 size)
{
    char buffer[100];

    String str = gui.active == id ? 
        String{ gui.edit.buffer, gui.edit.length } : 
        stringf(buffer, sizeof buffer, "%f", *value);
    
    GuiEditboxAction action = gui_editbox_id(id, str, size);
    if (action & GUI_EDITBOX_FINISH) f32_from_string({ gui.edit.buffer, gui.edit.length }, value);
    
    return action;
}

GuiEditboxAction gui_editbox_id(GuiId id, f32 *value, Vector2 pos, Vector2 size)
{
    char buffer[100];

    String str = gui.active == id ? 
        String{ gui.edit.buffer, gui.edit.length } : 
        stringf(buffer, sizeof buffer, "%f", *value);

    GuiEditboxAction action = gui_editbox_id(id, str, pos, size);
    if (action & GUI_EDITBOX_FINISH) f32_from_string({ gui.edit.buffer, gui.edit.length }, value);

    return action;
}


GuiEditboxAction gui_editbox_id(GuiId id, String label, f32 *value, Vector2 size)
{
    f32 margin = 5.0f;

    GuiWindow *wnd = &gui.windows[gui.current_window];
    GfxCommandBuffer *cmdbuf = &wnd->command_buffer;

    TextQuadsAndBounds td = calc_text_quads_and_bounds(label, &gui.style.text.font);
    Vector2 pos = gui_layout_widget({ td.bounds.size.x + size.x + margin, MAX3(td.bounds.size.y, size.y, gui.style.text.font.line_height) });
    gui_draw_text(cmdbuf, td.quads, pos, wnd->clip_rect, { 1.0f, 1.0f, 1.0f }, &gui.style.text.font);
    return gui_editbox_id(id, value, Vector2{ pos.x + td.bounds.size.x + margin, pos.y }, size);
}

i32 find_or_create_window(GuiId id)
{
    for (i32 i = 0; i < gui.windows.count; i++) {
        if (gui.windows[i].id == id) {
            return i;
        }
    }

    GuiWindow wnd;
    wnd.id = id;
    wnd.command_buffer = {};
    wnd.command_buffer.commands.alloc = mem_frame;
    return array_add(&gui.windows, wnd);
}

bool gui_window_close_button(GuiId id, Vector2 wnd_pos, Vector2 wnd_size, bool *visible)
{
    i32 index = find_or_create_window(id);
    GuiWindow *wnd = &gui.windows[index];
    
    GuiId close_id = GUI_ID_INTERNAL(id, 3);

    Vector2 close_size{ 20, 20 };
    Vector2 close_pos{ wnd_pos.x + wnd_size.x - close_size.x, wnd_pos.y + 1.0f };
    gui_hot_rect(close_id, close_pos, close_size);
    
    Vector2 icon_size{ 16, 16 };
    Vector2 icon_pos = close_pos + (close_size - icon_size) * 0.5f;

    if (gui_active_click(close_id)) {
        *visible = false;
        gui_hot(GUI_ID_INVALID);
        gui_active(GUI_ID_INVALID);
    }
    
    if (gui.hot == close_id) {
        gui_draw_rect(&wnd->command_buffer, close_pos, close_size, wnd->clip_rect, rgb_unpack(0xFFFF0000));
    }
    
    gui_draw_rect(&wnd->command_buffer, icon_pos, icon_size, wnd->clip_rect, gui.style.icons.close);
    return true;
}

bool gui_begin_window_id(
    GuiId id, 
    String title, 
    Vector2 pos, 
    Vector2 size,
    bool *visible)
{
    if (!*visible) return false;
    ASSERT(gui.current_window == 1);
    gui_begin_window_id(id, title, pos, size);
    return gui_window_close_button(id, pos, size, visible);
}

bool gui_begin_window_id(
    GuiId id, 
    String title, 
    Vector2 *pos, 
    Vector2 *size,
    bool *visible)
{
    
    if (!*visible) return false;
    ASSERT(gui.current_window == 1);
    gui_begin_window_id(id, title, pos, size);
    return gui_window_close_button(id, *pos, *size, visible);
}

bool gui_begin_window_id(
    GuiId id, 
    String title, 
    Vector2 *pos, 
    Vector2 *size)
{
    GuiId title_id = GUI_ID_INTERNAL(id, 1);
    
    gui_begin_window_id(id, title, *pos, *size);
    
    gui.current_window_data.pos = *pos;
    gui.current_window_data.size = size;
    
    if (gui.hot == title_id) {
        if (gui.active != title_id && gui.mouse.left_pressed && !gui.mouse.left_was_pressed) {
            gui_active(title_id);
            gui_drag_start(*pos);
        } else if (gui.active == title_id && !gui.mouse.left_pressed) {
            gui_active(GUI_ID_INVALID);
        } else if (gui.active == title_id && (gui.mouse.dx != 0 || gui.mouse.dy != 0)) {
            Vector2 mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };
            *pos = gui.drag_start_data + mouse - gui.drag_start_mouse;
        }
    } 
    
    return true;
}

bool gui_begin_window_id(GuiId id, String title, GuiWindowState *state)
{
    return gui_begin_window_id(id, title, &state->pos, &state->size, &state->active);
}

bool gui_begin_window_id(
    GuiId id, 
    String title, 
    Vector2 pos, 
    Vector2 size)
{
    ASSERT(gui.current_window == 1);
    
    i32 index = find_or_create_window(id);
    GuiWindow *wnd = &gui.windows[index];
    gui.current_window = index;
    
    Vector2 window_border = { 1.0f, 1.0f };
    Vector2 title_size = { size.x - 2.0f*window_border.x, 21.0f };

    GuiId title_id = GUI_ID_INTERNAL(id, 1);

    if (gui_hot_rect(id, pos, size)) {
        if (gui.hot == id &&
            gui.mouse.left_pressed && 
            !gui.mouse.left_was_pressed) 
        {
            wnd = push_window_to_top(index);
            gui.active_window = id;
        }
    } else {
        if (gui.active_window == id && 
            gui.mouse.left_pressed && 
            !gui.mouse.left_was_pressed) 
        {
            gui.active_window = GUI_ID_INVALID;
        }
    }
    
    if (gui.active == title_id) gui_hot(title_id, index);
    else gui_hot_rect(title_id, pos, title_size);

    GuiLayout layout{
        .type = GUI_LAYOUT_ROW,
        .pos = pos + window_border + Vector2{ 2.0f, title_size.y + 2.0f },
        .size = size - 2.0f*window_border - Vector2{ 2.0f, title_size.y + 2.0f },
        .row.margin = 2.0f
    };
    gui_begin_layout(layout);

    wnd->clip_rect = Rect{ layout.pos, layout.size };
    Vector3 title_bg = gui.active_window != id
        ? rgb_unpack(0xFF212121)
        : gui.hot == title_id ? rgb_unpack(0xFFFD8433) : rgb_unpack(0xFFCA5100);
    
    Vector2 title_pos = pos + window_border + Vector2{ 2.0f, 2.0f };
    Vector3 wnd_bg = rgb_unpack(0xFF282828);

    gui_draw_rect(&wnd->command_buffer, pos, size, title_bg);
    gui_draw_rect(&wnd->command_buffer, pos + window_border, size - 2.0f*window_border, wnd_bg);
    gui_draw_rect(&wnd->command_buffer, pos + window_border, title_size, title_bg);
    gui_draw_text(&wnd->command_buffer, title, title_pos, { title_pos, title_size }, { 1.0f, 1.0f, 1.0f }, &gui.style.text.font);
    
    return true;
}

void gui_end_window()
{
    ASSERT(gui.current_window > 1);
    ASSERT(gui.layout_stack.count > 1);

    if (gui.current_window_data.size) {
        // NOTE(jesper): we handle the window resizing at the end to ensure it's always on
        // top and has the highest input priority in the window.
        GuiWindow *wnd = &gui.windows[gui.current_window];

        Vector2 window_border = { 1.0f, 1.0f };
        Vector2 title_size = { gui.current_window_data.size->x - 2.0f*window_border.x, 20.0f };
        Vector2 min_size = { 100.0f, title_size.y + 2.0f*window_border.y };

        GuiId resize_id = GUI_ID_INTERNAL(wnd->id, 2);

        Vector2 mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };

        Vector2 br = gui.current_window_data.pos + *gui.current_window_data.size - window_border;
        Vector2 resize_tr{ br.x, br.y - 10.0f };
        Vector2 resize_br{ br.x, br.y };

        Vector2 resize_bl{ br.x - 10.0f, br.y };

        if (gui.active == resize_id ||
            point_in_triangle(mouse, resize_tr, resize_br, resize_bl))
        {
            gui_hot(resize_id, gui.current_window);
        } else if (gui.hot == resize_id) {
            gui_hot(GUI_ID_INVALID);
        }

        if (gui.hot == resize_id) {
            if (gui.active != resize_id && gui.mouse.left_pressed && !gui.mouse.left_was_pressed) {
                gui_active(resize_id);
                gui_drag_start(*gui.current_window_data.size);
            } else if (gui.active == resize_id && !gui.mouse.left_pressed) {
                gui_active(GUI_ID_INVALID);
            } else if (gui.active == resize_id && (gui.mouse.dx != 0 || gui.mouse.dy != 0)) {
                Vector2 nsize = gui.drag_start_data + mouse - gui.drag_start_mouse;
                gui.current_window_data.size->x = MAX(min_size.x, nsize.x);
                gui.current_window_data.size->y = MAX(min_size.y, nsize.y);

                br = gui.current_window_data.pos + *gui.current_window_data.size - window_border;
                resize_tr = { br.x, br.y - 10.0f };
                resize_br = { br.x, br.y };
                resize_bl = { br.x - 10.0f, br.y };
            }
        }

        Vector3 resize_bg = gui.hot == resize_id ? rgb_unpack(0xFFFFFFFF) : rgb_unpack(0xFF5B5B5B);
        gfx_draw_triangle(resize_tr, resize_br, resize_bl, resize_bg, &wnd->command_buffer);

        gui.current_window_data.size = nullptr;
    }

    gui_end_layout();
    gui.current_window = 1;
}

Vector2 gui_layout_widget(Vector2 size, GuiAnchor anchor)
{
    GuiLayout *cl = gui_current_layout();
    Vector2 pos = cl->pos;
    
    switch (cl->type) {
    case GUI_LAYOUT_ROW:
        switch (anchor) {
        case GUI_ANCHOR_TOP:
            pos += cl->current;
            cl->current.y += size.y + cl->row.margin;
            cl->available_space.y -=  size.y + cl->row.margin;
            break;
        case GUI_ANCHOR_BOTTOM:
            pos.x += cl->current.x;
            pos.y += cl->current.y + cl->available_space.y - size.y;
            cl->available_space.y -= size.y + cl->row.margin;
            break;
        }
        break;
    case GUI_LAYOUT_COLUMN:
        switch (anchor) {
        case GUI_ANCHOR_LEFT:
            pos += cl->current;
            cl->current.x += size.x + cl->column.margin;
            cl->available_space.x -= size.x + cl->column.margin;
            break;
        case GUI_ANCHOR_RIGHT:
            pos.x += cl->current.x + cl->available_space.x - size.x;
            pos.y += cl->current.y;
            cl->available_space.x -= size.x + cl->column.margin;
            break;
        }
        break;
    case GUI_LAYOUT_ABSOLUTE:
        break;
    }
    
    return pos;
}

bool gui_active_parent(GuiId id)
{
    return gui.active == id || gui.current_id.owner == id.owner;
}

Vector2 gui_layout_widget(Vector2 *required_size)
{
    GuiLayout *layout = gui_current_layout();

    Vector2 pos = gui_layout_widget(*required_size);
    switch (layout->type) {
    case GUI_LAYOUT_ROW:
        required_size->x = layout->size.x;
        break;
    case GUI_LAYOUT_COLUMN:
        required_size->y = layout->size.y;
        break;
    case GUI_LAYOUT_ABSOLUTE:
        break;
    }

    return pos;
}


GuiGizmoAction gui_2d_gizmo_translate_axis_id(GuiId id, Camera camera, Vector2 *position, Vector2 axis)
{
    GuiGizmoAction action = GUI_GIZMO_NONE;
    
    // TODO(jesper): these wigets need to support a rotated and translated camera
    GfxCommandBuffer *cmdbuf = &gui.windows[0].command_buffer;
    
    Vector2 n{ axis.y, -axis.x };
    
    // base
    Vector2 br = *position + 0.5f*GIZMO_BAR_SIZE.y*n + axis*GIZMO_BAR_SIZE.x;
    Vector2 tr = *position - 0.5f*GIZMO_BAR_SIZE.y*n + axis*GIZMO_BAR_SIZE.x;
    Vector2 tl = *position - 0.5f*GIZMO_BAR_SIZE.y*n;
    Vector2 bl = *position + 0.5f*GIZMO_BAR_SIZE.y*n;

    // tip
    // NOTE(jesper): -1 axis to cause a 1px overlap in base and tip to not cause
    // any wonkyness with mouse dragging across the arrow
    Vector2 t0 = *position + axis*GIZMO_BAR_SIZE.x - 0.5f*GIZMO_TIP_SIZE.y*n - axis;
    Vector2 t1 = *position + axis*GIZMO_BAR_SIZE.x + axis*GIZMO_TIP_SIZE.x - axis;
    Vector2 t2 = *position + axis*GIZMO_BAR_SIZE.x + 0.5f*GIZMO_TIP_SIZE.y*n - axis;

    Vector2 mouse = ws_from_ss(Vector2{ (f32)gui.mouse.x, (f32)gui.mouse.y } + camera.position.xy);

    if (gui.active == id && !gui.mouse.left_pressed) {
        gui_active(GUI_ID_INVALID);
        action = GUI_GIZMO_END;
    }
    
    if (gui.active == id ||
        point_in_rect(mouse, tl, tr, br, tl) ||
        point_in_triangle(mouse, t0, t1, t2))
    {
        gui_hot(id, -1);
    } else if (gui.hot == id) {
        gui_hot(GUI_ID_INVALID);
    }
    
    if (gui.active != id &&
        gui.hot == id && 
        gui.mouse.left_pressed && 
        !gui.mouse.left_was_pressed) 
    {
        gui_active(id);
        action = GUI_GIZMO_BEGIN;

        gui_drag_start(*position);
    }
    
    if (gui.active == id) {
        Vector2 d = mouse - gui.drag_start_mouse;
        *position = gui.drag_start_data + dot(axis, d) * axis;
    }

    Vector3 color{ .rg = absv(axis), ._b = 0 };
    if (gui.hot == id) color = 0.5f*(color + Vector3{ 1.0f, 1.0f, 1.0f });
    
    gfx_draw_square(tl, tr, br, bl, color, cmdbuf);
    gfx_draw_triangle(t0, t1, t2, color, cmdbuf);
    
    Vector2 points[] = { tl, tr, t0, t1, t2, br, bl };
    gfx_draw_line_loop(points, ARRAY_COUNT(points), GIZMO_OUTLINE_COLOR, cmdbuf);
    return action;
}

GuiGizmoAction gui_2d_gizmo_size_axis_id(GuiId id, Camera camera, Vector2 position, Vector2 *size, Vector2 axis)
{
    GuiGizmoAction action = GUI_GIZMO_NONE;
    
    // TODO(jesper): these wigets need to support a rotated and translated camera
    GfxCommandBuffer *cmdbuf = &gui.windows[0].command_buffer;

    Vector2 n{ axis.y, -axis.x };

    // TODO(jesper): this is a but ugly to do a yflip on the visual representation of the axis, needed because
    // of the y-flipped mouse vs gl-coordinates and the way I do the size calculations
    Vector2 vaxis{ axis.x, -axis.y };

    // base
    Vector2 br = position + 0.5f*GIZMO_BAR_SIZE.y*n + vaxis*GIZMO_BAR_SIZE.x;
    Vector2 tr = position - 0.5f*GIZMO_BAR_SIZE.y*n + vaxis*GIZMO_BAR_SIZE.x;
    Vector2 tl = position - 0.5f*GIZMO_BAR_SIZE.y*n;
    Vector2 bl = position + 0.5f*GIZMO_BAR_SIZE.y*n;
    
    // tip
    // NOTE(jesper): -1 axis to cause a 1px overlap in base and tip to not cause
    // any wonkyness with mouse dragging across the arrow
    Vector2 t0 = position + vaxis*GIZMO_BAR_SIZE.x - 0.5f*GIZMO_TIP_SIZE.y*n - axis;
    Vector2 t1 = position + vaxis*GIZMO_BAR_SIZE.x + 0.5f*GIZMO_TIP_SIZE.y*n - axis;
    Vector2 t2 = t1 + vaxis*GIZMO_TIP_SIZE.x;
    Vector2 t3 = t0 + vaxis*GIZMO_TIP_SIZE.x;

    Vector2 mouse = ws_from_ss(Vector2{ (f32)gui.mouse.x, (f32)gui.mouse.y } + camera.position.xy);

    if (gui.active == id && !gui.mouse.left_pressed) {
        gui_active(GUI_ID_INVALID);
        action = GUI_GIZMO_END;
    }

    if (gui.active == id ||
        point_in_rect(mouse, tl, tr, br, bl) ||
        point_in_rect(mouse, t0, t1, t2, t3))
    {
        gui_hot(id, -1);
    } else if (gui.hot == id) {
        gui_hot(GUI_ID_INVALID);
    }

    if (gui.active != id &&
        gui.hot == id && 
        gui.mouse.left_pressed && 
        !gui.mouse.left_was_pressed) 
    {
        gui_active(id);
        action = GUI_GIZMO_BEGIN;
        gui_drag_start(*size);
    }

    if (gui.active == id) {
        Vector2 delta = axis*dot(mouse, axis) - axis*dot(gui.drag_start_mouse, axis);
        delta.y = -delta.y;
        *size = gui.drag_start_data + delta;
    }

    Vector3 color{ .rg = absv(axis), ._b = 0 };
    if (gui.hot == id) color = 0.5f*(color + Vector3{ 1.0f, 1.0f, 1.0f });

    gfx_draw_square(tl, tr, br, bl, color, cmdbuf);
    gfx_draw_square(t0, t1, t2, t3, color, cmdbuf);
    
    Vector2 points[] = { tl, tr, t0, t3, t2, t1, br, bl };
    gfx_draw_line_loop(points, ARRAY_COUNT(points), GIZMO_OUTLINE_COLOR, cmdbuf);
    
    return action;
}

GuiGizmoAction gui_2d_gizmo_size_axis_id(GuiId id, Camera camera, Vector2 position, Vector2 *size, Vector2 axis, f32 multiple)
{
    GuiGizmoAction action = GUI_GIZMO_NONE;

    // TODO(jesper): these wigets need to support a rotated camera
    GfxCommandBuffer *cmdbuf = &gui.windows[0].command_buffer;

    Vector2 n{ axis.y, -axis.x };

    // TODO(jesper): this is a bit ugly to do a yflip on the visual representation of the axis, needed because
    // of the y-flipped mouse vs gl-coordinates and the way I do the size calculations
    Vector2 vaxis{ axis.x, -axis.y };

    // base
    Vector2 br = position + 0.5f*GIZMO_BAR_SIZE.y*n + vaxis*GIZMO_BAR_SIZE.x;
    Vector2 tr = position - 0.5f*GIZMO_BAR_SIZE.y*n + vaxis*GIZMO_BAR_SIZE.x;
    Vector2 tl = position - 0.5f*GIZMO_BAR_SIZE.y*n;
    Vector2 bl = position + 0.5f*GIZMO_BAR_SIZE.y*n;

    // tip
    // NOTE(jesper): -1 axis to cause a 1px overlap in base and tip to not cause
    // any wonkyness with mouse dragging across the arrow
    Vector2 t0 = position + vaxis*GIZMO_BAR_SIZE.x - 0.5f*GIZMO_TIP_SIZE.y*n - vaxis;
    Vector2 t1 = position + vaxis*GIZMO_BAR_SIZE.x + 0.5f*GIZMO_TIP_SIZE.y*n - vaxis;
    Vector2 t2 = t1 + vaxis*GIZMO_TIP_SIZE.x;
    Vector2 t3 = t0 + vaxis*GIZMO_TIP_SIZE.x;

    Vector2 mouse = ws_from_ss(Vector2{ (f32)gui.mouse.x, (f32)gui.mouse.y } + camera.position.xy);

    if (gui.active == id && !gui.mouse.left_pressed) {
        gui_active(GUI_ID_INVALID);
        action = GUI_GIZMO_END;
    }

    if (gui.active == id ||
        point_in_rect(mouse, tl, tr, br, bl) ||
        point_in_rect(mouse, t0, t1, t2, t3))
    {
        gui_hot(id, -1);
    } else if (gui.hot == id) {
        gui_hot(GUI_ID_INVALID);
    }

    if (gui.active != id &&
        gui.hot == id && 
        gui.mouse.left_pressed && 
        !gui.mouse.left_was_pressed) 
    {
        gui_active(id);
        action = GUI_GIZMO_BEGIN;
        gui_drag_start(*size);
    }

    if (gui.active == id) {
        Vector2 delta = axis*dot(mouse, axis) - axis*dot(gui.drag_start_mouse, axis);
        delta.x = round_to(delta.x, multiple);
        delta.y = round_to(delta.y, multiple);
        delta.y = -delta.y;
        *size = gui.drag_start_data + delta;
    }

    Vector3 color{ .rg = absv(axis), ._b = 0 };
    if (gui.hot == id) color = 0.5f*(color + Vector3{ 1.0f, 1.0f, 1.0f });

    gfx_draw_square(tl, tr, br, bl, color, cmdbuf);
    gfx_draw_square(t0, t1, t2, t3, color, cmdbuf);

    Vector2 points[] = { tl, tr, t0, t3, t2, t1, br, bl };
    gfx_draw_line_loop(points, ARRAY_COUNT(points), GIZMO_OUTLINE_COLOR, cmdbuf);

    return action;
}

GuiGizmoAction gui_2d_gizmo_translate_plane_id(GuiId id, Camera camera, Vector2 *position)
{
    GuiGizmoAction action = GUI_GIZMO_NONE;
    
    // TODO(jesper): these wigets need to support a rotated camera
    GfxCommandBuffer *cmdbuf = &gui.windows[0].command_buffer;

    Vector2 size{ 20.0f, 20.0f };
    
    Vector2 offset{ 10.0f, -10.0f };
    Vector2 br = *position + offset + Vector2{ size.x, 0.0f };
    Vector2 tr = *position + offset + Vector2{ size.x, -size.y };
    Vector2 tl = *position + offset + Vector2{ 0.0f, -size.y };
    Vector2 bl = *position + offset;

    Vector2 mouse = ws_from_ss(Vector2{ (f32)gui.mouse.x, (f32)gui.mouse.y } + camera.position.xy);

    if (gui.active == id && !gui.mouse.left_pressed) {
        gui_active(GUI_ID_INVALID);
        action = GUI_GIZMO_END;
    }

    if (gui.active == id || point_in_rect(mouse, tl, tr, br, bl)) {
        gui_hot(id, -1);
    } else if (gui.hot == id) {
        gui_hot(GUI_ID_INVALID);
    }

    if (gui.active != id &&
        gui.hot == id && 
        gui.mouse.left_pressed && 
        !gui.mouse.left_was_pressed) 
    {
        gui_active(id);
        action = GUI_GIZMO_BEGIN;
        gui_drag_start(*position);
    } 

    if (gui.active == id) {
        Vector2 d = mouse - gui.drag_start_mouse;
        *position = gui.drag_start_data + d;
    }
    
    Vector3 color{ 0.5f, 0.5f, 0.0f };
    if (gui.hot == id) color = { 1.0f, 1.0f, 0.0f };
    
    Vector2 points[] = { tl, tr, br, bl };
    gfx_draw_square(tl, tr, br, bl, color, cmdbuf);
    gfx_draw_line_loop(points, ARRAY_COUNT(points), GIZMO_OUTLINE_COLOR, cmdbuf);
    return action;
}

GuiGizmoAction gui_2d_gizmo_translate_id(GuiId id, Camera camera, Vector2 *position)
{
    GuiGizmoAction a0 = gui_2d_gizmo_translate_axis_id(GUI_ID_INTERNAL(id, 1), camera, position, { 1.0f, 0.0f });
    GuiGizmoAction a1 = gui_2d_gizmo_translate_axis_id(GUI_ID_INTERNAL(id, 2), camera, position, { 0.0f, -1.0f });
    GuiGizmoAction a2 = gui_2d_gizmo_translate_plane_id(GUI_ID_INTERNAL(id, 3), camera, position);
    return (GuiGizmoAction)MAX(a0, MAX(a1, a2));
}

GuiGizmoAction gui_2d_gizmo_translate_id(GuiId id, Camera camera, Vector2 *position, f32 multiple)
{
    GuiGizmoAction a0 = gui_2d_gizmo_translate_axis_id(GUI_ID_INTERNAL(id, 1), camera, position, { 1.0f, 0.0f });
    GuiGizmoAction a1 = gui_2d_gizmo_translate_axis_id(GUI_ID_INTERNAL(id, 2), camera, position, { 0.0f, -1.0f });
    GuiGizmoAction a2 = gui_2d_gizmo_translate_plane_id(GUI_ID_INTERNAL(id, 3), camera, position);
    
    position->x = round_to(position->x, 0.5f*multiple);
    position->y = round_to(position->y, 0.5f*multiple);

    return (GuiGizmoAction)MAX(a0, MAX(a1, a2));
}


GuiGizmoAction gui_2d_gizmo_size_id(GuiId id, Camera camera, Vector2 position, Vector2 *size)
{
    GuiGizmoAction a0 = gui_2d_gizmo_size_axis_id(GUI_ID_INTERNAL(id, 1), camera, position, size, { 1.0f, 0.0f });
    GuiGizmoAction a1 = gui_2d_gizmo_size_axis_id(GUI_ID_INTERNAL(id, 2), camera, position, size, { 0.0f, 1.0f });
    return (GuiGizmoAction)MAX(a0, a1);
}

GuiGizmoAction gui_2d_gizmo_size_id(GuiId id, Camera camera, Vector2 position, Vector2 *size, f32 multiple)
{
    GuiGizmoAction a0 = gui_2d_gizmo_size_axis_id(GUI_ID_INTERNAL(id, 1), camera, position, size, { 1.0f, 0.0f }, multiple);
    GuiGizmoAction a1 = gui_2d_gizmo_size_axis_id(GUI_ID_INTERNAL(id, 2), camera, position, size, { 0.0f, 1.0f }, multiple);
    return (GuiGizmoAction)MAX(a0, a1);
}

GuiGizmoAction gui_2d_gizmo_size_square_id(GuiId parent, Camera camera, Vector2 *center, Vector2 *size)
{
    GuiGizmoAction action = GUI_GIZMO_NONE;
    
    GfxCommandBuffer *cmdbuf = &gui.windows[0].command_buffer;
    
    Vector2 half_size = 0.5f * *size;
    Vector2 tl = *center - half_size;
    Vector2 br = *center + half_size;
    Vector2 tr{ br.x, tl.y };
    Vector2 bl{ tl.x, br.y };
    
    Vector2 gizmo_size{ 10.0f, 10.0f };
    
    Vector2 mouse = ws_from_ss(Vector2{ (f32)gui.mouse.x, (f32)gui.mouse.y } + camera.position.xy);

    Vector2 corners[] = { tl, br, tr, bl };
    
    GuiId corner_ids[] = { 
        GUI_ID_INTERNAL(parent, 0),
        GUI_ID_INTERNAL(parent, 1),
        GUI_ID_INTERNAL(parent, 2),
        GUI_ID_INTERNAL(parent, 3)
    };

    for (i32 i = 0; i < ARRAY_COUNT(corners); i++) {
        GuiId id = corner_ids[i];
        
        if (gui.active == id && !gui.mouse.left_pressed) {
            gui_active(GUI_ID_INVALID);
            action = GUI_GIZMO_END;
        }
        
        if (gui.active == id || point_in_rect(mouse, corners[i], gizmo_size)) {
            gui_hot(id, -1);
        } else if (gui.hot == id) {
            gui_hot(GUI_ID_INVALID);
        }
        
        if (gui.active != id &&
            gui.hot == id && 
            gui.mouse.left_pressed && 
            !gui.mouse.left_was_pressed) 
        {
            gui_active(id);
            action = GUI_GIZMO_BEGIN;
            gui_drag_start(*size, *center);
        }
        
        Vector3 color{ 0.5f, 0.5f, 0.5f };
        if (gui.hot == id) color = { 1.0f, 1.0f, 1.0f };
        
        gfx_draw_square(corners[i], gizmo_size, Vector4{ .rgb = color, ._a = 1.0f }, cmdbuf);
    }
    
    if (gui.active.owner == parent.owner && gui.active.index == parent.index) {
        Vector2 delta = mouse - gui.drag_start_mouse;
        
        if (gui.active == corner_ids[1] || gui.active == corner_ids[2]) {
            size->x = gui.drag_start_data.x + delta.x;
            center->x = gui.drag_start_data1.x - 0.5f*gui.drag_start_data.x + 0.5f*size->x; 
        } else {
            size->x = gui.drag_start_data.x - delta.x;
            center->x = gui.drag_start_data1.x + 0.5f*gui.drag_start_data.x - 0.5f*size->x;
        }
        
        if (gui.active == corner_ids[0] || gui.active == corner_ids[2]) {
            size->y = gui.drag_start_data.y - delta.y;
            center->y = gui.drag_start_data1.y + 0.5f*gui.drag_start_data.y - 0.5f*size->y;
        } else {
            size->y = gui.drag_start_data.y + delta.y;
            center->y = gui.drag_start_data1.y - 0.5f*gui.drag_start_data.y + 0.5f*size->y;
        }
    }

    return action;
}

GuiGizmoAction gui_2d_gizmo_size_square_id(GuiId parent, Camera camera, Vector2 *center, Vector2 *size, f32 multiple)
{
    GuiGizmoAction action = GUI_GIZMO_NONE;

    GfxCommandBuffer *cmdbuf = &gui.windows[0].command_buffer;

    Vector2 half_size = 0.5f * *size;
    Vector2 tl = *center - half_size;
    Vector2 br = *center + half_size;
    Vector2 tr{ br.x, tl.y };
    Vector2 bl{ tl.x, br.y };

    Vector2 gizmo_size{ 10.0f, 10.0f };

    Vector2 mouse = ws_from_ss(Vector2{ (f32)gui.mouse.x, (f32)gui.mouse.y } + camera.position.xy);

    Vector2 corners[] = { tl, br, tr, bl };

    GuiId corner_ids[] = { 
        GUI_ID_INTERNAL(parent, 0),
        GUI_ID_INTERNAL(parent, 1),
        GUI_ID_INTERNAL(parent, 2),
        GUI_ID_INTERNAL(parent, 3)
    };
    
    bool corner_gizmo_active = false;
    for (i32 i = 0; i < ARRAY_COUNT(corners); i++) {
        GuiId id = corner_ids[i];

        if (gui.active == id && !gui.mouse.left_pressed) {
            gui_active(GUI_ID_INVALID);
            action = gui.drag_start_data != *size || gui.drag_start_data1 != *center ? GUI_GIZMO_END : GUI_GIZMO_CANCEL;
        }

        if (gui.active == id || point_in_rect(mouse, corners[i], gizmo_size)) {
            gui_hot(id, -1);
        } else if (gui.hot == id) {
            gui_hot(GUI_ID_INVALID);
        }

        if (gui.active != id &&
            gui.hot == id && 
            gui.mouse.left_pressed && 
            !gui.mouse.left_was_pressed) 
        {
            gui_active(id);
            action = GUI_GIZMO_BEGIN;
            
            center->x = round_to(center->x, 0.5f*multiple);
            center->y = round_to(center->y, 0.5f*multiple);
            
            size->x = round_to(size->x, multiple);
            size->y = round_to(size->y, multiple);
            gui_drag_start(*size, *center);
        }
        
        if (gui.active == id) corner_gizmo_active = true;

        Vector3 color{ 0.5f, 0.5f, 0.0f };
        if (gui.hot == id) color = { 1.0f, 1.0f, 0.0f };

        gfx_draw_square(corners[i], gizmo_size, Vector4{ .rgb = color, ._a = 1.0f }, cmdbuf);
        gfx_draw_line_square(corners[i], gizmo_size, GIZMO_OUTLINE_COLOR, cmdbuf);
    }

    if (corner_gizmo_active) {
        Vector2 delta = mouse - gui.drag_start_mouse;
        delta.x = round_to(delta.x, multiple);
        delta.y = round_to(delta.y, multiple);

        if (gui.active == corner_ids[1] || gui.active == corner_ids[2]) {
            size->x = gui.drag_start_data.x + delta.x;
            center->x = gui.drag_start_data1.x - 0.5f*gui.drag_start_data.x + 0.5f*size->x; 
        } else {
            size->x = gui.drag_start_data.x - delta.x;
            center->x = gui.drag_start_data1.x + 0.5f*gui.drag_start_data.x - 0.5f*size->x;
        }

        if (gui.active == corner_ids[0] || gui.active == corner_ids[2]) {
            size->y = gui.drag_start_data.y - delta.y;
            center->y = gui.drag_start_data1.y + 0.5f*gui.drag_start_data.y - 0.5f*size->y;
        } else {
            size->y = gui.drag_start_data.y + delta.y;
            center->y = gui.drag_start_data1.y - 0.5f*gui.drag_start_data.y + 0.5f*size->y;
        }
    }
    
    GuiGizmoAction a0 = gui_2d_gizmo_size_axis_id(GUI_ID_INTERNAL(parent, 4), camera, *center, size, { 1.0f, 0.0f }, multiple);
    GuiGizmoAction a1 = gui_2d_gizmo_size_axis_id(GUI_ID_INTERNAL(parent, 5), camera, *center, size, { 0.0f, 1.0f }, multiple);
    action = (GuiGizmoAction)MAX(action, MAX(a0, a1));
    
    return action;
}

template<typename T>
void gui_dropdown_id(GuiId id, Array<String> labels, Array<T> values, T *value)
{
    ASSERT(labels.count > 0);
    ASSERT(labels.count == values.count);
    
    GuiWindow *wnd = &gui.windows[gui.current_window];
    GfxCommandBuffer *cmdbuf = &wnd->command_buffer;
    
    i32 current_index = array_find_index(values, *value);
    ASSERT(current_index != -1);
    
    f32 margin = 4.0f;
    f32 border = 1.0f;
    
    Vector2 total_size{ 100.0f + margin + border, gui.font.line_height + margin };

    f32 rhs = total_size.y;
    f32 rhs_margin = 6.0f;
    
    Vector2 inner_size{ total_size.x - 2*border, total_size.y - 2*border };

    Vector2 pos = gui_layout_widget(total_size);
    Vector2 inner_pos{ pos.x + border, pos.y + border };
    Vector2 text_pos{ inner_pos.x + 0.5f*margin, inner_pos.y + 0.5f*margin };
    
    gui_hot_rect(id, pos, total_size);
    gui_active_press(id);
    
    Vector2 rhs_p{ inner_pos.x + inner_size.x - rhs, inner_pos.y };
    Vector2 rhs_p0{ rhs_p.x + border + rhs_margin, inner_pos.y + rhs_margin };
    Vector2 rhs_p1{ rhs_p0.x + (rhs-2.0f*rhs_margin), rhs_p0.y };
    Vector2 rhs_p2{ rhs_p0.x + 0.5f*(rhs_p1.x - rhs_p0.x), rhs_p0.y + (rhs-2.0f*rhs_margin) };
    
    Vector3 bg_def = rgb_unpack(0xFF1d2021);
    Vector3 bg_hot = rgb_unpack(0xFF3A3A3A);
    Vector3 border_col = rgb_unpack(0xFFCCCCCC);
    Vector3 rhs_col = gui.hot == id ? rgb_unpack(0xFFFFFFFF) : rgb_unpack(0xFF5B5B5B);
    Vector3 bg_col = gui.hot == id ? bg_hot : bg_def;
    
    gui_draw_rect(cmdbuf, pos, total_size, wnd->clip_rect, border_col);
    gui_draw_rect(cmdbuf, inner_pos, inner_size, wnd->clip_rect, bg_col);
    gui_draw_text(cmdbuf, labels[current_index], text_pos, wnd->clip_rect, { 1.0f, 1.0f, 1.0f });
    gui_draw_rect(cmdbuf, rhs_p, { border, inner_size.y }, border_col);
    gfx_draw_triangle(rhs_p0, rhs_p1, rhs_p2, rhs_col, cmdbuf);

    if (gui.active.owner == id.owner && gui.active.index == id.index) {
        Vector2 p{ pos.x, pos.y + total_size.y };
        Vector2 inner_p{ p.x + border, p.y };
        Vector2 text_offset{ 0.5f*margin + border, 0.5f*margin };
        Vector2 text_p = p + text_offset;
        
        Vector2 total_s{ total_size.x, labels.count * gui.font.line_height + border + 0.5f*margin };
        Vector2 inner_s{ total_s.x - 2*border, total_s.y - border };
        
        gui_draw_rect(&gui.windows[1].command_buffer, p, total_s, gui.windows[0].clip_rect, border_col);
        gui_draw_rect(&gui.windows[1].command_buffer, inner_p, inner_s, gui.windows[0].clip_rect, bg_def);
        
        for (i32 i = 0; i < labels.count; i++) {
            String label = labels[i];
            GuiId label_id = GUI_ID_INTERNAL(id, i);
            
            Vector2 rect_p{ inner_p.x, inner_p.y + gui.font.line_height*i };
            Vector2 rect_s{ inner_s.x, gui.font.line_height + border };
            gui_hot_rect(label_id, rect_p, rect_s);

            if (gui.hot == label_id) gui_draw_rect(&gui.windows[1].command_buffer, rect_p, rect_s, bg_hot);
            gui_draw_text(&gui.windows[1].command_buffer, label, text_p, { rect_p, rect_s }, { 1.0f, 1.0f, 1.0f });
            text_p.y += gui.font.line_height;
            
            if ((gui.hot == label_id && gui.active_press == id && !gui.mouse.left_pressed) || 
                gui_active_click(label_id)) 
            {
                *value = values[i];
                gui_active(GUI_ID_INVALID);
                gui_hot(GUI_ID_INVALID);
            }
        }
    }
    
    // TODO(jesper): this will probably be used elsewhere and should be factored. Come up with a good name for this
    if (gui.active == id &&
        gui.mouse.left_pressed && !gui.mouse.left_was_pressed &&
        gui.hot != id)
    {
        gui_active(GUI_ID_INVALID);
    }
}

template<typename T>
void gui_dropdown_id(GuiId id, String *labels, T *values, i32 count, T *value)
{
    Array<String> labels_arr = Array<String>{ labels, count };
    Array<T> values_arr = Array<T>{ values, count };
    gui_dropdown_id(id, labels_arr, values_arr, value);
}
    
template<typename T> 
T gui_dropdown_id(GuiId id, Array<String> labels, Array<T> values, T value)
{
    gui_dropdown_id(id, labels, values, &value);
    return value;
}

template<typename T> 
T gui_dropdown_id(GuiId id, String *labels, T *values, i32 count, T value)
{
    gui_dropdown_id(id, labels, values, count, &value);
    return value;
}

void gui_push_id(GuiId id)
{
    array_add(&gui.id_stack, gui.current_id);
    gui.current_id = id;
}

void gui_pop_id()
{
    ASSERT(gui.id_stack.count > 0);
    gui.current_id = gui.id_stack[gui.id_stack.count-1];
    gui.id_stack.count--;
}

bool gui_begin_menu_id(GuiId id)
{
    GuiWindow *wnd = &gui.windows[gui.current_window];
    GuiLayout *cl = gui_current_layout();

    Vector2 size{ cl->size.x, gui.style.text.font.line_height + 10.0f };
    Vector2 pos = gui_layout_widget(size);
    
    GuiLayout layout{
        .type = GUI_LAYOUT_COLUMN,
        .pos = pos,
        .size = size,
        .column.margin = 1.0f,
    };
    gui_begin_layout(layout);

    Vector3 bg = rgb_unpack(0xFF212121);
    gui_draw_rect(&wnd->command_buffer, layout.pos, layout.size, wnd->clip_rect, bg);
    
    gui_push_id(id);
    return true;
}

void gui_end_menu()
{
    GuiLayout *layout = gui_current_layout();
    
    if (layout->type == GUI_LAYOUT_ROW &&
        (gui.active == gui.current_id || gui.active.owner == gui.current_id.owner)) 
    {    
        Vector3 bg = rgb_unpack(0xFF212121);
        Vector2 size{ layout->size.x, layout->size.y + layout->current.y };
        gui_draw_rect(&gui.windows[1].command_buffer, layout->pos, size, bg, 0);
    }
    
    gui_end_layout();
    gui_pop_id();
}

bool gui_begin_menu_id(GuiId id, String label)
{
    // TODO(jesper): need to figure out how to do nested menus with id_stack and whatever else
    ASSERT(gui.id_stack.count <= 1);
    
    GuiWindow *wnd = &gui.windows[gui.current_window];
    GuiLayout *cl = gui_current_layout();
    
    TextQuadsAndBounds td = calc_text_quads_and_bounds(label, &gui.style.text.font);
    Vector2 text_offset{ 5.0f, 5.0f };
    
    Vector2 size{ td.bounds.size.x + 2.0f*text_offset.x, gui.style.text.font.line_height + 2.0f*text_offset.y };
    Vector2 pos = gui_layout_widget(&size);
    
    gui_hot_rect(id, pos, size);
    gui_active_press(id);
    
    // TODO(jesper): this will probably be used elsewhere and should be factored. Come up with a good name for this
    if (gui.active == id &&
        gui.mouse.left_pressed && !gui.mouse.left_was_pressed &&
        gui.hot.owner != id.owner)
    {
        gui_active(GUI_ID_INVALID);
    }
    
    Vector3 btn_fg = rgb_unpack(0xFFFFFFFF);
    Vector3 btn_bg = gui.hot == id ? rgb_unpack(0xFF282828) : rgb_unpack(0xFF212121);
    gui_draw_rect(&wnd->command_buffer, pos, size, wnd->clip_rect, btn_bg);
    gui_draw_text(&wnd->command_buffer, td.quads, pos + text_offset, wnd->clip_rect, btn_fg, &gui.style.text.font);
    
    if (gui.active == id || gui.active.owner == id.owner) {
        gui_push_id(id);
        gui.current_window = 1; // overlay
        
        Vector2 menu_pos = cl->type == GUI_LAYOUT_COLUMN ? 
            Vector2{ pos.x, pos.y + size.y } : 
            Vector2{ pos.x + cl->size.x, pos.y };

        GuiLayout layout{
            .type = GUI_LAYOUT_ROW,
            .pos = menu_pos,
            .size = { 200.0f, 0.0f },
            .column.margin = 1.0f,
        };

        gui_begin_layout(layout);
        return true;
    }
    
    return false;
}

bool gui_menu_button_id(GuiId id, String label)
{
    ASSERT(gui.current_id != GUI_ID_INVALID);
    id.internal = id.owner;
    id.owner = gui.current_id.owner;
    
    // TODO(jesper): can we just make this a regular button? only difference is styling
    GuiWindow *wnd = &gui.windows[gui.current_window];
    GfxCommandBuffer *cmdbuf = &wnd->command_buffer;
    
    TextQuadsAndBounds td = calc_text_quads_and_bounds(label, &gui.style.text.font);
    Vector2 text_offset{ 5.0f, 5.0f };
    
    Vector2 size{ td.bounds.size.x + 2.0f*text_offset.x, gui.style.text.font.line_height + 2.0f*text_offset.y };
    Vector2 pos = gui_layout_widget(&size);
    
    gui_hot_rect(id, pos, size);
    bool clicked = gui_active_click(id);
    
    Vector3 btn_fg = rgb_unpack(0xFFFFFFFF);
    Vector3 btn_bg = gui.hot == id ? rgb_unpack(0xFF282828) : rgb_unpack(0xFF212121);
    gui_draw_rect(cmdbuf, pos, size, wnd->clip_rect, btn_bg);
    gui_draw_text(cmdbuf, td.quads, pos + text_offset, wnd->clip_rect, btn_fg, &gui.style.button.font);
    return clicked;
}


bool gui_menu_button_id(GuiId id, String label, bool *toggle_value)
{
    bool clicked = gui_menu_button_id(id, label);
    if (clicked) *toggle_value = !*toggle_value;
    return clicked;
}


void gui_scrollbar_id(GuiId id, f32 line_height, i32 *current, i32 max, i32 num_visible, f32 *offset)
{
    GuiWindow *wnd = &gui.windows[gui.current_window];
    GuiLayout *cl = gui_current_layout();
    
    GuiId up_id = GUI_ID_INTERNAL(id, 1);
    GuiId dwn_id = GUI_ID_INTERNAL(id, 2);
    GuiId scroll_id = GUI_ID_INTERNAL(id, 3);
    
    // TODO(jesper): support left hand side scroll bars
    Vector2 total_size{ gui.style.scrollbar.thickness, cl->size.y };
    Vector2 total_pos = { cl->pos.x + cl->size.x - total_size.x, cl->pos.y };
    
    switch (cl->type) {
    case GUI_LAYOUT_ABSOLUTE:
        cl->size.x -= total_size.x;
        break;
    case GUI_LAYOUT_ROW:
    case GUI_LAYOUT_COLUMN:
        LOG_ERROR("unimplemented");
        break;
    }
    
    Vector2 btn_size{ gui.style.scrollbar.thickness, gui.style.scrollbar.thickness };
    Vector2 btn_up_p{ total_pos.x, total_pos.y };
    Vector2 btn_dwn_p{ total_pos.x, total_pos.y + total_size.y - btn_size.y };
    
    Vector2 scroll_size{ gui.style.scrollbar.thickness, total_size.y - btn_size.y*2 };
    Vector2 scroll_pos{ total_pos.x, total_pos.y + btn_up_p.y + btn_size.y };
    
    GfxCommandBuffer *cmdbuf = &wnd->command_buffer;
    gui_draw_rect(cmdbuf, total_pos, total_size, wnd->clip_rect, gui.style.scrollbar.bg);
    
    i32 step_size = 3;
    if (gui_button_id(up_id, btn_up_p, btn_size)) {
        *current -= step_size;
        *current = MAX(*current, 0);
    }

    if (gui_button_id(dwn_id, btn_dwn_p, btn_size)) {
        *current += step_size;
        *current = MIN(*current, max-3);
    }

    // NOTE(jesper): line_height*total_to_scroll != row_height
    // The row_height is line_height converted to [0, scroll_size.y] coordinate space,
    // but the scroll_to_total and total_to_scroll coordintae space factors convert 
    // to/from the space taken on the scroll bar _handle_
    f32 ratio = scroll_size.y/total_size.y;
    f32 row_height = line_height*ratio;
    
    f32 pixels_per_i = scroll_size.y / max;
    f32 scroll_to_total = row_height/pixels_per_i;
    f32 total_to_scroll = pixels_per_i/row_height;
    
    if (gui_active_drag(scroll_id, { *offset, (f32)*current }) && gui.mouse.dy != 0) {
        f32 dy = gui.mouse.y - gui.drag_start_mouse.y;
        
        f32 o = gui.drag_start_data.x*total_to_scroll + dy;
        i32 di = o / (line_height*total_to_scroll);
        o = (o - di*(line_height*total_to_scroll))*scroll_to_total;
        
        i32 c = di + (i32)gui.drag_start_data.y;
        c = CLAMP(c, 0, max-3);
        
        if (c == 0) o = MAX(o, 0);
        else if (c == max-3) o = MIN(o, 0);
        
        if (c > 0) {
            o += line_height;
            c -= 1;
        }
        
        *offset = o;
        *current = c;
    }
    
    f32 y0 = *current*pixels_per_i + *offset*total_to_scroll;
    f32 y1 = y0 + MIN(num_visible, max) * pixels_per_i;
    y1 = MIN(y1, scroll_pos.y + scroll_size.y - gui.style.scrollbar.thickness);

    Vector2 scroll_btn_p{ scroll_pos.x, scroll_pos.y + y0 };
    Vector2 scroll_btn_s{ gui.style.scrollbar.thickness, y1 - y0 };
    gui_hot_rect(scroll_id, scroll_btn_p, scroll_btn_s);

    Vector3 scroll_btn_bg = gui.active == scroll_id ? 
        gui.style.scrollbar.scroll_btn_active : 
        gui.hot == scroll_id ? 
        gui.style.scrollbar.scroll_btn_hot : 
        gui.style.scrollbar.scroll_btn;
    
    gui_draw_rect(cmdbuf, scroll_btn_p, scroll_btn_s, wnd->clip_rect, scroll_btn_bg);
}

Rect gui_layout_widget_fill()
{
    GuiLayout *cl = gui_current_layout();
    
    Rect r;
    r.pos = cl->pos + cl->current;
    r.size = cl->available_space;

    switch (cl->type) {
    case GUI_LAYOUT_ABSOLUTE:
        break;
    case GUI_LAYOUT_ROW:
    case GUI_LAYOUT_COLUMN:
        cl->available_space = { 0, 0 };
        cl->current = cl->pos + cl->size;
        break;
    }
    return r;
}

void gui_begin_layout(GuiLayout layout)
{
    layout.available_space = layout.size;
    array_add(&gui.layout_stack, layout);
}

void gui_end_layout()
{
    ASSERT(gui.layout_stack.count > 1);
    gui.layout_stack.count--;
}