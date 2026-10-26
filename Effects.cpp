#include "Effects.h"
#include <iostream>
#include <cstdlib>
#include "glm/gtc/matrix_transform.hpp"


// VAO vide pour le triangle plein ecran (le core profile en exige un)
static void drawFullscreen()
{
	static GLuint vao = 0;
	if (vao == 0)
		glGenVertexArrays(1, &vao);

	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
}

static float randf(float a, float b)
{
	return a + (b - a) * (rand() / (float)RAND_MAX);
}


//-----------------------------------------------------------------------------
// ShadowMap
//-----------------------------------------------------------------------------
ShadowMap::ShadowMap()
	: mSize(0), mFBO(0), mDepth(0)
{}

ShadowMap::~ShadowMap()
{
	glDeleteFramebuffers(1, &mFBO);
	glDeleteTextures(1, &mDepth);
}

bool ShadowMap::init(int size)
{
	mSize = size;

	glGenTextures(1, &mDepth);
	glBindTexture(GL_TEXTURE_2D, mDepth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

	glGenFramebuffers(1, &mFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mDepth, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	if (!ok)
		std::cerr << "Shadow map FBO incomplete" << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return ok;
}

void ShadowMap::begin()
{
	glViewport(0, 0, mSize, mSize);
	glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
	glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::end(int screenW, int screenH)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, screenW, screenH);
}

void ShadowMap::bindTexture(GLuint unit)
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, mDepth);
}


//-----------------------------------------------------------------------------
// PostFX : scene en HDR -> extraction des zones claires -> flou -> composition
//-----------------------------------------------------------------------------
PostFX::PostFX()
	: mWidth(0), mHeight(0), mSceneFBO(0), mSceneTex(0), mSceneDepth(0)
{
	mBlurFBO[0] = mBlurFBO[1] = 0;
	mBlurTex[0] = mBlurTex[1] = 0;
}

PostFX::~PostFX()
{
	deleteTargets();
}

bool PostFX::init(int width, int height)
{
	mBright.loadShaders("shaders/fullscreen.vert", "shaders/bright.frag");
	mBlur.loadShaders("shaders/fullscreen.vert", "shaders/blur.frag");
	mFinal.loadShaders("shaders/fullscreen.vert", "shaders/final.frag");

	mWidth = width;
	mHeight = height;
	createTargets();

	return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void PostFX::resize(int width, int height)
{
	if (width <= 0 || height <= 0 || (width == mWidth && height == mHeight))
		return;

	mWidth = width;
	mHeight = height;
	deleteTargets();
	createTargets();
}

void PostFX::createTargets()
{
	// FBO principal en flottant pour garder les valeurs > 1
	glGenFramebuffers(1, &mSceneFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, mSceneFBO);

	glGenTextures(1, &mSceneTex);
	glBindTexture(GL_TEXTURE_2D, mSceneTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, mWidth, mHeight, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mSceneTex, 0);

	glGenRenderbuffers(1, &mSceneDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, mSceneDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mWidth, mHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mSceneDepth);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "HDR FBO incomplete" << std::endl;

	// le flou se fait en demi resolution, c'est largement suffisant
	int bw = mWidth / 2 > 0 ? mWidth / 2 : 1;
	int bh = mHeight / 2 > 0 ? mHeight / 2 : 1;

	glGenFramebuffers(2, mBlurFBO);
	glGenTextures(2, mBlurTex);
	for (int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mBlurFBO[i]);
		glBindTexture(GL_TEXTURE_2D, mBlurTex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bw, bh, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mBlurTex[i], 0);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostFX::deleteTargets()
{
	glDeleteFramebuffers(1, &mSceneFBO);
	glDeleteTextures(1, &mSceneTex);
	glDeleteRenderbuffers(1, &mSceneDepth);
	glDeleteFramebuffers(2, mBlurFBO);
	glDeleteTextures(2, mBlurTex);
}

void PostFX::beginScene()
{
	glBindFramebuffer(GL_FRAMEBUFFER, mSceneFBO);
	glViewport(0, 0, mWidth, mHeight);
}

void PostFX::render(bool bloom, float exposure, float fade, float bars)
{
	glDisable(GL_DEPTH_TEST);

	if (bloom)
	{
		glViewport(0, 0, mWidth / 2, mHeight / 2);

		glBindFramebuffer(GL_FRAMEBUFFER, mBlurFBO[0]);
		mBright.use();
		mBright.setUniform("threshold", 1.0f);
		mBright.setUniformSampler("scene", 0);
		glBindTexture(GL_TEXTURE_2D, mSceneTex);
		drawFullscreen();

		// ping-pong entre les 2 textures
		mBlur.use();
		mBlur.setUniformSampler("image", 0);
		bool horizontal = true;
		for (int i = 0; i < 8; i++)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, mBlurFBO[horizontal ? 1 : 0]);
			mBlur.setUniform("horizontal", horizontal ? 1 : 0);
			glBindTexture(GL_TEXTURE_2D, mBlurTex[horizontal ? 0 : 1]);
			drawFullscreen();
			horizontal = !horizontal;
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, mWidth, mHeight);

	mFinal.use();
	mFinal.setUniformSampler("scene", 0);
	glBindTexture(GL_TEXTURE_2D, mSceneTex);
	mFinal.setUniformSampler("bloom", 1);
	glBindTexture(GL_TEXTURE_2D, mBlurTex[0]);
	mFinal.setUniform("bloomOn", bloom ? 1 : 0);
	mFinal.setUniform("bloomStrength", 0.6f);
	mFinal.setUniform("exposure", exposure);
	mFinal.setUniform("fade", fade);
	mFinal.setUniform("bars", bars);
	drawFullscreen();

	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_DEPTH_TEST);
}


