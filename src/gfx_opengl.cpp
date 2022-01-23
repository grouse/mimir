#include "gfx_opengl.h"

#include "external/stb/stb_image.h"

GfxContext gfx;

extern Allocator mem_frame;

void gfx_push_command(GfxCommandBuffer *cmdbuf, GfxCommand cmd)
{
    if (cmdbuf->commands.count > 0) {
        GfxCommand *last = &cmdbuf->commands[cmdbuf->commands.count-1];
        if (cmd.type == last->type) {
            switch (cmd.type) {
            case GFX_COMMAND_COLORED_PRIM:
            case GFX_COMMAND_COLORED_LINE:
                last->colored_prim.vertex_count += cmd.colored_prim.vertex_count;
                return;
            case GFX_COMMAND_TEXTURED_PRIM:
                last->textured_prim.vertex_count += cmd.textured_prim.vertex_count;
                return;
            case GFX_COMMAND_GUI_PRIM_COLOR:
            case GFX_COMMAND_GUI_PRIM_TEXTURE:
            case GFX_COMMAND_GUI_TEXT:
                break;
            }
        }
    }

    array_add(&cmdbuf->commands, cmd);
}

u32 gfx_create_shader(const char *vertex_src, const char *fragment_src)
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
    }

    u32 program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    
    GLint program_linked;
    glGetProgramiv(program, GL_LINK_STATUS, &program_linked);
    if (program_linked != GL_TRUE) {
        GLsizei log_length = 0;
        GLchar message[1024];
        glGetProgramInfoLog(program, 1024, &log_length, message);
        LOG_ERROR("program failed to link: %s", message);
    }

    return program;
}

void gl_debug_proc(
    GLenum source, 
    GLenum type, 
    GLuint id, 
    GLenum severity, 
    GLsizei length, 
    const GLchar *message, 
    const void */*userParam*/)
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
        
    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
        String filename = filename_of(__FILE__);
        LOG_RAW("%.*s:%d: %.*s: %.*s, %.*s (0x%x): %.*s\n",
                STRFMT(filename), __LINE__,
                STRFMT(s_severity), STRFMT(s_source), STRFMT(s_type), id,
                length, message);
        
        if (type == GL_DEBUG_TYPE_ERROR &&
            debugger_attached()) 
        {
            DEBUG_BREAK();
        }
    }
}

#define SHADER_HEADER \
    "#version 330 core\n" \
    "#extension GL_ARB_explicit_uniform_location : enable\n"\
    "layout(location = 0) uniform mat4 cs_from_ws;\n"

