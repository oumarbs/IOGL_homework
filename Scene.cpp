#include "Scene.h"
#include <string>
#include <cmath>
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/constants.hpp"

using glm::vec3;
using glm::mat4;

static mat4 placeAt(const vec3& pos, float yawDeg = 0.0f, const vec3& scale = vec3(1.0f))
{
	mat4 m = glm::translate(mat4(1.0f), pos);
	m = glm::rotate(m, glm::radians(yawDeg), vec3(0.0f, 1.0f, 0.0f));
	return glm::scale(m, scale);
}

// cylindre de rayon r tendu entre a et b (fils des guirlandes)
static mat4 segment(const vec3& a, const vec3& b, float r)
{
	vec3 d = b - a;
	float len = glm::length(d);
	vec3 dir = d / len;
	vec3 up(0.0f, 1.0f, 0.0f);

	mat4 m = glm::translate(mat4(1.0f), a);
	vec3 axis = glm::cross(up, dir);
	if (glm::length(axis) > 0.0001f)
		m = glm::rotate(m, acosf(glm::clamp(glm::dot(up, dir), -1.0f, 1.0f)), glm::normalize(axis));
	return glm::scale(m, vec3(r, len, r));
}

// texture a bandes verticales, 2 couleurs
static void makeStripes(Texture2D& tex, const vec3& c1, const vec3& c2, int stripes)
{
	const int w = 256, h = 4;
	std::vector<unsigned char> px(w * h * 4);
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			vec3 c = ((x * stripes / w) % 2 == 0) ? c1 : c2;
			unsigned char* p = &px[(y * w + x) * 4];
			p[0] = (unsigned char)(c.r * 255);
			p[1] = (unsigned char)(c.g * 255);
			p[2] = (unsigned char)(c.b * 255);
			p[3] = 255;
		}
	}
	tex.createFromPixels(w, h, &px[0]);
}


Scene::Scene()
	: mFlashPos(0.0f), mFlashColor(0.0f),
	  mTime(0.0f), mPaused(false), mTimeOfDay(0.70f), mTargetTime(0.70f), mDayCycle(false)
{
}

bool Scene::load()
{
	bool ok = true;

	ok &= mFloor.loadOBJ("models/floor.obj");
	ok &= mBarrel.loadOBJ("models/barrel.obj");
	ok &= mCrate.loadOBJ("models/crate.obj");
	ok &= mWoodCrate.loadOBJ("models/woodcrate.obj");
	ok &= mPin.loadOBJ("models/bowling_pin.obj");
	ok &= mBunny.loadOBJ("models/bunny.obj");
	ok &= mLamp.loadOBJ("models/lampPost.obj");
	ok &= mBulbMesh.loadOBJ("models/light.obj");

	// formes simples generees pour le manege
	mBall.createSphere(1.0f, 24, 32);
	mCylinder.createCylinder(1.0f, 1.0f, 32);
	mCone.createCone(1.0f, 1.0f, 32);

	ok &= mFloorTex.loadTexture("textures/tile_floor.jpg", true);
	ok &= mBarrelTex.loadTexture("textures/barrel_diffuse.png", true);
	ok &= mBarrelSpec.loadTexture("textures/barrel_spec.png", true);
	ok &= mCrateTex.loadTexture("textures/crate.jpg", true);
	ok &= mWoodCrateTex.loadTexture("textures/woodcrate_diffuse.jpg", true);
	ok &= mPinTex.loadTexture("textures/AMF.tga", true);
	ok &= mLampTex.loadTexture("textures/lamp_post_diffuse.png", true);
	mWhite.createColor(255, 255, 255);
	makeStripes(mRedStripes, vec3(0.75f, 0.08f, 0.08f), vec3(0.95f, 0.92f, 0.85f), 16);
	makeStripes(mBlueStripes, vec3(0.1f, 0.25f, 0.65f), vec3(0.95f, 0.95f, 0.9f), 10);

	mLampPositions.push_back(vec3(-10.0f, 0.0f,  9.0f));
	mLampPositions.push_back(vec3( 10.0f, 0.0f,  9.0f));
	mLampPositions.push_back(vec3(-10.0f, 0.0f, -9.0f));
	mLampPositions.push_back(vec3( 10.0f, 0.0f, -9.0f));

	computeEnvironment();
	buildObjects();

	return ok;
}