//-----------------------------------------------------------------------------
// Sky
//-----------------------------------------------------------------------------
bool Sky::init()
{
	return mShader.loadShaders("shaders/fullscreen.vert", "shaders/sky.frag");
}

void Sky::draw(const glm::mat4& view, const glm::mat4& projection,
               const glm::vec3& zenith, const glm::vec3& horizon,
               const glm::vec3& sunDir, const glm::vec3& moonDir, float night, float time)
{
	// on enleve la translation : le ciel est "a l'infini"
	glm::mat4 rotOnly = glm::mat4(glm::mat3(view));
	glm::mat4 inv = glm::inverse(projection * rotOnly);

	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);

	mShader.use();
	mShader.setUniform("invViewProj", inv);
	mShader.setUniform("zenithColor", zenith);
	mShader.setUniform("horizonColor", horizon);
	mShader.setUniform("sunDir", sunDir);
	mShader.setUniform("moonDir", moonDir);
	mShader.setUniform("night", night);
	mShader.setUniform("time", time);
	drawFullscreen();

	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
}


//-----------------------------------------------------------------------------
// Fireworks
//-----------------------------------------------------------------------------
static const int MAX_PARTICLES = 6000;
static const glm::vec3 GRAVITY(0.0f, -4.5f, 0.0f);

Fireworks::Fireworks()
	: mNextLaunch(1.0f), mFlashPos(0.0f), mFlashColor(0.0f), mFlash(0.0f), mVAO(0), mVBO(0), mQuadVBO(0)
{}

Fireworks::~Fireworks()
{
	glDeleteVertexArrays(1, &mVAO);
	glDeleteBuffers(1, &mVBO);
	glDeleteBuffers(1, &mQuadVBO);
}

bool Fireworks::init()
{
	if (!mShader.loadShaders("shaders/fireworks.vert", "shaders/fireworks.frag"))
		return false;

	mParticles.reserve(MAX_PARTICLES);
	mData.reserve(MAX_PARTICLES);

	// chaque particule = un petit carre face camera (instancing).
	// GL_POINTS marchait mal avec le driver D3D12 de WSL
	float quad[] = { -1.0f, -1.0f,  1.0f, -1.0f,  -1.0f, 1.0f,  1.0f, 1.0f };

	glGenVertexArrays(1, &mVAO);
	glBindVertexArray(mVAO);

	glGenBuffers(1, &mQuadVBO);
	glBindBuffer(GL_ARRAY_BUFFER, mQuadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &mVBO);
	glBindBuffer(GL_ARRAY_BUFFER, mVBO);
	glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * sizeof(GpuPoint), NULL, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GpuPoint), (GLvoid*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribDivisor(1, 1);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GpuPoint), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);
	glVertexAttribDivisor(2, 1);

	glBindVertexArray(0);

	return true;
}