void init_gfx(Vector2 resolution)
{
    if (debugger_attached()) {
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    } else {
        glEnable(GL_DEBUG_OUTPUT);
    }
            
    glDebugMessageCallback(gl_debug_proc, nullptr);
    
    gfx.resolution = resolution;
    
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
            "   gl_Position = cs_from_ws * vec4(v_pos, 0.0, 1.0);\n"
            "   gl_Position.y = -gl_Position.y;\n"
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
        ASSERT(glGetUniformLocation(gfx.shaders.pass2d.program, "cs_from_ws") == gfx.shaders.global.cs_from_ws);
    }

    {
        const char *vert = SHADER_HEADER
            "layout(location = 0) in vec2 v_pos;\n"
            "layout(location = 1) in vec2 v_uv;\n"
            "out vec2 vs_uv;\n"
            "void main()\n"
            "{\n"
            "   gl_Position = cs_from_ws * vec4(v_pos, 0.0, 1.0);\n"
            "   gl_Position.y = -gl_Position.y;\n"
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
        ASSERT(glGetUniformLocation(gfx.shaders.basic2d.program, "cs_from_ws") == gfx.shaders.global.cs_from_ws);
    }
    
    {
        const char *vert = SHADER_HEADER
            "layout(location = 0) in vec2 v_pos;\n"
            "layout(location = 1) in vec3 v_color;\n"
            "uniform vec2 resolution;\n"
            "out vec3 vs_color;\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vec4(v_pos.xy / resolution * 2.0 - 1.0, 0.0, 1.0);\n"
            "    gl_Position.y = -gl_Position.y;\n"
            "    vs_color = v_color;\n"
            "}\0";

        const char *frag = SHADER_HEADER
            "in vec3 vs_color;\n"
            "out vec4 out_color;\n"
            "void main()\n"
            "{\n"
            "	out_color = vec4(vs_color, 1.0f);\n"
            "}\0";

        gfx.shaders.gui_prim.program = gfx_create_shader(vert, frag);
        gfx.shaders.gui_prim.resolution = glGetUniformLocation(gfx.shaders.gui_prim.program, "resolution");
    }
    
    {
        const char *vert = SHADER_HEADER
            "layout(location = 0) in vec2 v_pos;\n"
            "layout(location = 1) in vec2 v_uv;\n"
            "uniform vec2 resolution;\n"
            "out vec2 vs_uv;\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vec4(v_pos.xy / resolution * 2.0 - 1.0, 0.0, 1.0);\n"
            "    gl_Position.y = -gl_Position.y;\n"
            "    vs_uv = v_uv;\n"
            "}\0";

        const char *frag = SHADER_HEADER
            "in vec2 vs_uv;\n"
            "out vec4 out_color;\n"
            "uniform sampler2D sampler;\n"
            "void main()\n"
            "{\n"
            "	out_color = texture(sampler, vs_uv);\n"
            "}\0";

        gfx.shaders.gui_prim_texture.program = gfx_create_shader(vert, frag);
        gfx.shaders.gui_prim_texture.resolution = glGetUniformLocation(gfx.shaders.gui_prim_texture.program, "resolution");
    }


    {
        const char *vert = SHADER_HEADER
            "layout(location = 0) in vec2 v_pos;\n"
            "layout(location = 1) in vec2 v_uv;\n"
            "uniform vec2 resolution;\n"
            "uniform vec3 color;\n"
            "out vec2 vs_uv;\n"
            "out vec3 vs_color;\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vec4(v_pos.xy / resolution * 2.0 - 1.0, 0.0, 1.0);\n"
            "    gl_Position.y = -gl_Position.y;\n"
            "    vs_uv = v_uv;\n"
            "    vs_color = color;\n"
            "}\0";

        const char *frag = SHADER_HEADER
            "in vec2 vs_uv;\n"
            "in vec3 vs_color;\n"
            "out vec4 out_color;\n"
            "uniform sampler2D atlas_sampler;\n"
            "void main()\n"
            "{\n"
            "	out_color = vec4(vs_color.rgb, texture(atlas_sampler, vs_uv).r);\n"
            "}\0";

        gfx.shaders.text.program = gfx_create_shader(vert, frag);
        gfx.shaders.text.resolution = glGetUniformLocation(gfx.shaders.text.program, "resolution");
        gfx.shaders.text.color = glGetUniformLocation(gfx.shaders.text.program, "color");
    }

    f32 square_vertices[] = {
        0.5f, -0.5f, 0.0f, 
        -0.5f, -0.5f, 0.0f, 
        -0.5f, 0.5f, 0.0f, 

        -0.5f, 0.5f, 0.0f, 
        0.5f, 0.5f, 0.0f, 
        0.5f, -0.5f, 0.0f
    };
    
    glGenBuffers(1, &gfx.vbos.square);
    glGenVertexArrays(1, &gfx.vaos.square);

    glBindVertexArray(gfx.vaos.square);

    glBindBuffer(GL_ARRAY_BUFFER, gfx.vbos.square);
    glBufferData(GL_ARRAY_BUFFER, sizeof square_vertices, square_vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof square_vertices[0], (void*)0);
    glEnableVertexAttribArray(0);
    
    glGenBuffers(1, &gfx.vbos.frame);
    glGenVertexArrays(1, &gfx.vaos.frame);
}

