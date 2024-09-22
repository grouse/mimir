#include "gfx_opengl.h"
#include "generated/gfx_opengl.h"

#include "assets.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x) ASSERT(x)
#include "stb/stb_image.h"

extern Allocator mem_frame;

GfxContext gfx{};

void gfx_push_command(GfxCommand cmd, GfxCommandBuffer *cmdbuf) EXPORT
{
    if (auto *last = array_tail(cmdbuf->commands);
        last && cmd.type == last->type)
    {
        switch (cmd.type) {
        case GFX_COMMAND_COLORED_PRIM:
        case GFX_COMMAND_COLORED_LINE:
            if (last->colored_prim.vbo == cmd.colored_prim.vbo &&
                (last->colored_prim.vbo_offset+(last->colored_prim.vertex_count*6))  == cmd.colored_prim.vbo_offset)
            {
                last->colored_prim.vertex_count += cmd.colored_prim.vertex_count;
                return;
            }
            break;
        case GFX_COMMAND_TEXTURED_PRIM:
            if (last->textured_prim.texture == cmd.textured_prim.texture &&
                last->textured_prim.vbo == cmd.textured_prim.vbo &&
                last->textured_prim.vbo_offset+last->textured_prim.vertex_count*4 == cmd.textured_prim.vbo_offset)
            {
                last->textured_prim.vertex_count += cmd.textured_prim.vertex_count;
                return;
            }
            break;
        case GFX_COMMAND_MONO_TEXT:
        case GFX_COMMAND_GUI_PRIM_TEXTURE:
        case GFX_COMMAND_GUI_TEXT:
            break;
        }
    }

    array_add(&cmdbuf->commands, cmd);
}

bool gfx_link_program(GfxProgram *program) INTERNAL
{
    for (auto it : program->shaders) glAttachShader(program->object, it);
    glLinkProgram(program->object);

    GLint program_linked;
    glGetProgramiv(program->object, GL_LINK_STATUS, &program_linked);
    if (program_linked != GL_TRUE) {
        GLsizei log_length = 0;
        GLchar message[1024];
        glGetProgramInfoLog(program->object, 1024, &log_length, message);
        LOG_ERROR("program failed to link: %s", message);
        return false;
    } else {
        return true;
    }
}

GfxProgram* gfx_create_shader(const char *vertex_src, const char *fragment_src) EXPORT
{
    u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_src, nullptr);
    glCompileShader(vertex_shader);

    GLint vertex_compiled;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &vertex_compiled);
    if (vertex_compiled != GL_TRUE) {
        GLsizei log_length = 0;
        GLchar message[1024];
        glGetShaderInfoLog(vertex_shader, 1024, &log_length, message);
        LOG_ERROR("vertex shader failed to compile: %s", message);
        return 0;
    }

    u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_src, nullptr);
    glCompileShader(fragment_shader);

    GLint fragment_compiled;
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &fragment_compiled);
    if (fragment_compiled != GL_TRUE) {
        GLsizei log_length = 0;
        GLchar message[1024];
        glGetShaderInfoLog(fragment_shader, 1024, &log_length, message);
        LOG_ERROR("fragment shader failed to compile: %s", message);
        return 0;
    }

    GfxProgram *program = ALLOC_T(mem_dynamic, GfxProgram) {
        .object = glCreateProgram(),
        .shaders = { vertex_shader, fragment_shader },
    };

    if (gfx_link_program(program)) {
        return program;
    } else {
        return {};
    }
}

GfxProgram* gfx_create_shader_program(String vertex, String fragment) EXPORT
{
    auto *vertex_shader = find_asset<ShaderAsset>(vertex);
    auto *fragment_shader = find_asset<ShaderAsset>(fragment);

    if (!vertex_shader) {
        LOG_ERROR("unable to find vertex shader: %.*s", STRFMT(vertex));
        return {};
    }

    if (!fragment_shader) {
        LOG_ERROR("unable to find fragment shader: %.*s", STRFMT(fragment));
        return {};
    }

    GfxProgram *program = ALLOC_T(mem_dynamic, GfxProgram) {
        .object = glCreateProgram(),
        .shaders = { vertex_shader->object, fragment_shader->object },
    };

    if (gfx_link_program(program)) {
        array_add(&vertex_shader->used_by, program);
        array_add(&fragment_shader->used_by, program);
        return program;
    } else {
        return {};
    }
}