void Scene::toggleDayNight()
{
	// on vise midi ou minuit, la transition se fait dans update()
	mDayCycle = false;
	float goal = isNight() ? 0.25f : 0.75f;
	while (goal < mTimeOfDay)
		goal += 1.0f;
	mTargetTime = goal;
}

void Scene::setFireworkFlash(const vec3& pos, const vec3& color)
{
	mFlashPos = pos;
	mFlashColor = color;
}

void Scene::update(double dt)
{
	if (!mPaused)
		mTime += (float)dt;

	if (mDayCycle)
	{
		mTimeOfDay += (float)dt / 90.0f;	// un jour complet en 1min30
		mTargetTime = mTimeOfDay;
	}
	else if (mTimeOfDay < mTargetTime)
	{
		mTimeOfDay = glm::min(mTimeOfDay + (float)dt * 0.12f, mTargetTime);
	}

	if (mTimeOfDay >= 1.0f)
	{
		mTimeOfDay -= 1.0f;
		mTargetTime -= 1.0f;
	}

	computeEnvironment();
	buildObjects();
}

void Scene::computeEnvironment()
{
	float a = mTimeOfDay * glm::two_pi<float>();
	mEnv.sunDir = glm::normalize(vec3(cosf(a), sinf(a), -0.45f));
	mEnv.moonDir = glm::normalize(vec3(-cosf(a), -sinf(a), -0.3f));
	mEnv.day = glm::smoothstep(-0.08f, 0.22f, mEnv.sunDir.y);

	// coucher de soleil : plus orange quand le soleil est bas
	float sunset = expf(-fabsf(mEnv.sunDir.y) * 7.0f) * glm::smoothstep(-0.2f, 0.05f, mEnv.sunDir.y);

	if (mEnv.sunDir.y > 0.0f)
	{
		mEnv.lightDir = mEnv.sunDir;
		vec3 warm(1.0f, 0.55f, 0.3f), white(1.0f, 0.95f, 0.88f);
		mEnv.lightColor = glm::mix(warm, white, glm::smoothstep(0.0f, 0.4f, mEnv.sunDir.y)) * 2.2f
		                  * glm::smoothstep(0.0f, 0.1f, mEnv.sunDir.y);
	}
	else
	{
		mEnv.lightDir = mEnv.moonDir;
		mEnv.lightColor = vec3(0.22f, 0.28f, 0.45f) * glm::smoothstep(0.0f, 0.15f, mEnv.moonDir.y);
	}

	mEnv.ambient = glm::mix(vec3(0.02f, 0.025f, 0.05f), vec3(0.35f, 0.38f, 0.45f), mEnv.day);
	mEnv.zenith  = glm::mix(vec3(0.003f, 0.005f, 0.018f), vec3(0.12f, 0.3f, 0.7f), mEnv.day);
	mEnv.horizon = glm::mix(vec3(0.025f, 0.03f, 0.055f), vec3(0.6f, 0.7f, 0.85f), mEnv.day);
	mEnv.horizon += vec3(0.6f, 0.25f, 0.08f) * sunset;
}

void Scene::add(Mesh& mesh, Texture2D& tex, const mat4& model, const Material& mat, Texture2D* spec)
{
	SceneObject o;
	o.mesh = &mesh;
	o.diffuse = &tex;
	o.specMap = spec ? spec : &mWhite;
	o.material = mat;
	o.model = model;
	mObjects.push_back(o);
}

void Scene::addBulb(const vec3& pos, const vec3& color, float intensity, float scale)
{
	Bulb b;
	b.position = pos;
	b.color = color;
	b.intensity = intensity;
	b.scale = scale;
	mBulbs.push_back(b);
}

