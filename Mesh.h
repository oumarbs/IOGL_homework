//-----------------------------------------------------------------------------
// Basic Mesh class
//-----------------------------------------------------------------------------
#ifndef MESH_H
#define MESH_H

#include <vector>
#include <string>

#define GLEW_STATIC
#include "GL/glew.h"	// Important - this header must come before glfw3 header
#include "glm/glm.hpp"


struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoords;
};

class Mesh
{
public:

	 Mesh();
	~Mesh();

	bool loadOBJ(const std::string& filename);
	void createSphere(float radius, int rings, int sectors);
	void createCylinder(float radius, float height, int segments);
	void createCone(float radius, float height, int segments);
	void draw();

	// boite englobante, pratique pour poser les objets au sol
	glm::vec3 getMin() const { return mMin; }
	glm::vec3 getMax() const { return mMax; }

private:

	void initBuffers();
	void computeBounds();
	void addDisc(float radius, float y, int segments, bool facingUp);

	bool mLoaded;
	std::vector<Vertex> mVertices;
	GLuint mVBO, mVAO;
	glm::vec3 mMin, mMax;
};
#endif //MESH_H