void gl_debug_proc(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar *message,
    const void */*userParam*/) INTERNAL
{
    String s_source;
    switch (source) {
    case GL_DEBUG_SOURCE_API: s_source = "api"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM: s_source = "wm"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: s_source = "shaderc"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY: s_source = "3rd_party"; break;
    case GL_DEBUG_SOURCE_APPLICATION: s_source = "app"; break;
    case GL_DEBUG_SOURCE_OTHER: s_source = "other"; break;
    default: s_source = "unknown"; break;
    }

    String s_severity;
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH: s_severity = "high"; break;
    case GL_DEBUG_SEVERITY_MEDIUM: s_severity = "medium"; break;
    case GL_DEBUG_SEVERITY_LOW: s_severity = "low"; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: s_severity = "notification"; break;
    default: s_severity = "unknown"; break;
    }

    String s_type;
    switch (type) {
    case GL_DEBUG_TYPE_ERROR: s_type = "error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: s_type = "deprecated"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: s_type = "undefined"; break;
    case GL_DEBUG_TYPE_PORTABILITY: s_type = "portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE: s_type = "performance"; break;
    case GL_DEBUG_TYPE_MARKER: s_type = "marker"; break;
    case GL_DEBUG_TYPE_PUSH_GROUP: s_type = "push_group"; break;
    case GL_DEBUG_TYPE_POP_GROUP: s_type = "pop_group"; break;
    case GL_DEBUG_TYPE_OTHER: s_type = "other"; break;
    default: s_type = "unknown"; break;
    }

    LogType log_type = LOG_TYPE_INFO;
    if (severity == GL_DEBUG_SEVERITY_HIGH ||
        severity == GL_DEBUG_SEVERITY_MEDIUM)
    {
        log_type = LOG_TYPE_ERROR;
    }

    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
        LOG(log_type, "%.*s (0x%x): %.*s", STRFMT(s_severity), id, length, message);

        if (type == GL_DEBUG_TYPE_ERROR &&
            debugger_attached())
        {
            DEBUG_BREAK();
        }
    }
}

#define SHADER_HEADER \
    "#version 430 core\n" \
    "#extension GL_ARB_explicit_uniform_location : enable\n"\
    "layout(location = 0) uniform mat3 cs_from_ws;\n"\
    "vec3 rgb_unpack(uint rgb)\n"\
    "{\n"\
    "	uint r = rgb & 0xff;\n"\
    "	uint g = (rgb >> 8) & 0xff;\n"\
    "	uint b = (rgb >> 16) & 0xff;\n"\
    "	return vec3(r, g, b) / 255.0;\n"\
    "}\n"\
    "vec3 bgr_unpack(uint rgb)\n"\
    "{\n"\
    "	uint b = rgb & 0xff;\n"\
    "	uint g = (rgb >> 8) & 0xff;\n"\
    "	uint r = (rgb >> 16) & 0xff;\n"\
    "	return vec3(r, g, b) / 255.0;\n"\
    "}\n"

