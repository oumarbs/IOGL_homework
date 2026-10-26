#version 330 core

in vec2 TexCoord;

uniform sampler2D scene;
uniform sampler2D bloom;
uniform int bloomOn;
uniform float bloomStrength;
uniform float exposure;
uniform float fade;		// 0 = noir, 1 = image normale
uniform float bars;		// bandes noires "cinema" (0 a ~0.1)

out vec4 frag_color;

// approximation ACES (Narkowicz)
vec3 aces(vec3 x)
{
	return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
	vec3 hdr = texture(scene, TexCoord).rgb;
	if (bloomOn == 1)
		hdr += texture(bloom, TexCoord).rgb * bloomStrength;

	vec3 col = aces(hdr * exposure);
	col = pow(col, vec3(1.0 / 2.2));

	// vignette
	vec2 uv = TexCoord - 0.5;
	col *= 1.0 - dot(uv, uv) * 0.7;

	col *= fade;

	if (TexCoord.y < bars || TexCoord.y > 1.0 - bars)
		col = vec3(0.0);

	frag_color = vec4(col, 1.0);
}
