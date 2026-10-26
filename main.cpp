//-----------------------------------------------------------------------------
// IOGL - Homework
// Une petite fete foraine de nuit : manege avec des lapins, stand,
// guirlandes, bowling et feu d'artifice. Visite automatique de la
// camera (touche C pour passer en camera libre).
//-----------------------------------------------------------------------------
#include <iostream>
#include <sstream>
#include <string>
#define GLEW_STATIC
#include "GL/glew.h"	// Important - this header must come before glfw3 header
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "ShaderProgram.h"
#include "Texture2D.h"
#include "Camera.h"
#include "Mesh.h"
#include "Scene.h"
#include "CameraPath.h"
#include "Effects.h"


// Global Variables
const char* APP_TITLE = "IOGL Homework - Fete foraine";
int gWindowWidth = 1280;
int gWindowHeight = 720;
GLFWwindow* gWindow = NULL;

bool gWireframe = false;
bool gFlashlightOn = false;
bool gFreeCamera = false;
bool gBloom = true;
bool gFog = true;
bool gShadows = true;

Scene* gScene = NULL;
PostFX* gPost = NULL;

FPSCamera fpsCamera(glm::vec3(0.0f, 3.0f, 14.0f));
CameraPath gTour(4.5f);
float gTourTime = 0.0f;
glm::vec3 gTourPos, gTourTarget;

const double ZOOM_SENSITIVITY = -3.0;
const float MOVE_SPEED = 5.0; // units per second
const float MOUSE_SENSITIVITY = 0.1f;


// Function prototypes
void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mode);
void glfw_onFramebufferSize(GLFWwindow* window, int width, int height);
void glfw_onMouseScroll(GLFWwindow* window, double deltaX, double deltaY);
void update(double elapsedTime);
void showFPS(GLFWwindow* window);
bool initOpenGL();
void setupTour();
void printHelp();

