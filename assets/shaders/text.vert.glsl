layout(location = 0) in vec2 v_pos;
layout(location = 1) in vec2 v_uv;

uniform vec2 resolution;
uniform vec3 color;

out vec2 vs_uv;
out vec3 vs_color;

void main()
{
    gl_Position = vec4(v_pos.xy / resolution * 2.0 - 1.0, 0.0, 1.0);
    gl_Position.y = -gl_Position.y;
    vs_uv = v_uv;
    vs_color = color;
}
