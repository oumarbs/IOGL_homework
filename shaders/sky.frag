#version 330 core

in vec2 TexCoord;

uniform mat4 invViewProj;	// sans la translation de la camera
uniform vec3 zenithColor;
uniform vec3 horizonColor;
uniform vec3 sunDir;		// vers le soleil
uniform vec3 moonDir;
uniform float night;		// 0 = jour, 1 = nuit
uniform float time;

out vec4 frag_color;

float hash(vec3 p)
{
	p = fract(p * 0.3183099 + 0.1);
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

void main()
{
	vec4 ndc = vec4(TexCoord * 2.0 - 1.0, 1.0, 1.0);
	vec4 world = invViewProj * ndc;
	vec3 dir = normalize(world.xyz / world.w);

	float h = max(dir.y, 0.0);
	vec3 col = mix(horizonColor, zenithColor, pow(h, 0.45));

	// soleil + halo
	float s = max(dot(dir, sunDir), 0.0);
	col += vec3(1.0, 0.85, 0.6) * pow(s, 400.0) * 12.0 * (1.0 - night);
	col += vec3(1.0, 0.6, 0.3) * pow(s, 8.0) * 0.25 * (1.0 - night);

	// lune
	float m = max(dot(dir, moonDir), 0.0);
	col += vec3(0.8, 0.85, 1.0) * smoothstep(0.9993, 0.9996, m) * 3.0 * night;
	col += vec3(0.3, 0.35, 0.5) * pow(m, 60.0) * 0.15 * night;

	// etoiles : une case sur ~500 contient une etoile
	if (dir.y > 0.0 && night > 0.01)
	{
		vec3 p = dir * 220.0;
		vec3 cell = floor(p);
		float r = hash(cell);
		if (r > 0.998)
		{
			float d = length(fract(p) - 0.5);
			float twinkle = 0.6 + 0.4 * sin(time * (2.0 + r * 10.0) + r * 100.0);
			col += vec3(1.0) * smoothstep(0.35, 0.0, d) * twinkle * night * smoothstep(0.0, 0.25, dir.y) * 2.5;
		}
	}

	// sous l'horizon on reste sur la couleur du brouillard
	if (dir.y < 0.0)
		col = horizonColor;

	frag_color = vec4(col, 1.0);
}
