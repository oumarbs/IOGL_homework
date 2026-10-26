//-----------------------------------------------------------------------------
// La scene : petite fete foraine de nuit.
// Chargement des modeles, placement, animations et lumieres
//-----------------------------------------------------------------------------
#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include "glm/glm.hpp"
#include "Mesh.h"
#include "Texture2D.h"
#include "ShaderProgram.h"


struct Material
{
	glm::vec3 specular;
	float shininess;
	glm::vec3 tint;
	glm::vec3 emissive;
	float uvScale;

	Material(float spec = 0.3f, float shin = 32.0f)
		: specular(spec), shininess(shin), tint(1.0f), emissive(0.0f), uvScale(1.0f) {}
};

struct SceneObject
{
	Mesh* mesh;
	Texture2D* diffuse;
	Texture2D* specMap;
	Material material;
	glm::mat4 model;
};

// ampoule visible (juste dessinee, n'eclaire pas)
struct Bulb
{
	glm::vec3 position;
	glm::vec3 color;
	float intensity;
	float scale;
};

struct PointLightData
{
	glm::vec3 position;
	glm::vec3 color;
	float linear;
	float exponent;
};

// tout ce qui depend de l'heure de la journee
struct Environment
{
	glm::vec3 sunDir;		// direction VERS le soleil
	glm::vec3 moonDir;
	glm::vec3 lightDir;		// soleil ou lune, celle qui est levee
	glm::vec3 lightColor;
	glm::vec3 ambient;
	glm::vec3 zenith;
	glm::vec3 horizon;
	float day;				// 0 nuit -> 1 jour
};


class Scene
{
public:
	Scene();

	bool load();
	void update(double dt);

	void draw(ShaderProgram& shader, bool depthOnly);
	void drawBulbs(ShaderProgram& shader);
	void setLights(ShaderProgram& shader, const glm::vec3& camPos, const glm::vec3& camLook, bool flashlight);
	void setFireworkFlash(const glm::vec3& pos, const glm::vec3& color);

	glm::mat4 getLightSpaceMatrix() const;
	const Environment& getEnv() const { return mEnv; }
	float getTime() const { return mTime; }

	void togglePause() { mPaused = !mPaused; }
	void toggleDayNight();
	void toggleDayCycle() { mDayCycle = !mDayCycle; }
	bool isNight() const { return mEnv.day < 0.5f; }

private:
	void computeEnvironment();
	void buildObjects();

	void add(Mesh& mesh, Texture2D& tex, const glm::mat4& model, const Material& mat = Material(), Texture2D* spec = NULL);
	void addBulb(const glm::vec3& pos, const glm::vec3& color, float intensity, float scale);
	void addLight(const glm::vec3& pos, const glm::vec3& color, float linear = 0.35f, float exponent = 0.12f);

	void buildCarousel(float t);
	void buildStall();
	void buildGarland(const glm::vec3& a, const glm::vec3& b, float sag, int colorOffset);
	void animateBowling(float t);

	// modeles
	Mesh mFloor, mBarrel, mCrate, mWoodCrate, mPin, mBunny, mLamp, mBulbMesh;
	Mesh mBall, mCylinder, mCone;

	// textures
	Texture2D mFloorTex, mBarrelTex, mBarrelSpec, mCrateTex, mWoodCrateTex;
	Texture2D mPinTex, mLampTex, mWhite;
	Texture2D mRedStripes, mBlueStripes;

	std::vector<SceneObject> mObjects;
	std::vector<Bulb> mBulbs;
	std::vector<PointLightData> mLights;
	std::vector<glm::vec3> mLampPositions;

	glm::vec3 mFlashPos, mFlashColor;

	Environment mEnv;

	float mTime;
	bool mPaused;
	float mTimeOfDay;	// 0 = lever, 0.25 = midi, 0.5 = coucher, 0.75 = minuit
	float mTargetTime;
	bool mDayCycle;
};

#endif