bool gfx_change_resolution(Vector2 resolution)
{
    if (gfx.resolution == resolution) return false;
    gfx.resolution = resolution;
    return true;
}

void gfx_draw_square(Vector2 center, Vector2 size, Vector4 color, GfxCommandBuffer *cmdbuf)
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_PRIM;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = 6;
    gfx_push_command(cmdbuf, cmd);

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

void gfx_draw_square(
    Vector2 p0, 
    Vector2 p1, 
    Vector2 p2,
    Vector2 p3,
    Vector3 color,
    GfxCommandBuffer *cmdbuf)
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_PRIM;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = 6;
    gfx_push_command(cmdbuf, cmd);
    
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


void gfx_draw_square(Vector2 center, Vector2 size, Vector2 uv_tl, Vector2 uv_br, GLuint texture, GfxCommandBuffer *cmdbuf)
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_TEXTURED_PRIM;
    cmd.textured_prim.vbo = gfx.vbos.frame;
    cmd.textured_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.textured_prim.texture = texture;
    cmd.textured_prim.vertex_count = 6;
    gfx_push_command(cmdbuf, cmd);

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
    array_add(&gfx.frame_vertices, vertices, ARRAY_COUNT(vertices));

}

void gfx_draw_triangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector3 color, GfxCommandBuffer *cmdbuf)
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_PRIM;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = 3;
    gfx_push_command(cmdbuf, cmd);
    
    color = linear_from_sRGB(color);

    f32 vertices[] = {
        p0.x, p0.y, color.r, color.g, color.b, 1.0f,
        p1.x, p1.y, color.r, color.g, color.b, 1.0f,
        p2.x, p2.y, color.r, color.g, color.b, 1.0f,
    };
    array_add(&gfx.frame_vertices, vertices, ARRAY_COUNT(vertices));
}


void gfx_draw_line(Vector2 a, Vector2 b, Vector3 color, GfxCommandBuffer *cmdbuf)
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_LINE;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = 2;
    gfx_push_command(cmdbuf, cmd);
    
    color = linear_from_sRGB(color);

    f32 vertices[] = { 
        a.x, a.y, color.r, color.g, color.b, 1.0f, 
        b.x, b.y, color.r, color.g, color.b, 1.0f,
    };
    array_add(&gfx.frame_vertices, vertices, ARRAY_COUNT(vertices));
}

void gfx_draw_line_loop(Vector2 *points, i32 num_points, Vector3 color, GfxCommandBuffer *cmdbuf)
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_LINE;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = num_points*2;
    gfx_push_command(cmdbuf, cmd);

    color = linear_from_sRGB(color);
    
    array_reserve_add(&gfx.frame_vertices, num_points*2*6);
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


void gfx_draw_line_square(Vector2 center, Vector2 size, Vector3 color, GfxCommandBuffer *cmdbuf)
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_LINE;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = 8;
    gfx_push_command(cmdbuf, cmd);

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

