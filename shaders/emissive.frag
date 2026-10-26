// ampoules : couleur pure, > 1.0 pour que le bloom les fasse briller
#version 330 core

in vec3 FragPos;

uniform vec3 lightColor;
uniform float intensity;
uniform vec3 viewPos;
uniform vec3 fogColor;
uniform float fogDensity;

out vec4 frag_color;

void main()
{
	vec3 color = lightColor * intensity;
	float fog = exp(-pow(length(viewPos - FragPos) * fogDensity, 2.0));
	frag_color = vec4(mix(fogColor, color, clamp(fog, 0.0, 1.0)), 1.0);
}