void init_gfx(Vector2 resolution) EXPORT
{
    if (debugger_attached()) {
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    } else {
        glEnable(GL_DEBUG_OUTPUT);
    }

    gfx.frame_vertices.alloc = mem_frame;

    glDebugMessageCallback(gl_debug_proc, nullptr);
    gfx_change_resolution(resolution);

    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    gfx.shaders.global.cs_from_ws = 0;

    {
        const char *vert = SHADER_HEADER
            "layout(location = 0) in vec2 v_pos;\n"
            "layout(location = 1) in vec4 v_color;\n"
            "out vec4 vs_color;\n"
            "void main()\n"
            "{\n"
            "   gl_Position = vec4((cs_from_ws * vec3(v_pos, 1.0)).xy, 0.0, 1.0);\n"
            "   vs_color = v_color;\n"
            "}\0";

        const char *frag = SHADER_HEADER
            "in vec4 vs_color;\n"
            "out vec4 out_color;\n"
            "void main()\n"
            "{\n"
            "	out_color = vs_color;\n"
            "}\0";

        gfx.shaders.pass2d.program = gfx_create_shader(vert, frag);
        ASSERT(glGetUniformLocation(gfx.shaders.pass2d.program->object, "cs_from_ws") == gfx.shaders.global.cs_from_ws);
    }

    {
        const char *vert = SHADER_HEADER
            "layout(location = 0) in vec2 v_pos;\n"
            "layout(location = 1) in vec2 v_uv;\n"
            "out vec2 vs_uv;\n"
            "void main()\n"
            "{\n"
            "   gl_Position = vec4((cs_from_ws * vec3(v_pos, 1.0)).xy, 0.0, 1.0);\n"
            "   vs_uv = v_uv;\n"
            "}\0";

        const char *frag = SHADER_HEADER
            "in vec2 vs_uv;\n"
            "layout(location = 1) uniform sampler2D tex_sampler;\n"
            "out vec4 out_color;\n"
            "void main()\n"
            "{\n"
            "	out_color = vec4(texture(tex_sampler, vs_uv).rgba);\n"
            "}\0";

        gfx.shaders.basic2d.program = gfx_create_shader(vert, frag);
        ASSERT(glGetUniformLocation(gfx.shaders.basic2d.program->object, "cs_from_ws") == gfx.shaders.global.cs_from_ws);
    }

    {
        gfx.shaders.gui_prim_texture.program = gfx_create_shader_program(
            "shaders/gui_prim_texture.vert.glsl",
            "shaders/gui_prim_texture.frag.glsl");
    }

    {
        gfx.shaders.text.program = gfx_create_shader_program("shaders/text.vert.glsl", "shaders/text.frag.glsl");
        gfx.shaders.text.resolution = glGetUniformLocation(gfx.shaders.text.program->object, "resolution");
        gfx.shaders.text.color = glGetUniformLocation(gfx.shaders.text.program->object, "color");
    }

    {
        const char *vert = SHADER_HEADER
            "layout(location = 0) in vec2 v_pos;\n"
            "uniform vec2 resolution;\n"
            "uniform vec2 pos;\n"
            "uniform float voffset;\n"
            "out vec2 vs_pos;\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vec4(v_pos.xy / resolution * 2.0 - 1.0, 0.0, 1.0);\n"
            "    gl_Position.y = -gl_Position.y;\n"
            "	vs_pos = v_pos - pos + vec2(0.0, voffset);\n"
            "}\0";

        const char *frag = SHADER_HEADER
            "in vec2 vs_pos;\n"
            "uniform sampler2D glyph_atlas;\n"
            "uniform vec2 cell_size;\n"
            "uniform int columns;\n"
            "uniform int line_offset;\n"
            "uniform vec2 pos;\n"
            "struct GlyphData {\n"
            "	uint glyph_index;\n"
            "	uint fg;\n"
            "};\n"
            "layout(std430, binding = 3) buffer cells\n"
            "{\n"
            "	GlyphData glyph_data[];\n"
            "};\n"
            "out vec4 out_color;\n"
            "void main()\n"
            "{\n"
            "	ivec2 cell_index = ivec2(vec2(vs_pos / cell_size));\n"
            "	vec2 cell_pos = vec2(mod(vs_pos, cell_size));\n"
            "	GlyphData cell = glyph_data[cell_index.y*columns + cell_index.x];\n"
            "	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);\n"
            "	if (cell.glyph_index != 0xFFFFFFFF)\n"
            "	{\n"
            "		ivec2 glyph_pos = ivec2(cell.glyph_index & 0xFFFF, cell.glyph_index >> 16);\n"
            "		vec2 uv = (glyph_pos + cell_pos) / textureSize(glyph_atlas, 0);\n"
            "		color = vec4(bgr_unpack(cell.fg), texture(glyph_atlas, uv).r);\n"
            "	}\n"
            "	out_color = color;\n"
            "}\0";

            gfx.shaders.mono_text.program = gfx_create_shader(vert, frag);
            gfx.shaders.mono_text.resolution = glGetUniformLocation(gfx.shaders.mono_text.program->object, "resolution");
            gfx.shaders.mono_text.cell_size = glGetUniformLocation(gfx.shaders.mono_text.program->object, "cell_size");
            gfx.shaders.mono_text.pos = glGetUniformLocation(gfx.shaders.mono_text.program->object, "pos");
            gfx.shaders.mono_text.offset = glGetUniformLocation(gfx.shaders.mono_text.program->object, "voffset");
            gfx.shaders.mono_text.line_offset = glGetUniformLocation(gfx.shaders.mono_text.program->object, "line_offset");
            gfx.shaders.mono_text.columns = glGetUniformLocation(gfx.shaders.mono_text.program->object, "columns");
    }

    f32 square_vertices[] = {
        0.5f, -0.5f,
        -0.5f, -0.5f,
        -0.5f, 0.5f,

        -0.5f, 0.5f,
        0.5f, 0.5f,
        0.5f, -0.5f,
    };

    glGenBuffers(1, &gfx.vbos.square);
    glGenVertexArrays(1, &gfx.vaos.square);

    glBindVertexArray(gfx.vaos.square);

    glBindBuffer(GL_ARRAY_BUFFER, gfx.vbos.square);
    glBufferData(GL_ARRAY_BUFFER, sizeof square_vertices, square_vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof square_vertices[0], (void*)0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &gfx.vbos.frame);
    glGenVertexArrays(1, &gfx.vaos.frame);
}