//-----------------------------------------------------------------------------
// Main Application Entry Point
//-----------------------------------------------------------------------------
int main()
{
	if (!initOpenGL())
	{
		std::cerr << "GLFW initialization failed" << std::endl;
		return -1;
	}

	printHelp();

	// bloc a part : tout ce qui contient des objets OpenGL doit etre
	// detruit avant glfwTerminate()
	{
		ShaderProgram lightingShader, depthShader, bulbShader;
		lightingShader.loadShaders("shaders/scene.vert", "shaders/scene.frag");
		depthShader.loadShaders("shaders/shadow_depth.vert", "shaders/shadow_depth.frag");
		bulbShader.loadShaders("shaders/emissive.vert", "shaders/emissive.frag");

		Scene scene;
		if (!scene.load())
			std::cerr << "Attention : certains modeles/textures n'ont pas ete charges" << std::endl;
		gScene = &scene;

		int fbw, fbh;
		glfwGetFramebufferSize(gWindow, &fbw, &fbh);

		ShadowMap shadowMap;
		shadowMap.init(2048);

		PostFX post;
		post.init(fbw, fbh);
		gPost = &post;

		Sky sky;
		sky.init();

		Fireworks fireworks;
		fireworks.init();

		setupTour();
		gTour.evaluate(0.0f, gTourPos, gTourTarget);
		fpsCamera.setPosition(gTourPos);
		fpsCamera.lookAt(gTourTarget);

		float fade = 0.0f;
		float bars = 0.0f;
		double lastTime = glfwGetTime();

		// Rendering loop
		while (!glfwWindowShouldClose(gWindow))
		{
			showFPS(gWindow);

			double currentTime = glfwGetTime();
			double deltaTime = currentTime - lastTime;
			lastTime = currentTime;

			// evite un gros saut si la fenetre a ete bloquee (deplacement...)
			if (deltaTime > 0.1)
				deltaTime = 0.1;

			glfwPollEvents();
			update(deltaTime);

			// le feu d'artifice seulement la nuit
			fireworks.update((float)deltaTime, 1.0f - scene.getEnv().day);
			scene.setFireworkFlash(fireworks.getFlashPos(), fireworks.getFlashColor());
			scene.update(deltaTime);

			const Environment& env = scene.getEnv();
			float t = scene.getTime();

			// fondu au debut + bandes noires pendant la visite
			fade = glm::min(fade + (float)deltaTime * 0.5f, 1.0f);
			float barsTarget = gFreeCamera ? 0.0f : 0.08f;
			bars += (barsTarget - bars) * glm::min((float)deltaTime * 3.0f, 1.0f);

			glfwGetFramebufferSize(gWindow, &fbw, &fbh);
			if (fbw == 0 || fbh == 0)
				continue;	// fenetre reduite

			// Camera
			glm::vec3 viewPos, look;
			glm::mat4 view;
			if (gFreeCamera)
			{
				view = fpsCamera.getViewMatrix();
				viewPos = fpsCamera.getPosition();
				look = fpsCamera.getLook();
			}
			else
			{
				view = glm::lookAt(gTourPos, gTourTarget, glm::vec3(0.0f, 1.0f, 0.0f));
				viewPos = gTourPos;
				look = glm::normalize(gTourTarget - gTourPos);
			}

			glm::mat4 projection = glm::perspective(glm::radians(fpsCamera.getFOV()), (float)fbw / (float)fbh, 0.1f, 200.0f);
			glm::mat4 lightSpace = scene.getLightSpaceMatrix();

			// 1) shadow map vue depuis le soleil / la lune
			shadowMap.begin();
			depthShader.use();
			depthShader.setUniform("lightSpace", lightSpace);
			scene.draw(depthShader, true);
			shadowMap.end(fbw, fbh);

			// 2) scene dans le FBO HDR
			post.beginScene();
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// moins de brouillard en journee
			float fogDensity = gFog ? glm::mix(0.035f, 0.014f, env.day) : 0.0f;

			if (gWireframe)
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			lightingShader.use();
			lightingShader.setUniform("view", view);
			lightingShader.setUniform("projection", projection);
			lightingShader.setUniform("lightSpace", lightSpace);
			lightingShader.setUniform("viewPos", viewPos);
			lightingShader.setUniform("fogColor", env.horizon);
			lightingShader.setUniform("fogDensity", fogDensity);
			lightingShader.setUniform("shadowsOn", gShadows ? 1 : 0);
			lightingShader.setUniformSampler("shadowMap", 2);
			shadowMap.bindTexture(2);
			scene.setLights(lightingShader, viewPos, look, gFlashlightOn);
			scene.draw(lightingShader, false);

			bulbShader.use();
			bulbShader.setUniform("view", view);
			bulbShader.setUniform("projection", projection);
			bulbShader.setUniform("viewPos", viewPos);
			bulbShader.setUniform("fogColor", env.horizon);
			bulbShader.setUniform("fogDensity", fogDensity);
			scene.drawBulbs(bulbShader);

			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			sky.draw(view, projection, env.zenith, env.horizon, env.sunDir, env.moonDir, 1.0f - env.day, t);

			fireworks.draw(view, projection);

			// 3) bloom + tone mapping vers l'ecran
			float exposure = glm::mix(1.6f, 1.0f, env.day);
			post.render(gBloom, exposure, fade, bars);

			glfwSwapBuffers(gWindow);
		}

		gScene = NULL;
		gPost = NULL;
	}

	glfwTerminate();

	return 0;
}

