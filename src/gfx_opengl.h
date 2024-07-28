#ifndef GFX_OPENGL_H
#define GFX_OPENGL_H

#include "core/maths.h"
#include "core/array.h"

#if defined(_WIN32)
#include "core/win32_lite.h"
#include "core/win32_opengl.h"
#elif defined(__linux__)
#include "core/linux_opengl.h"
#endif

struct GfxProgram {
    GLuint object;
    GLuint shaders[2];
};

struct TextureAsset {
    GLuint texture_handle;
};

struct ShaderAsset {
    GLuint object;
    GLint stage;

    DynamicArray<GfxProgram*> used_by;
};

enum GfxCommandType {
    GFX_COMMAND_COLORED_LINE,
    GFX_COMMAND_COLORED_PRIM,
    GFX_COMMAND_TEXTURED_PRIM,
    GFX_COMMAND_GUI_PRIM_TEXTURE,
    GFX_COMMAND_GUI_TEXT,
    GFX_COMMAND_MONO_TEXT,
};

struct GfxCommand {
    GfxCommandType type;
    union {
        struct {
            GLuint vbo;
            i32 vbo_offset;
            i32 vertex_count;
        } colored_prim;
        struct {
            GLuint vbo;
            i32 vbo_offset;
            i32 vertex_count;
            GLuint texture;
        } textured_prim;
        struct {
            GLuint vbo;
            GLuint texture;
            i32 vbo_offset;
            i32 vertex_count;
        } gui_prim_texture;
        struct {
            GLuint vbo;
            GLuint font_atlas;
            i32 vbo_offset;
            i32 vertex_count;
            Vector3 color;
        } gui_text;
        struct {
            GLuint vbo;
            i32 vbo_offset;
            GLuint glyph_ssbo;
            GLuint glyph_atlas;
            Vector2 cell_size;
            Vector2 pos;
            f32 offset;
            i32 line_offset;
            i32 columns;
        } mono_text;
    };
};

struct GfxCommandBuffer {
    DynamicArray<GfxCommand> commands;
};

struct GfxVertexAttrib {
    GLuint index;
    GLint size;
    GLenum type;
    GLboolean normalized;
    GLsizei stride;
    GLsizei offset;
};

struct GfxContext {
    struct {
        struct {
            GLint cs_from_ws;
        } global;
        struct {
            GfxProgram *program;
            GLint ws_from_ls;
            GLint color;
        } basic2d;
        struct {
            GfxProgram *program;
        } pass2d;
        struct {
            GfxProgram *program;
            GLint resolution;
            GLint color;
        } text;
        struct {
            GfxProgram *program;
            GLuint resolution;
            GLuint cell_size;
            GLuint pos, offset, line_offset, columns;
        } mono_text;
        struct {
            GfxProgram *program;
            GLint resolution;
        } gui_prim_texture;
    } shaders;

    struct {
        GLuint square;
        GLuint frame;
    } vaos;

    struct {
        GLuint square;
        GLuint frame;
    } vbos;

    struct {
        GLuint font;
    } textures;

    GfxCommandBuffer frame_cmdbuf;
    DynamicArray<f32> frame_vertices;
    Vector2 resolution;
};

extern GfxContext gfx;

struct Camera2 {
    Matrix3 projection;
    Vector2 position;
    f32 uni_scale = 1.0f;
};

struct Camera3 {
    Matrix4 projection;
    Vector3 position;
    f32 uni_scale = 1.0f;
};

#include "gen/gfx_opengl.h"


void gfx_push_command(GfxCommand cmd, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);
void gfx_draw_square(Vector2 center, Vector2 size, Vector4 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);
void gfx_draw_square(Vector2 center, Vector2 size, Vector2 uv_tl, Vector2 uv_br, GLuint texture, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);
void gfx_draw_square(Vector2 p0,  Vector2 p1,  Vector2 p2, Vector2 p3, Vector3 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);
void gfx_draw_rect(Vector2 tl, Vector2 size, Vector4 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);

void gfx_draw_triangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector3 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);

void gfx_draw_line(Vector2 a, Vector2 b, Vector3 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);
void gfx_draw_line_square(Vector2 center, Vector2 size, Vector3 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);
void gfx_draw_line_rect(Vector2 tl, Vector2 size, Vector3 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);
void gfx_draw_line_loop(Vector2 *points, i32 num_points, Vector3 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);

#endif // GFX_OPENGL_H