void Scene::addLight(const vec3& pos, const vec3& color, float linear, float exponent)
{
	PointLightData l;
	l.position = pos;
	l.color = color;
	l.linear = linear;
	l.exponent = exponent;
	mLights.push_back(l);
}

//-----------------------------------------------------------------------------
// Placement de tous les objets. Appele a chaque frame car une bonne
// partie est animee (manege, bowling, guirlandes...)
//-----------------------------------------------------------------------------
void Scene::buildObjects()
{
	float t = mTime;
	float night = 1.0f - mEnv.day;
	mObjects.clear();
	mBulbs.clear();
	mLights.clear();

	// sol
	Material floorMat(0.5f, 64.0f);
	floorMat.uvScale = 3.0f;
	add(mFloor, mFloorTex, placeAt(vec3(0.0f), 0.0f, vec3(6.0f, 1.0f, 6.0f)), floorMat);

	buildCarousel(t);
	buildStall();
	animateBowling(t);

	// lampadaires + guirlandes qui partent vers le haut du manege
	vec3 hub(0.0f, 5.0f, 0.0f);
	for (size_t i = 0; i < mLampPositions.size(); i++)
	{
		add(mLamp, mLampTex, placeAt(mLampPositions[i]), Material(0.4f, 32.0f));

		vec3 top = mLampPositions[i] + vec3(0.0f, 3.75f, 0.0f);
		float intensity = 3.0f * (1.0f - mEnv.day * 0.95f);

		// celui du fond a droite gresille
		if (i == 3)
		{
			float n = sinf(t * 13.0f) + sinf(t * 31.7f) + sinf(t * 7.3f);
			if (n > 1.4f)
				intensity *= 0.1f;
		}

		addBulb(top, vec3(1.0f, 0.68f, 0.35f), glm::max(intensity, 0.4f), 0.55f);
		addLight(top, vec3(1.0f, 0.68f, 0.35f) * intensity);

		buildGarland(mLampPositions[i] + vec3(0.0f, 4.1f, 0.0f), hub, 1.4f, (int)i);
	}
	// et deux sur les cotes
	buildGarland(mLampPositions[0] + vec3(0, 4.1f, 0), mLampPositions[2] + vec3(0, 4.1f, 0), 1.8f, 1);
	buildGarland(mLampPositions[1] + vec3(0, 4.1f, 0), mLampPositions[3] + vec3(0, 4.1f, 0), 1.8f, 2);

	// lumiere du feu d'artifice
	if (glm::length(mFlashColor) > 0.01f)
		addLight(mFlashPos, mFlashColor * 3.0f * night, 0.02f, 0.002f);
}

