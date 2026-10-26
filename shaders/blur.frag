// flou gaussien separe (horizontal puis vertical)
#version 330 core

in vec2 TexCoord;

uniform sampler2D image;
uniform int horizontal;

out vec4 frag_color;

const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
	vec2 off = 1.0 / textureSize(image, 0);
	off = (horizontal == 1) ? vec2(off.x, 0.0) : vec2(0.0, off.y);

	vec3 result = texture(image, TexCoord).rgb * weight[0];
	for (int i = 1; i < 5; i++)
	{
		result += texture(image, TexCoord + off * i * 1.5).rgb * weight[i];
		result += texture(image, TexCoord - off * i * 1.5).rgb * weight[i];
	}
	frag_color = vec4(result, 1.0);
}
