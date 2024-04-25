in vec2 vs_uv;
in vec3 vs_color;

out vec4 out_color;

uniform sampler2D atlas_sampler;

void main()
{
	out_color = vec4(vs_color.rgb, texture(atlas_sampler, vs_uv).r);
}