bool gfx_change_resolution(Vector2 resolution) EXPORT
{
    if (gfx.resolution == resolution) return false;
    LOG_INFO("new render resolution: {%f, %f}", resolution.x, resolution.y);
    glViewport(0, 0, resolution.x, resolution.y);
    gfx.resolution = resolution;
    return true;
}

void gfx_draw_square(Vector2 center, Vector2 size, Vector4 color, GfxCommandBuffer *cmdbuf) EXPORT
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_PRIM;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = 6;
    gfx_push_command(cmd, cmdbuf);

    color = linear_from_sRGB(color);

    f32 left = center.x - size.x*0.5f;
    f32 right = center.x + size.x*0.5f;
    f32 top = center.y - size.y*0.5f;
    f32 bottom = center.y + size.y*0.5f;

    f32 vertices[] = {
        right, top, color.r, color.g, color.b, color.a,
        left, top, color.r, color.g, color.b, color.a,
        left, bottom, color.r, color.g, color.b, color.a,

        left, bottom, color.r, color.g, color.b, color.a,
        right, bottom, color.r, color.g, color.b, color.a,
        right, top, color.r, color.g, color.b, color.a,
    };
    array_add(&gfx.frame_vertices, vertices, ARRAY_COUNT(vertices));
}

void gfx_draw_rect(
        Vector2 tl,
        Vector2 size,
        Vector4 color,
        GfxCommandBuffer *cmdbuf) EXPORT
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_PRIM;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = 6;
    gfx_push_command(cmd, cmdbuf);

    color = linear_from_sRGB(color);

    f32 vertices[] = {
        tl.x+size.x, tl.y,        color.r, color.g, color.b, color.a,
        tl.x,        tl.y,        color.r, color.g, color.b, color.a,
        tl.x,        tl.y+size.y, color.r, color.g, color.b, color.a,

        tl.x,        tl.y+size.y, color.r, color.g, color.b, color.a,
        tl.x+size.x, tl.y+size.y, color.r, color.g, color.b, color.a,
        tl.x+size.x, tl.y,        color.r, color.g, color.b, color.a,
    };
    array_add(&gfx.frame_vertices, vertices, ARRAY_COUNT(vertices));
}

