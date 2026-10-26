#version 330 core

in vec2 TexCoord;

uniform sampler2D scene;
uniform float threshold;

out vec4 frag_color;

void main()
{
	vec3 c = texture(scene, TexCoord).rgb;
	float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
	frag_color = vec4(c * smoothstep(threshold, threshold + 0.6, lum), 1.0);
}