//-----------------------------------------------------------------------------
// Points de passage de la visite (position camera, point regarde)
//-----------------------------------------------------------------------------
void setupTour()
{
	gTour.addKey(glm::vec3(  0.0f, 7.0f,  24.0f), glm::vec3( 0.0f, 2.0f,   0.0f));	// vue d'ensemble
	gTour.addKey(glm::vec3(-10.0f, 4.0f,  13.0f), glm::vec3(-7.0f, 1.2f,   0.0f));
	gTour.addKey(glm::vec3( -4.5f, 2.0f,   4.5f), glm::vec3(-8.5f, 1.4f,  -0.5f));	// le stand
	gTour.addKey(glm::vec3( -6.5f, 2.0f,   6.5f), glm::vec3( 0.0f, 2.0f,   0.0f));
	gTour.addKey(glm::vec3( -2.5f, 1.4f,   5.5f), glm::vec3( 0.0f, 1.4f,   0.0f));	// lapins sur le manege
	gTour.addKey(glm::vec3(  2.5f, 1.0f,   5.8f), glm::vec3( 0.0f, 3.6f,   0.0f));	// contre-plongee sur le toit
	gTour.addKey(glm::vec3(  5.0f, 3.0f,   7.5f), glm::vec3( 7.0f, 0.5f,  -3.0f));
	gTour.addKey(glm::vec3(  7.6f, 1.3f,   8.5f), glm::vec3( 7.0f, 0.5f,  -3.0f));	// piste de bowling
	gTour.addKey(glm::vec3(  5.0f, 2.2f,   1.5f), glm::vec3( 7.0f, 0.3f,  -4.0f));	// quilles de pres
	gTour.addKey(glm::vec3( 13.0f, 3.0f,  -1.0f), glm::vec3( 6.0f, 0.8f,  -4.0f));
	gTour.addKey(glm::vec3(  9.0f, 1.6f,   7.0f), glm::vec3(-2.0f, 9.0f, -28.0f));	// le feu d'artifice derriere le manege
	gTour.addKey(glm::vec3(  6.0f, 6.0f,  20.0f), glm::vec3( 0.0f, 6.0f, -10.0f));
}

void printHelp()
{
	std::cout << "\n"
	          << "  C        visite auto / camera libre\n"
	          << "  WASD     deplacement (camera libre), Z/X monter/descendre\n"
	          << "  Souris   regarder, molette = zoom\n"
	          << "  F        lampe torche\n"
	          << "  N        jour / nuit       T  cycle jour-nuit auto\n"
	          << "  B        bloom             G  brouillard\n"
	          << "  O        ombres            P  pause animations\n"
	          << "  F1       fil de fer        Echap  quitter\n"
	          << std::endl;
}

//-----------------------------------------------------------------------------
// Initialize GLFW and OpenGL
//-----------------------------------------------------------------------------
bool initOpenGL()
{
	if (!glfwInit())
	{
		std::cerr << "GLFW initialization failed" << std::endl;
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	gWindow = glfwCreateWindow(gWindowWidth, gWindowHeight, APP_TITLE, NULL, NULL);
	if (gWindow == NULL)
	{
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return false;
	}

	glfwMakeContextCurrent(gWindow);
	glfwSwapInterval(1);

	// Initialize GLEW
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		std::cerr << "Failed to initialize GLEW" << std::endl;
		return false;
	}

	std::cout << "OpenGL " << glGetString(GL_VERSION) << " - " << glGetString(GL_RENDERER) << std::endl;

	glfwSetKeyCallback(gWindow, glfw_onKey);
	glfwSetFramebufferSizeCallback(gWindow, glfw_onFramebufferSize);
	glfwSetScrollCallback(gWindow, glfw_onMouseScroll);

	int w, h;
	glfwGetFramebufferSize(gWindow, &w, &h); // For retina display
	glViewport(0, 0, w, h);

	glEnable(GL_DEPTH_TEST);

	return true;
}

//-----------------------------------------------------------------------------
// Is called whenever a key is pressed/released via GLFW
//-----------------------------------------------------------------------------
void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	if (action != GLFW_PRESS)
		return;

	switch (key)
	{
	case GLFW_KEY_ESCAPE:
		glfwSetWindowShouldClose(window, GL_TRUE);
		break;

	case GLFW_KEY_F1:
		gWireframe = !gWireframe;
		break;

	case GLFW_KEY_F:
		gFlashlightOn = !gFlashlightOn;
		break;

	case GLFW_KEY_C:
		gFreeCamera = !gFreeCamera;
		if (gFreeCamera)
		{
			// on repart de l'endroit ou etait la visite
			fpsCamera.setPosition(gTourPos);
			fpsCamera.lookAt(gTourTarget);
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			glfwSetCursorPos(window, gWindowWidth / 2.0, gWindowHeight / 2.0);
		}
		else
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		break;

	case GLFW_KEY_B: gBloom = !gBloom; break;
	case GLFW_KEY_G: gFog = !gFog; break;
	case GLFW_KEY_O: gShadows = !gShadows; break;

	case GLFW_KEY_N: if (gScene) gScene->toggleDayNight(); break;
	case GLFW_KEY_T: if (gScene) gScene->toggleDayCycle(); break;
	case GLFW_KEY_P: if (gScene) gScene->togglePause(); break;
	}
}

