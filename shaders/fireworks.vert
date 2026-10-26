#version 330 core

layout (location = 0) in vec2 corner;	// coin du carre (-1..1)
layout (location = 1) in vec3 pos;		// par instance
layout (location = 2) in vec4 data;		// rgb = couleur, a = taille

uniform mat4 view;
uniform mat4 projection;

out vec3 Color;
out vec2 Uv;

void main()
{
	// on construit le carre directement dans l'espace camera, il fait donc toujours face a l'ecran
	vec4 viewPos = view * vec4(pos, 1.0);
	viewPos.xy += corner * data.a;
	gl_Position = projection * viewPos;

	Color = data.rgb;
	Uv = corner;
}