//-----------------------------------------------------------------------------
// Le manege : socle, colonne, toit en cone, lapins sur des barres
//-----------------------------------------------------------------------------
void Scene::buildCarousel(float t)
{
	float night = 1.0f - mEnv.day;
	float rot = t * 0.45f;	// radians
	float rotDeg = glm::degrees(rot);

	Material gold(1.0f, 96.0f);
	gold.tint = vec3(0.9f, 0.65f, 0.2f);

	Material red(0.4f, 32.0f);
	red.tint = vec3(0.55f, 0.06f, 0.06f);

	// socle (ne tourne pas) + plancher en bois
	add(mCylinder, mWhite, placeAt(vec3(0.0f), 0.0f, vec3(3.6f, 0.35f, 3.6f)), red);
	add(mCylinder, mWoodCrateTex, placeAt(vec3(0.0f, 0.35f, 0.0f), rotDeg, vec3(3.45f, 0.06f, 3.45f)), Material(0.1f, 8.0f));

	// colonne centrale rayee
	add(mCylinder, mRedStripes, placeAt(vec3(0.0f, 0.4f, 0.0f), rotDeg, vec3(0.45f, 3.0f, 0.45f)), Material(0.5f, 32.0f));

	// toit
	add(mCylinder, mWhite, placeAt(vec3(0.0f, 3.3f, 0.0f), rotDeg, vec3(3.9f, 0.35f, 3.9f)), gold);
	add(mCone, mRedStripes, placeAt(vec3(0.0f, 3.65f, 0.0f), rotDeg, vec3(3.9f, 1.5f, 3.9f)), Material(0.3f, 16.0f));
	add(mBall, mWhite, placeAt(vec3(0.0f, 5.25f, 0.0f), 0.0f, vec3(0.22f)), gold);

	// barres + lapins qui montent et descendent
	vec3 bunnyTint[4] = { vec3(0.95f, 0.95f, 0.92f), vec3(1.0f, 0.6f, 0.75f), vec3(0.6f, 0.8f, 1.0f), vec3(1.0f, 0.9f, 0.45f) };
	const int riders = 8;
	for (int i = 0; i < riders; i++)
	{
		float a = rot + i * glm::two_pi<float>() / riders;
		vec3 p(cosf(a) * 2.6f, 0.0f, -sinf(a) * 2.6f);

		add(mCylinder, mWhite, placeAt(p + vec3(0.0f, 0.4f, 0.0f), 0.0f, vec3(0.05f, 2.9f, 0.05f)), gold);

		float h = 0.75f + sinf(t * 2.2f + i * 1.3f) * 0.35f;
		Material fur(0.15f, 16.0f);
		fur.tint = bunnyTint[i % 4];
		// tangente au cercle : le lapin regarde dans le sens de la rotation
		add(mBunny, mWhite, placeAt(p + vec3(0.0f, h, 0.0f), glm::degrees(a), vec3(0.36f)), fur);
	}

	// ampoules autour du toit, effet "chenillard"
	const int rim = 28;
	int step = (int)(t * 6.0f);
	for (int i = 0; i < rim; i++)
	{
		float a = rot + i * glm::two_pi<float>() / rim;
		vec3 p(cosf(a) * 3.95f, 3.47f, -sinf(a) * 3.95f);
		bool on = ((i + step) % 3) != 0;
		vec3 c = (i % 2 == 0) ? vec3(1.0f, 0.85f, 0.5f) : vec3(1.0f, 0.4f, 0.6f);
		addBulb(p, c, on ? 3.5f * night + 0.6f : 0.3f, 0.2f);
	}

	// lumiere chaude sous le toit
	addLight(vec3(0.0f, 2.9f, 1.0f), vec3(1.0f, 0.75f, 0.45f) * 2.5f * (night * 0.9f + 0.1f), 0.22f, 0.08f);
}