void gfx_draw_square(
    Vector2 p0,
    Vector2 p1,
    Vector2 p2,
    Vector2 p3,
    Vector3 color,
    GfxCommandBuffer *cmdbuf) EXPORT
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_PRIM;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = 6;
    gfx_push_command(cmd, cmdbuf);

    color = linear_from_sRGB(color);

    f32 vertices[] = {
        p0.x, p0.y, color.r, color.g, color.b, 1.0f,
        p1.x, p1.y, color.r, color.g, color.b, 1.0f,
        p2.x, p2.y, color.r, color.g, color.b, 1.0f,

        p2.x, p2.y, color.r, color.g, color.b, 1.0f,
        p3.x, p3.y, color.r, color.g, color.b, 1.0f,
        p0.x, p0.y, color.r, color.g, color.b, 1.0f,
    };
    array_add(&gfx.frame_vertices, vertices, ARRAY_COUNT(vertices));
}

void gfx_draw_square(
    Vector2 center,
    Vector2 size,
    Vector2 uv_tl,
    Vector2 uv_br,
    GLuint texture,
    GfxCommandBuffer *cmdbuf) EXPORT
{
    f32 left = center.x - size.x*0.5f;
    f32 right = center.x + size.x*0.5f;
    f32 top = center.y - size.y*0.5f;
    f32 bottom = center.y + size.y*0.5f;

    f32 vertices[] = {
        right, top, uv_br.x, uv_tl.y,
        left, top, uv_tl.x, uv_tl.y,
        left, bottom, uv_tl.x, uv_br.y,

        left, bottom, uv_tl.x, uv_br.y,
        right, bottom, uv_br.x, uv_br.y,
        right, top, uv_br.x, uv_tl.y,
    };

    GfxCommand cmd{
        .type = GFX_COMMAND_TEXTURED_PRIM,
        .textured_prim.vbo = gfx.vbos.frame,
        .textured_prim.vbo_offset = gfx.frame_vertices.count,
        .textured_prim.texture = texture,
        .textured_prim.vertex_count = 6,
    };
    gfx_push_command(cmd, cmdbuf);

    array_add(&gfx.frame_vertices, vertices, ARRAY_COUNT(vertices));

}

void gfx_draw_triangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector3 color, GfxCommandBuffer *cmdbuf) EXPORT
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_PRIM;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = 3;
    gfx_push_command(cmd, cmdbuf);

    color = linear_from_sRGB(color);

    f32 vertices[] = {
        p0.x, p0.y, color.r, color.g, color.b, 1.0f,
        p1.x, p1.y, color.r, color.g, color.b, 1.0f,
        p2.x, p2.y, color.r, color.g, color.b, 1.0f,
    };
    array_add(&gfx.frame_vertices, vertices, ARRAY_COUNT(vertices));
}


void gfx_draw_line(Vector2 a, Vector2 b, Vector3 color, GfxCommandBuffer *cmdbuf) EXPORT
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_LINE;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = 2;
    gfx_push_command(cmd, cmdbuf);

    color = linear_from_sRGB(color);

    f32 vertices[] = {
        a.x, a.y, color.r, color.g, color.b, 1.0f,
        b.x, b.y, color.r, color.g, color.b, 1.0f,
    };
    array_add(&gfx.frame_vertices, vertices, ARRAY_COUNT(vertices));
}

void gfx_draw_line_loop(Vector2 *points, i32 num_points, Vector3 color, GfxCommandBuffer *cmdbuf) EXPORT
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_LINE;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = num_points*2;
    gfx_push_command(cmd, cmdbuf);

    color = linear_from_sRGB(color);

    array_grow(&gfx.frame_vertices, num_points*2*6);
    for (i32 i = 0; i < num_points-1; i++) {
        f32 vertices[] = {
            points[i].x, points[i].y, color.r, color.g, color.b, 1.0f,
            points[i+1].x, points[i+1].y, color.r, color.g, color.b, 1.0f,
        };
        array_add(&gfx.frame_vertices, vertices, ARRAY_COUNT(vertices));
    }

    f32 vertices[] = {
        points[num_points-1].x, points[num_points-1].y, color.r, color.g, color.b, 1.0f,
        points[0].x, points[0].y, color.r, color.g, color.b, 1.0f,
    };
    array_add(&gfx.frame_vertices, vertices, ARRAY_COUNT(vertices));
}