void gfx_draw_line_rect(Vector2 tl, Vector2 size, Vector3 color, GfxCommandBuffer *cmdbuf)
{
    GfxCommand cmd;
    cmd.type = GFX_COMMAND_COLORED_LINE;
    cmd.colored_prim.vbo = gfx.vbos.frame;
    cmd.colored_prim.vbo_offset = gfx.frame_vertices.count;
    cmd.colored_prim.vertex_count = 8;
    gfx_push_command(cmdbuf, cmd);

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



void gfx_begin_frame()
{
    array_reset(&gfx.frame_vertices, mem_frame, gfx.frame_vertices.count);
    gfx_reset_command_buffer(&gfx.frame_cmdbuf);
}

void gfx_flush_transfers()
{
    glBindVertexArray(gfx.vaos.frame);
    glBindBuffer(GL_ARRAY_BUFFER, gfx.vbos.frame);
    glBufferData(GL_ARRAY_BUFFER, gfx.frame_vertices.count * sizeof gfx.frame_vertices[0], gfx.frame_vertices.data, GL_STREAM_DRAW);
}

void gfx_reset_command_buffer(GfxCommandBuffer *cmdbuf)
{
    array_reset(&cmdbuf->commands, mem_frame, cmdbuf->commands.count);
}

GfxCommandBuffer gfx_command_buffer()
{
    GfxCommandBuffer cmd{ .commands.alloc = mem_frame };
    return cmd;
}

void gfx_submit_commands(GfxCommandBuffer cmdbuf)
{
    Matrix4 cs_from_ws = matrix4_identity();
    cs_from_ws[0][0] = 2.0f / gfx.resolution.x;
    cs_from_ws[1][1] = 2.0f / gfx.resolution.y;
    cs_from_ws[3][0] = -1.0f;
    cs_from_ws[3][1] = -1.0f;

    for (GfxCommand cmd : cmdbuf.commands) {
        switch (cmd.type) {
        case GFX_COMMAND_TEXTURED_PRIM:
            glBindBuffer(GL_ARRAY_BUFFER, cmd.textured_prim.vbo);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            glUseProgram(gfx.shaders.basic2d.program);
            glBindTexture(GL_TEXTURE_2D, cmd.textured_prim.texture);
            glUniformMatrix4fv(gfx.shaders.global.cs_from_ws, 1, GL_FALSE, cs_from_ws.data);

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(f32), (void*)(i64)((cmd.textured_prim.vbo_offset+0)*sizeof(f32)));
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(f32), (void*)(i64)((cmd.textured_prim.vbo_offset+2)*sizeof(f32)));

            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1); 

            glDrawArrays(GL_TRIANGLES, 0, cmd.textured_prim.vertex_count);
            break;
        case GFX_COMMAND_COLORED_PRIM:
            glBindBuffer(GL_ARRAY_BUFFER, gfx.vbos.frame);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            glUseProgram(gfx.shaders.pass2d.program);
            glUniformMatrix4fv(gfx.shaders.global.cs_from_ws, 1, GL_FALSE, cs_from_ws.data);

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6*sizeof(f32), (void*)(i64)((cmd.colored_prim.vbo_offset+0)*sizeof(f32)));
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6*sizeof(f32), (void*)(i64)((cmd.colored_prim.vbo_offset+2)*sizeof(f32)));

            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1); 

            glDrawArrays(GL_TRIANGLES, 0, cmd.colored_prim.vertex_count);
            break;
        case GFX_COMMAND_COLORED_LINE:
            glBindBuffer(GL_ARRAY_BUFFER, gfx.vbos.frame);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

            glUseProgram(gfx.shaders.pass2d.program);
            glUniformMatrix4fv(gfx.shaders.global.cs_from_ws, 1, GL_FALSE, cs_from_ws.data);

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6*sizeof(f32), (void*)(i64)((cmd.colored_prim.vbo_offset+0)*sizeof(f32)));
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6*sizeof(f32), (void*)(i64)((cmd.colored_prim.vbo_offset+2)*sizeof(f32)));

            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1); 

            glDrawArrays(GL_LINES, 0, cmd.colored_prim.vertex_count);
            break;
        case GFX_COMMAND_GUI_PRIM_COLOR: 
            glBindBuffer(GL_ARRAY_BUFFER, cmd.gui_prim.vbo);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            glUseProgram(gfx.shaders.gui_prim.program);

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5*sizeof(f32), (void*)(cmd.gui_prim.vbo_offset*sizeof(f32)));
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5*sizeof(f32), (void*)((cmd.gui_prim.vbo_offset+2)*sizeof(f32)));
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1); 

            glUniform2f(gfx.shaders.gui_prim.resolution, gfx.resolution.x, gfx.resolution.y);

            glDrawArrays(GL_TRIANGLES, 0, cmd.gui_prim.vertex_count / 5);
            break;
        case GFX_COMMAND_GUI_PRIM_TEXTURE:
            glBindBuffer(GL_ARRAY_BUFFER, cmd.gui_prim_texture.vbo);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            glUseProgram(gfx.shaders.gui_prim_texture.program);

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(f32), (void*)(cmd.gui_prim_texture.vbo_offset*sizeof(f32)));
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(f32), (void*)((cmd.gui_prim_texture.vbo_offset+2)*sizeof(f32)));
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1); 

            glUniform2f(gfx.shaders.gui_prim_texture.resolution, gfx.resolution.x, gfx.resolution.y);
            glBindTexture(GL_TEXTURE_2D, cmd.gui_prim_texture.texture);

            glDrawArrays(GL_TRIANGLES, 0, cmd.gui_prim_texture.vertex_count / 4);
            break;
        case GFX_COMMAND_GUI_TEXT: 
            glBindBuffer(GL_ARRAY_BUFFER, cmd.gui_text.vbo);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            glUseProgram(gfx.shaders.text.program);

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