//-----------------------------------------------------------------------------
// Petit stand a gauche : comptoir en caisses, tonneaux, auvent raye
//-----------------------------------------------------------------------------
void Scene::buildStall()
{
	float night = 1.0f - mEnv.day;

	Material crateMat(0.15f, 16.0f);
	add(mCrate, mCrateTex, placeAt(vec3(-8.0f, 0.0f, -1.2f), 0.0f, vec3(0.55f)), crateMat);
	add(mCrate, mCrateTex, placeAt(vec3(-8.0f, 0.0f,  0.0f), 0.0f, vec3(0.55f)), crateMat);
	add(mCrate, mCrateTex, placeAt(vec3(-8.0f, 0.0f,  1.2f), 0.0f, vec3(0.55f)), crateMat);

	// des quilles a gagner sur le comptoir
	Material pinMat(0.8f, 64.0f);
	for (int i = 0; i < 4; i++)
		add(mPin, mPinTex, placeAt(vec3(-8.0f, 1.1f, -1.2f + i * 0.8f), i * 30.0f, vec3(0.045f)), pinMat);

	Material barrelMat(1.0f, 48.0f);
	add(mBarrel, mBarrelTex, placeAt(vec3(-9.8f, 0.0f, -1.6f), 10.0f, vec3(0.5f)), barrelMat, &mBarrelSpec);
	add(mBarrel, mBarrelTex, placeAt(vec3(-9.7f, 0.0f,  1.3f), 70.0f, vec3(0.5f)), barrelMat, &mBarrelSpec);
	add(mBarrel, mBarrelTex, placeAt(vec3(-6.6f, 0.0f,  3.4f), 130.0f, vec3(0.45f)), barrelMat, &mBarrelSpec);

	Material wood(0.1f, 8.0f);
	add(mWoodCrate, mWoodCrateTex, placeAt(vec3(-10.0f, 0.0f, 3.6f), 15.0f, vec3(0.6f)), wood);
	add(mWoodCrate, mWoodCrateTex, placeAt(vec3(-10.0f, 1.2f, 3.6f), 40.0f, vec3(0.45f)), wood);
	add(mWoodCrate, mWoodCrateTex, placeAt(vec3(-7.0f, 0.0f, -3.8f), -25.0f, vec3(0.5f)), wood);

	// poteaux
	Material poleMat(0.3f, 32.0f);
	poleMat.tint = vec3(0.35f, 0.22f, 0.12f);
	add(mCylinder, mWhite, placeAt(vec3(-7.2f, 0.0f, -2.0f), 0.0f, vec3(0.06f, 2.6f, 0.06f)), poleMat);
	add(mCylinder, mWhite, placeAt(vec3(-7.2f, 0.0f,  2.0f), 0.0f, vec3(0.06f, 2.6f, 0.06f)), poleMat);
	add(mCylinder, mWhite, placeAt(vec3(-9.4f, 0.0f, -2.0f), 0.0f, vec3(0.06f, 3.2f, 0.06f)), poleMat);
	add(mCylinder, mWhite, placeAt(vec3(-9.4f, 0.0f,  2.0f), 0.0f, vec3(0.06f, 3.2f, 0.06f)), poleMat);

	// auvent : une caisse tres aplatie avec la texture rayee
	mat4 m = glm::translate(mat4(1.0f), vec3(-8.3f, 2.85f, 0.0f));
	m = glm::rotate(m, glm::radians(-15.0f), vec3(0.0f, 0.0f, 1.0f));
	m = glm::scale(m, vec3(1.3f, 0.03f, 2.15f));
	add(mCrate, mBlueStripes, m, Material(0.2f, 16.0f));

	vec3 lamp(-7.7f, 2.45f, 0.0f);
	addBulb(lamp, vec3(1.0f, 0.8f, 0.5f), 4.0f * night + 0.4f, 0.4f);
	addLight(lamp, vec3(1.0f, 0.75f, 0.45f) * 2.0f * night, 0.3f, 0.15f);
}

//-----------------------------------------------------------------------------
// Guirlande entre deux points : un fil qui pend + des ampoules colorees
//-----------------------------------------------------------------------------
void Scene::buildGarland(const vec3& a, const vec3& b, float sag, int colorOffset)
{
	static const vec3 colors[4] = {
		vec3(1.0f, 0.3f, 0.2f), vec3(1.0f, 0.85f, 0.3f), vec3(0.3f, 0.9f, 0.4f), vec3(0.35f, 0.5f, 1.0f)
	};
	float night = 1.0f - mEnv.day;
	const int n = 16;

	Material wire(0.1f, 8.0f);
	wire.tint = vec3(0.05f);

	vec3 prev = a;
	for (int i = 1; i <= n; i++)
	{
		float s = (float)i / n;
		vec3 p = glm::mix(a, b, s) - vec3(0.0f, sag * 4.0f * s * (1.0f - s), 0.0f);
		add(mCylinder, mWhite, segment(prev, p, 0.015f), wire);

		if (i < n)
		{
			// petit scintillement different pour chaque ampoule
			float twinkle = 0.75f + 0.25f * sinf(mTime * 3.0f + i * 1.7f + colorOffset);
			addBulb(p - vec3(0.0f, 0.08f, 0.0f), colors[(i + colorOffset) % 4], (3.0f * night + 0.3f) * twinkle, 0.16f);
		}
		prev = p;
	}
}

