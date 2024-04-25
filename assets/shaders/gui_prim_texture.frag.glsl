in vec2 vs_uv;

out vec4 out_color;

uniform sampler2D sampler;

void main()
{
	out_color = texture(sampler, vs_uv);
}