Vector3 rgb_unpack(u32 argb)
{
    Vector3 rgb;
    rgb.r = ((argb >> 16) & 0xFF) / 255.0f;
    rgb.g = ((argb >> 8) & 0xFF) / 255.0f;
    rgb.b = ((argb >> 0) & 0xFF) / 255.0f;
    return rgb;
}

Vector4 argb_unpack(u32 argb)
{
    Vector4 v;
    v.r = ((argb >> 16) & 0xFF) / 255.0f;
    v.g = ((argb >> 8) & 0xFF) / 255.0f;
    v.b = ((argb >> 0) & 0xFF) / 255.0f;
    v.a = ((argb >> 24) & 0xFF) / 255.0f;
    return v;
}

f32 linear_from_sRGB(f32 s)
{
    if (s <= 0.04045f) {
        return s / 12.92f;
    } else {
        return powf((s + 0.055f) / (1.055f), 2.4f);
    }
}

Vector3 linear_from_sRGB(Vector3 sRGB)
{
    Vector3 l;
    l.r = linear_from_sRGB(sRGB.r);
    l.g = linear_from_sRGB(sRGB.g);
    l.b = linear_from_sRGB(sRGB.b);
    return l;
}

Vector4 linear_from_sRGB(Vector4 sRGB)
{
    Vector4 l;
    l.r = linear_from_sRGB(sRGB.r);
    l.g = linear_from_sRGB(sRGB.g);
    l.b = linear_from_sRGB(sRGB.b);
    l.a = sRGB.a;
    return l;
}
    
GLuint gfx_create_texture(void *pixel_data, i32 width, i32 height)
{
    GLuint handle;
    glGenTextures(1, &handle);
    
    glBindTexture(GL_TEXTURE_2D, handle);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data);
    
    return handle;
}

GLuint gfx_load_texture(u8 *data, i32 size)
{
    i32 width, height, num_channels;
    u8 *pixel_data = stbi_load_from_memory(data, size, &width, &height, &num_channels, 4);
    return gfx_create_texture(pixel_data, width, height);
}

Matrix4 transform(Camera *camera)
{
    Matrix4 m = camera->projection;
    m[3].xyz -= (camera->projection * Vector4{ .xyz = camera->position, ._w = 1.0f }).xyz;
    m[0].x *= camera->uni_scale;
    m[1].y *= camera->uni_scale;
    m[2].z *= camera->uni_scale;
    return m;
}

Vector2 ws_from_ss(Vector2 ss_pos)
{
    Vector2 r;
    r.x = ss_pos.x - 0.5f*gfx.resolution.x;
    r.y = ss_pos.y - 0.5f*gfx.resolution.y;
    return r;
}