//-----------------------------------------------------------------------------
// Bowling : la boule part, fait tomber les quilles, revient par le cote
// et les quilles se relevent. Cycle de 10 secondes.
//-----------------------------------------------------------------------------
void Scene::animateBowling(float t)
{
	const float period = 10.0f;
	const float laneX = 7.0f, returnX = 8.6f;
	const float headZ = -3.0f;
	const float r = 0.35f;
	float p = fmodf(t, period);

	vec3 ball;
	if (p < 2.5f)
		ball = vec3(laneX, r, glm::mix(6.0f, headZ, p / 2.5f));
	else if (p < 4.0f)
	{
		float k = (p - 2.5f) / 1.5f;
		k = 1.0f - (1.0f - k) * (1.0f - k);	// ralentit
		ball = vec3(laneX, r, glm::mix(headZ, -7.5f, k));
	}
	else if (p < 4.6f)
		ball = vec3(glm::mix(laneX, returnX, glm::smoothstep(4.0f, 4.6f, p)), r, -7.5f);
	else if (p < 9.0f)
		ball = vec3(returnX, r, glm::mix(-7.5f, 6.0f, glm::smoothstep(4.6f, 9.0f, p)));
	else
		ball = vec3(glm::mix(returnX, laneX, glm::smoothstep(9.0f, 10.0f, p)), r, 6.0f);

	// roulement : angle = distance / rayon
	mat4 m = glm::translate(mat4(1.0f), ball);
	m = glm::rotate(m, ball.z / r, vec3(1, 0, 0));
	m = glm::scale(m, vec3(r));

	Material shiny(1.0f, 128.0f);
	shiny.tint = vec3(0.08f, 0.12f, 0.55f);
	add(mBall, mWhite, m, shiny);

	// direction de chute "aleatoire" mais toujours la meme
	static const float fallAngle[10] = { 0.0f, -35.0f, 30.0f, -60.0f, 10.0f, 55.0f, -80.0f, -20.0f, 25.0f, 75.0f };

	Material pinMat(0.8f, 64.0f);
	int idx = 0;
	for (int row = 0; row < 4; row++)
	{
		for (int j = 0; j <= row; j++, idx++)
		{
			vec3 pos(laneX + (j - row * 0.5f) * 0.55f, 0.0f, headZ - row * 0.5f);

			float hit = 2.5f + row * 0.12f + (idx % 3) * 0.04f;
			float fall = glm::smoothstep(hit, hit + 0.4f, p) * (1.0f - glm::smoothstep(7.5f, 8.5f, p));

			float a = glm::radians(fallAngle[idx] + 180.0f);
			vec3 dir(sinf(a), 0.0f, cosf(a));
			vec3 axis = glm::cross(vec3(0, 1, 0), dir);
			float tilt = fall * glm::radians(88.0f);

			pos += dir * fall * 0.5f;
			pos.y += sinf(tilt) * 0.22f;	// sinon la quille couchee rentre dans le sol

			mat4 pm = glm::translate(mat4(1.0f), pos);
			pm = glm::rotate(pm, tilt, axis);
			pm = glm::scale(pm, vec3(0.1f));
			add(mPin, mPinTex, pm, pinMat);
		}
	}
}

void Scene::draw(ShaderProgram& shader, bool depthOnly)
{
	for (size_t i = 0; i < mObjects.size(); i++)
	{
		SceneObject& o = mObjects[i];
		shader.setUniform("model", o.model);

		if (!depthOnly)
		{
			shader.setUniform("uvScale", o.material.uvScale);
			shader.setUniform("material.specular", o.material.specular);
			shader.setUniform("material.shininess", o.material.shininess);
			shader.setUniform("material.tint", o.material.tint);
			shader.setUniform("material.emissive", o.material.emissive);
			shader.setUniformSampler("material.diffuseMap", 0);
			o.diffuse->bind(0);
			shader.setUniformSampler("material.specularMap", 1);
			o.specMap->bind(1);
		}

		o.mesh->draw();
	}
}