//-----------------------------------------------------------------------------
// Is called when the window is resized
//-----------------------------------------------------------------------------
void glfw_onFramebufferSize(GLFWwindow* window, int width, int height)
{
	glfwGetWindowSize(window, &gWindowWidth, &gWindowHeight);
	glViewport(0, 0, width, height);

	if (gPost)
		gPost->resize(width, height);
}

//-----------------------------------------------------------------------------
// Called by GLFW when the mouse wheel is rotated
//-----------------------------------------------------------------------------
void glfw_onMouseScroll(GLFWwindow* window, double deltaX, double deltaY)
{
	double fov = fpsCamera.getFOV() + deltaY * ZOOM_SENSITIVITY;

	fov = glm::clamp(fov, 1.0, 120.0);

	fpsCamera.setFOV((float)fov);
}

//-----------------------------------------------------------------------------
// Update stuff every frame
//-----------------------------------------------------------------------------
void update(double elapsedTime)
{
	if (!gFreeCamera)
	{
		gTourTime += (float)elapsedTime;
		gTour.evaluate(gTourTime, gTourPos, gTourTarget);
		return;
	}

	// Camera orientation
	double mouseX, mouseY;
	glfwGetCursorPos(gWindow, &mouseX, &mouseY);

	fpsCamera.rotate((float)(gWindowWidth / 2.0 - mouseX) * MOUSE_SENSITIVITY, (float)(gWindowHeight / 2.0 - mouseY) * MOUSE_SENSITIVITY);

	glfwSetCursorPos(gWindow, gWindowWidth / 2.0, gWindowHeight / 2.0);

	float speed = MOVE_SPEED;
	if (glfwGetKey(gWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
		speed *= 2.5f;

	// Forward/backward
	if (glfwGetKey(gWindow, GLFW_KEY_W) == GLFW_PRESS)
		fpsCamera.move(speed * (float)elapsedTime * fpsCamera.getLook());
	else if (glfwGetKey(gWindow, GLFW_KEY_S) == GLFW_PRESS)
		fpsCamera.move(speed * (float)elapsedTime * -fpsCamera.getLook());

	// Strafe left/right
	if (glfwGetKey(gWindow, GLFW_KEY_A) == GLFW_PRESS)
		fpsCamera.move(speed * (float)elapsedTime * -fpsCamera.getRight());
	else if (glfwGetKey(gWindow, GLFW_KEY_D) == GLFW_PRESS)
		fpsCamera.move(speed * (float)elapsedTime * fpsCamera.getRight());

	// Up/down
	if (glfwGetKey(gWindow, GLFW_KEY_Z) == GLFW_PRESS)
		fpsCamera.move(speed * (float)elapsedTime * glm::vec3(0.0f, 1.0f, 0.0f));
	else if (glfwGetKey(gWindow, GLFW_KEY_X) == GLFW_PRESS)
		fpsCamera.move(speed * (float)elapsedTime * -glm::vec3(0.0f, 1.0f, 0.0f));
}

//-----------------------------------------------------------------------------
// Code computes the average frames per second, and also the average time it takes
// to render one frame.  These stats are appended to the window caption bar.
//-----------------------------------------------------------------------------
void showFPS(GLFWwindow* window)
{
	static double previousSeconds = 0.0;
	static int frameCount = 0;
	double elapsedSeconds;
	double currentSeconds = glfwGetTime();

	elapsedSeconds = currentSeconds - previousSeconds;

	if (elapsedSeconds > 0.25)
	{
		previousSeconds = currentSeconds;
		double fps = (double)frameCount / elapsedSeconds;
		double msPerFrame = 1000.0 / fps;

		std::ostringstream outs;
		outs.precision(3);
		outs << std::fixed
			<< APP_TITLE << "    "
			<< (gFreeCamera ? "[camera libre]" : "[visite]") << "    "
			<< "FPS: " << fps << "    "
			<< "Frame Time: " << msPerFrame << " (ms)";
		glfwSetWindowTitle(window, outs.str().c_str());

		frameCount = 0;
	}

	frameCount++;
}
