layout(location = 0) in vec2 v_pos;
layout(location = 1) in vec2 v_uv;

out vec2 vs_uv;

void main()
{
   gl_Position = vec4((cs_from_ws * vec3(v_pos, 1.0)).xy, 0.0, 1.0);
   vs_uv = v_uv;
}
