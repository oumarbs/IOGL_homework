//-----------------------------------------------------------------------------
// Blinn-Phong : 1 lumiere directionnelle (soleil/lune) avec ombre,
// N point lights, 2 spots, + brouillard.
// La sortie est en HDR lineaire, le tone mapping se fait dans final.frag
//-----------------------------------------------------------------------------
#version 330 core

struct Material
{
	sampler2D diffuseMap;
	sampler2D specularMap;
	vec3 specular;
	float shininess;
	vec3 tint;
	vec3 emissive;
};

struct DirectionalLight
{
	vec3 direction;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};

struct PointLight
{
	vec3 position;
	vec3 color;
	float constant;
	float linear;
	float exponent;
};

struct SpotLight
{
	vec3 position;
	vec3 direction;
	vec3 color;
	float cosInnerCone;
	float cosOuterCone;
	float constant;
	float linear;
	float exponent;
	int on;
};

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 2

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FragPosLight;

uniform DirectionalLight sunLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int numPointLights;
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];
uniform Material material;
uniform vec3 viewPos;

uniform sampler2D shadowMap;
uniform int shadowsOn;

uniform vec3 fogColor;
uniform float fogDensity;

out vec4 frag_color;

vec3 albedo;
vec3 specMask;

vec3 blinnPhong(vec3 lightDir, vec3 lightColor, vec3 specColor, vec3 normal, vec3 viewDir)
{
	float NdotL = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = lightColor * NdotL * albedo;

	vec3 halfDir = normalize(lightDir + viewDir);
	float NdotH = max(dot(normal, halfDir), 0.0);
	vec3 specular = specColor * material.specular * specMask * pow(NdotH, material.shininess);

	// pas de reflet si la lumiere est derriere la surface
	if (NdotL <= 0.0)
		specular = vec3(0.0);

	return diffuse + specular;
}

float attenuation(vec3 lightPos, float kc, float kl, float kq)
{
	float d = length(lightPos - FragPos);
	return 1.0 / (kc + kl * d + kq * d * d);
}

float shadowFactor(vec3 normal, vec3 lightDir)
{
	vec3 p = FragPosLight.xyz / FragPosLight.w;
	p = p * 0.5 + 0.5;

	if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0)
		return 0.0;

	float bias = max(0.004 * (1.0 - dot(normal, lightDir)), 0.0008);

	// PCF 3x3 pour adoucir les bords
	float shadow = 0.0;
	vec2 texel = 1.0 / textureSize(shadowMap, 0);
	for (int x = -1; x <= 1; x++)
	{
		for (int y = -1; y <= 1; y++)
		{
			float closest = texture(shadowMap, p.xy + vec2(x, y) * texel).r;
			shadow += (p.z - bias > closest) ? 1.0 : 0.0;
		}
	}
	return shadow / 9.0;
}

void main()
{
	vec3 normal = normalize(Normal);
	if (!gl_FrontFacing)
		normal = -normal;

	vec3 viewDir = normalize(viewPos - FragPos);

	// les textures sont en sRGB -> on repasse en lineaire
	albedo = pow(texture(material.diffuseMap, TexCoord).rgb, vec3(2.2)) * material.tint;
	specMask = texture(material.specularMap, TexCoord).rgb;

	// ambiant "hemispherique" : un peu plus clair vers le ciel
	float up = normal.y * 0.5 + 0.5;
	vec3 color = sunLight.ambient * mix(0.4, 1.0, up) * albedo;

	vec3 sunDir = normalize(-sunLight.direction);
	float shadow = (shadowsOn == 1) ? shadowFactor(normal, sunDir) : 0.0;
	color += (1.0 - shadow) * blinnPhong(sunDir, sunLight.diffuse, sunLight.specular, normal, viewDir);

	for (int i = 0; i < numPointLights; i++)
	{
		vec3 L = normalize(pointLights[i].position - FragPos);
		float att = attenuation(pointLights[i].position, pointLights[i].constant, pointLights[i].linear, pointLights[i].exponent);
		color += att * blinnPhong(L, pointLights[i].color, pointLights[i].color, normal, viewDir);
	}

	for (int i = 0; i < MAX_SPOT_LIGHTS; i++)
	{
		if (spotLights[i].on == 0)
			continue;

		vec3 L = normalize(spotLights[i].position - FragPos);
		float cosDir = dot(-L, normalize(spotLights[i].direction));
		float cone = smoothstep(spotLights[i].cosOuterCone, spotLights[i].cosInnerCone, cosDir);
		float att = attenuation(spotLights[i].position, spotLights[i].constant, spotLights[i].linear, spotLights[i].exponent);
		color += cone * att * blinnPhong(L, spotLights[i].color, spotLights[i].color, normal, viewDir);
	}

	color += material.emissive;

	// brouillard exponentiel au carre
	float dist = length(viewPos - FragPos);
	float fog = exp(-pow(dist * fogDensity, 2.0));
	color = mix(fogColor, color, clamp(fog, 0.0, 1.0));

	frag_color = vec4(color, 1.0);
}
