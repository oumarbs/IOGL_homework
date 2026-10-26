#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpace;
uniform float uvScale;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 FragPosLight;

void main()
{
	vec4 world = model * vec4(pos, 1.0);
	FragPos = world.xyz;
	Normal = mat3(transpose(inverse(model))) * normal;
	TexCoord = texCoord * uvScale;
	FragPosLight = lightSpace * world;

	gl_Position = projection * view * world;
}