void gfx_draw_line_square(Vector2 center, Vector2 size, Vector3 color, GfxCommandBuffer *cmdbuf) EXPORT
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_LINE;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = 8;
    gfx_push_command(cmd, cmdbuf);

    f32 left = center.x - size.x*0.5f;
    f32 right = center.x + size.x*0.5f;
    f32 top = center.y - size.y*0.5f;
    f32 bottom = center.y + size.y*0.5f;

    color = linear_from_sRGB(color);

    f32 vertices[] = {
        // ---
        left, top, color.r, color.g, color.b, 1.0f,
        right, top, color.r, color.g, color.b, 1.0f,

        //   |
        right, top, color.r, color.g, color.b, 1.0f,
        right, bottom, color.r, color.g, color.b, 1.0f,

        // ___
        right, bottom, color.r, color.g, color.b, 1.0f,
        left, bottom, color.r, color.g, color.b, 1.0f,

        // |
        left, bottom, color.r, color.g, color.b, 1.0f,
        left, top, color.r, color.g, color.b, 1.0f,
    };
    array_add(&gfx.frame_vertices, vertices, ARRAY_COUNT(vertices));
}

void gfx_draw_line_rect(Vector2 tl, Vector2 size, Vector3 color, GfxCommandBuffer *cmdbuf) EXPORT
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_LINE;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = 8;
    gfx_push_command(cmd, cmdbuf);

    f32 left = tl.x;
    f32 right = tl.x + size.x;
    f32 top = tl.y;
    f32 bottom = tl.y + size.y;

    color = linear_from_sRGB(color);

    f32 vertices[] = {
        // ---
        left, top, color.r, color.g, color.b, 1.0f,
        right, top, color.r, color.g, color.b, 1.0f,

        //   |
        right, top, color.r, color.g, color.b, 1.0f,
        right, bottom, color.r, color.g, color.b, 1.0f,

        // ___
        right, bottom - 1, color.r, color.g, color.b, 1.0f,
        left, bottom - 1, color.r, color.g, color.b, 1.0f,

        // |
        left, bottom, color.r, color.g, color.b, 1.0f,
        left, top, color.r, color.g, color.b, 1.0f,
    };
    array_add(&gfx.frame_vertices, vertices, ARRAY_COUNT(vertices));
}



void gfx_begin_frame() EXPORT
{
    array_reset(&gfx.frame_vertices, gfx.frame_vertices.count);
    gfx_reset_command_buffer(&gfx.frame_cmdbuf);
}

void gfx_flush_transfers() EXPORT
{
    glBindVertexArray(gfx.vaos.frame);
    glBindBuffer(GL_ARRAY_BUFFER, gfx.vbos.frame);
    glBufferData(GL_ARRAY_BUFFER, gfx.frame_vertices.count * sizeof gfx.frame_vertices[0], gfx.frame_vertices.data, GL_STREAM_DRAW);
}

void gfx_reset_command_buffer(GfxCommandBuffer *cmdbuf) EXPORT
{
    array_reset(&cmdbuf->commands, cmdbuf->commands.count);
}

GfxCommandBuffer gfx_command_buffer() EXPORT
{
    GfxCommandBuffer cmd{ .commands.alloc = mem_frame };
    return cmd;
}

