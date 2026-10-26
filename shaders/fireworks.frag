#version 330 core

in vec3 Color;
in vec2 Uv;

out vec4 frag_color;

void main()
{
	float a = smoothstep(1.0, 0.0, length(Uv));
	frag_color = vec4(Color * a * a, 1.0);
}
