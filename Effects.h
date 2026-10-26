//-----------------------------------------------------------------------------
// Petits effets en plus du cours : shadow map, post-process (bloom),
// ciel procedural et feu d'artifice
//-----------------------------------------------------------------------------
#ifndef EFFECTS_H
#define EFFECTS_H

#include <vector>
#define GLEW_STATIC
#include "GL/glew.h"
#include "glm/glm.hpp"
#include "ShaderProgram.h"


class ShadowMap
{
public:
	ShadowMap();
	~ShadowMap();

	bool init(int size);
	void begin();
	void end(int screenW, int screenH);
	void bindTexture(GLuint unit);

private:
	int mSize;
	GLuint mFBO, mDepth;
};


class PostFX
{
public:
	PostFX();
	~PostFX();

	bool init(int width, int height);
	void resize(int width, int height);

	void beginScene();	// on dessine dans le FBO HDR
	void render(bool bloom, float exposure, float fade, float bars);

private:
	void createTargets();
	void deleteTargets();

	int mWidth, mHeight;
	GLuint mSceneFBO, mSceneTex, mSceneDepth;
	GLuint mBlurFBO[2], mBlurTex[2];

	ShaderProgram mBright, mBlur, mFinal;
};


class Sky
{
public:
	bool init();
	void draw(const glm::mat4& view, const glm::mat4& projection,
	          const glm::vec3& zenith, const glm::vec3& horizon,
	          const glm::vec3& sunDir, const glm::vec3& moonDir, float night, float time);

private:
	ShaderProgram mShader;
};


// feu d'artifice : des fusees montent puis eclatent en particules
class Fireworks
{
public:
	Fireworks();
	~Fireworks();

	bool init();
	void update(float dt, float spawnRate);
	void draw(const glm::mat4& view, const glm::mat4& projection);

	// derniere explosion, pour eclairer la scene avec une point light
	glm::vec3 getFlashPos() const { return mFlashPos; }
	glm::vec3 getFlashColor() const { return mFlashColor * mFlash; }

private:
	struct Particle
	{
		glm::vec3 pos;
		glm::vec3 vel;
		glm::vec3 color;
		float life;
		float maxLife;
		bool rocket;
	};

	struct GpuPoint
	{
		glm::vec3 pos;
		glm::vec4 color;	// rgb + taille (en unites du monde)
	};

	void launch();
	void explode(const Particle& rocket);

	std::vector<Particle> mParticles;
	std::vector<GpuPoint> mData;
	float mNextLaunch;
	glm::vec3 mFlashPos, mFlashColor;
	float mFlash;

	GLuint mVAO, mVBO, mQuadVBO;
	ShaderProgram mShader;
};

#endif // EFFECTS_H