void gfx_submit_commands(GfxCommandBuffer cmdbuf, Matrix3 view) EXPORT
{
    for (GfxCommand cmd : cmdbuf.commands) {
        switch (cmd.type) {
        case GFX_COMMAND_MONO_TEXT:
            glBindBuffer(GL_ARRAY_BUFFER, cmd.mono_text.vbo);

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glUseProgram(gfx.shaders.mono_text.program->object);

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(f32), (void*)(i64)((cmd.mono_text.vbo_offset+0)*sizeof(f32)));
            glEnableVertexAttribArray(0);

            glBindTexture(GL_TEXTURE_2D, cmd.mono_text.glyph_atlas);

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, cmd.mono_text.glyph_ssbo);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, cmd.mono_text.glyph_ssbo);
            //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

            glUniform2f(gfx.shaders.mono_text.resolution, gfx.resolution.x, gfx.resolution.y);
            glUniform2f(gfx.shaders.mono_text.cell_size, cmd.mono_text.cell_size.x, cmd.mono_text.cell_size.y);
            glUniform2f(gfx.shaders.mono_text.pos, cmd.mono_text.pos.x, cmd.mono_text.pos.y);
            glUniform1f(gfx.shaders.mono_text.offset, cmd.mono_text.offset);
            glUniform1i(gfx.shaders.mono_text.line_offset, cmd.mono_text.line_offset);
            glUniform1i(gfx.shaders.mono_text.columns, cmd.mono_text.columns);

            glDrawArrays(GL_TRIANGLES, 0, 6);
            break;
        case GFX_COMMAND_TEXTURED_PRIM:
            glBindBuffer(GL_ARRAY_BUFFER, cmd.textured_prim.vbo);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            glUseProgram(gfx.shaders.basic2d.program->object);
            glBindTexture(GL_TEXTURE_2D, cmd.textured_prim.texture);
            glUniformMatrix3fv(gfx.shaders.global.cs_from_ws, 1, GL_FALSE, view.data);

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(f32), (void*)(i64)((cmd.textured_prim.vbo_offset+0)*sizeof(f32)));
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(f32), (void*)(i64)((cmd.textured_prim.vbo_offset+2)*sizeof(f32)));

            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);

            glDrawArrays(GL_TRIANGLES, 0, cmd.textured_prim.vertex_count);
            break;
        case GFX_COMMAND_COLORED_PRIM:
            glBindBuffer(GL_ARRAY_BUFFER, cmd.colored_prim.vbo);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            glUseProgram(gfx.shaders.pass2d.program->object);
            glUniformMatrix3fv(gfx.shaders.global.cs_from_ws, 1, GL_FALSE, view.data);

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6*sizeof(f32), (void*)(i64)((cmd.colored_prim.vbo_offset+0)*sizeof(f32)));
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6*sizeof(f32), (void*)(i64)((cmd.colored_prim.vbo_offset+2)*sizeof(f32)));

            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);

            glDrawArrays(GL_TRIANGLES, 0, cmd.colored_prim.vertex_count);
            break;
        case GFX_COMMAND_COLORED_LINE:
            glBindBuffer(GL_ARRAY_BUFFER, gfx.vbos.frame);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

            glUseProgram(gfx.shaders.pass2d.program->object);
            glUniformMatrix3fv(gfx.shaders.global.cs_from_ws, 1, GL_FALSE, view.data);

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6*sizeof(f32), (void*)(i64)((cmd.colored_prim.vbo_offset+0)*sizeof(f32)));
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6*sizeof(f32), (void*)(i64)((cmd.colored_prim.vbo_offset+2)*sizeof(f32)));

            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);

            glDrawArrays(GL_LINES, 0, cmd.colored_prim.vertex_count);
            break;
        case GFX_COMMAND_GUI_PRIM_TEXTURE:
            glBindBuffer(GL_ARRAY_BUFFER, cmd.gui_prim_texture.vbo);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            glUseProgram(gfx.shaders.gui_prim_texture.program->object);

            glUniformMatrix3fv(gfx.shaders.global.cs_from_ws, 1, GL_FALSE, view.data);

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(f32), (void*)(cmd.gui_prim_texture.vbo_offset*sizeof(f32)));
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(f32), (void*)((cmd.gui_prim_texture.vbo_offset+2)*sizeof(f32)));
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);

            glBindTexture(GL_TEXTURE_2D, cmd.gui_prim_texture.texture);

            glDrawArrays(GL_TRIANGLES, 0, cmd.gui_prim_texture.vertex_count / 4);
            break;
        case GFX_COMMAND_GUI_TEXT:
            glBindBuffer(GL_ARRAY_BUFFER, cmd.gui_text.vbo);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            glUseProgram(gfx.shaders.text.program->object);

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(f32), (void*)(cmd.gui_text.vbo_offset*sizeof(f32)));
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(f32), (void*)((cmd.gui_text.vbo_offset+2)*sizeof(f32)));
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);

            glUniform2f(gfx.shaders.text.resolution, gfx.resolution.x, gfx.resolution.y);
            glUniform3f(gfx.shaders.text.color, cmd.gui_text.color.r, cmd.gui_text.color.g, cmd.gui_text.color.b);

            glBindTexture(GL_TEXTURE_2D, cmd.gui_text.font_atlas);

            glDrawArrays(GL_TRIANGLES, 0, cmd.gui_text.vertex_count / 4);
            break;
        }
    }
}