void Scene::drawBulbs(ShaderProgram& shader)
{
	for (size_t i = 0; i < mBulbs.size(); i++)
	{
		mat4 m = glm::translate(mat4(1.0f), mBulbs[i].position);
		m = glm::scale(m, vec3(mBulbs[i].scale));
		shader.setUniform("model", m);
		shader.setUniform("lightColor", mBulbs[i].color);
		shader.setUniform("intensity", mBulbs[i].intensity);
		mBulbMesh.draw();
	}
}

void Scene::setLights(ShaderProgram& shader, const vec3& camPos, const vec3& camLook, bool flashlight)
{
	shader.setUniform("sunLight.direction", -mEnv.lightDir);
	shader.setUniform("sunLight.ambient", mEnv.ambient);
	shader.setUniform("sunLight.diffuse", mEnv.lightColor);
	shader.setUniform("sunLight.specular", mEnv.lightColor);

	const size_t maxLights = 8;		// MAX_POINT_LIGHTS dans scene.frag
	size_t count = glm::min(mLights.size(), maxLights);
	shader.setUniform("numPointLights", (int)count);
	for (size_t i = 0; i < count; i++)
	{
		std::string base = "pointLights[" + std::to_string(i) + "]";
		shader.setUniform((base + ".position").c_str(), mLights[i].position);
		shader.setUniform((base + ".color").c_str(), mLights[i].color);
		shader.setUniform((base + ".constant").c_str(), 1.0f);
		shader.setUniform((base + ".linear").c_str(), mLights[i].linear);
		shader.setUniform((base + ".exponent").c_str(), mLights[i].exponent);
	}

	// spot 0 : lampe torche de la camera
	shader.setUniform("spotLights[0].position", camPos - vec3(0.0f, 0.3f, 0.0f));
	shader.setUniform("spotLights[0].direction", camLook);
	shader.setUniform("spotLights[0].color", vec3(1.0f, 0.97f, 0.9f) * 1.6f);
	shader.setUniform("spotLights[0].cosInnerCone", glm::cos(glm::radians(12.0f)));
	shader.setUniform("spotLights[0].cosOuterCone", glm::cos(glm::radians(18.0f)));
	shader.setUniform("spotLights[0].constant", 1.0f);
	shader.setUniform("spotLights[0].linear", 0.07f);
	shader.setUniform("spotLights[0].exponent", 0.017f);
	shader.setUniform("spotLights[0].on", flashlight ? 1 : 0);

	// spot 1 : projecteur sur le manege, il balaye doucement
	vec3 spotPos(0.0f, 11.0f, 9.0f);
	vec3 spotTarget(sinf(mTime * 0.5f) * 1.5f, 1.0f, 0.0f);
	shader.setUniform("spotLights[1].position", spotPos);
	shader.setUniform("spotLights[1].direction", glm::normalize(spotTarget - spotPos));
	shader.setUniform("spotLights[1].color", vec3(0.75f, 0.85f, 1.0f) * 3.5f * (1.0f - mEnv.day * 0.8f));
	shader.setUniform("spotLights[1].cosInnerCone", glm::cos(glm::radians(14.0f)));
	shader.setUniform("spotLights[1].cosOuterCone", glm::cos(glm::radians(20.0f)));
	shader.setUniform("spotLights[1].constant", 1.0f);
	shader.setUniform("spotLights[1].linear", 0.04f);
	shader.setUniform("spotLights[1].exponent", 0.006f);
	shader.setUniform("spotLights[1].on", 1);
}

glm::mat4 Scene::getLightSpaceMatrix() const
{
	mat4 proj = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 1.0f, 80.0f);
	mat4 view = glm::lookAt(mEnv.lightDir * 40.0f, vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
	return proj * view;
}