void Fireworks::launch()
{
	if (mParticles.size() >= MAX_PARTICLES)
		return;

	// les fusees partent de derriere la fete
	Particle p;
	p.pos = glm::vec3(randf(-18.0f, 18.0f), 0.0f, randf(-32.0f, -22.0f));
	p.vel = glm::vec3(randf(-1.5f, 1.5f), randf(11.0f, 14.0f), randf(-1.0f, 1.0f));
	p.color = glm::vec3(1.0f, 0.7f, 0.4f);
	p.life = p.maxLife = randf(1.1f, 1.6f);
	p.rocket = true;
	mParticles.push_back(p);
}

void Fireworks::explode(const Particle& rocket)
{
	static const glm::vec3 palette[] = {
		glm::vec3(1.0f, 0.2f, 0.15f), glm::vec3(0.2f, 1.0f, 0.3f), glm::vec3(0.3f, 0.5f, 1.0f),
		glm::vec3(1.0f, 0.85f, 0.2f), glm::vec3(1.0f, 0.3f, 0.9f), glm::vec3(0.3f, 1.0f, 1.0f)
	};
	glm::vec3 color = palette[rand() % 6];
	float power = randf(5.0f, 8.0f);
	int count = 160 + rand() % 120;

	for (int i = 0; i < count && mParticles.size() < MAX_PARTICLES; i++)
	{
		// direction au hasard sur une sphere
		float z = randf(-1.0f, 1.0f);
		float a = randf(0.0f, 6.2831f);
		float r = sqrtf(1.0f - z * z);
		glm::vec3 dir(r * cosf(a), z, r * sinf(a));

		Particle p;
		p.pos = rocket.pos;
		p.vel = rocket.vel * 0.3f + dir * power * randf(0.85f, 1.0f);
		p.color = (rand() % 8 == 0) ? glm::vec3(1.0f) : color;
		p.life = p.maxLife = randf(1.4f, 2.2f);
		p.rocket = false;
		mParticles.push_back(p);
	}

	mFlashPos = rocket.pos;
	mFlashColor = color;
	mFlash = 1.0f;
}

void Fireworks::update(float dt, float spawnRate)
{
	if (spawnRate > 0.05f)
	{
		mNextLaunch -= dt * spawnRate;
		if (mNextLaunch <= 0.0f)
		{
			launch();
			if (rand() % 3 == 0)
				launch();	// de temps en temps deux d'un coup
			mNextLaunch = randf(0.8f, 2.0f);
		}
	}

	mFlash = glm::max(mFlash - dt * 2.5f, 0.0f);

	for (size_t i = 0; i < mParticles.size(); )
	{
		Particle& p = mParticles[i];
		p.life -= dt;

		if (p.rocket)
		{
			p.vel += GRAVITY * 0.5f * dt;
			p.pos += p.vel * dt;
			if (p.life <= 0.0f)
			{
				Particle r = p;
				mParticles[i] = mParticles.back();
				mParticles.pop_back();
				explode(r);
				continue;
			}
		}
		else
		{
			p.vel += GRAVITY * dt;
			p.vel *= 1.0f - 1.2f * dt;	// frottement de l'air
			p.pos += p.vel * dt;
		}

		if (p.life <= 0.0f)
		{
			mParticles[i] = mParticles.back();
			mParticles.pop_back();
			continue;
		}
		i++;
	}

	mData.clear();
	for (size_t i = 0; i < mParticles.size(); i++)
	{
		const Particle& p = mParticles[i];
		float k = p.life / p.maxLife;
		GpuPoint g;
		g.pos = p.pos;
		if (p.rocket)
			g.color = glm::vec4(p.color * 3.0f, 0.1f);
		else
		{
			// scintille un peu a la fin
			float sparkle = (k < 0.3f && rand() % 3 == 0) ? 0.2f : 1.0f;
			g.color = glm::vec4(p.color * (6.0f * k * k * sparkle), 0.15f);
		}
		mData.push_back(g);
	}

	if (!mData.empty())
	{
		glBindBuffer(GL_ARRAY_BUFFER, mVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, mData.size() * sizeof(GpuPoint), &mData[0]);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}

void Fireworks::draw(const glm::mat4& view, const glm::mat4& projection)
{
	if (mData.empty())
		return;

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDepthMask(GL_FALSE);

	mShader.use();
	mShader.setUniform("view", view);
	mShader.setUniform("projection", projection);

	glBindVertexArray(mVAO);
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)mData.size());
	glBindVertexArray(0);

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}