GLuint gfx_create_texture(void *pixel_data, i32 width, i32 height) EXPORT
{
    GLuint handle;
    glGenTextures(1, &handle);

    glBindTexture(GL_TEXTURE_2D, handle);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data);

    return handle;
}

// ----------------------------------------
// GFX_ASSETS
// ----------------------------------------
void* gfx_load_texture_asset(AssetHandle /*handle*/, void *existing, String /*identifier*/, u8 *data, i32 size)
{
    i32 width, height, num_channels;
    u8 *pixel_data = stbi_load_from_memory(data, size, &width, &height, &num_channels, 4);

    if (!pixel_data) {
        LOG_ERROR("failed to load texture: %s", stbi_failure_reason());
        return nullptr;
    }

    GLuint object = gfx_create_texture(pixel_data, width, height);
    if (!object) {
        LOG_ERROR("failed to create texture");
        return nullptr;
    }

    if (existing) {
        TextureAsset *texture = (TextureAsset*)existing;
        glDeleteTextures(1, &texture->texture_handle);
        texture->texture_handle = object;
        return texture;
    }

    return ALLOC_T(mem_dynamic, TextureAsset) {
        .texture_handle = object,
    };
}

void* gfx_load_shader_asset(
    AssetHandle /*handle*/,
    void *existing,
    String identifier,
    u8 *data, i32 size)
{
    ShaderAsset *shader = (ShaderAsset*)existing;

    GLint stage;
    if (ends_with(identifier, ".vert.glsl")) stage = GL_VERTEX_SHADER;
    else if (ends_with(identifier, ".frag.glsl")) stage = GL_FRAGMENT_SHADER;
    else {
        LOG_ERROR("unknown shader stage for identifier %.*s", STRFMT(identifier));
        return nullptr;
    }

    const char *strings[] = { SHADER_HEADER, (char*)data };
    i32 lengths[] = { sizeof(SHADER_HEADER)-1, size };

    GLuint object = shader ? shader->object : glCreateShader(stage);
    glShaderSource(object, ARRAY_COUNT(strings), strings, lengths);
    glCompileShader(object);

    GLint status;
    glGetShaderiv(object, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLsizei log_length = 0;
        GLchar message[1024];
        glGetShaderInfoLog(object, 1024, &log_length, message);
        LOG_ERROR("%.*s: compile error: %s", STRFMT(identifier), message);
        return nullptr;
    }

    if (shader) {
        for (auto it : shader->used_by) {
            for (auto s : it->shaders) glDetachShader(it->object, s);

            glDeleteProgram(it->object);
            it->object = glCreateProgram();

            gfx_link_program(it);
        }

        return shader;
    }

    return ALLOC_T(mem_dynamic, ShaderAsset) {
        .object = object,
        .stage = stage,
    };
}
