//-----------------------------------------------------------------------------
// Basic Mesh class
//-----------------------------------------------------------------------------
#include "Mesh.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include "glm/gtc/constants.hpp"


//-----------------------------------------------------------------------------
// split
//
// Params:  s - string to split
//		    t - string to split (ie. delimiter)
//
//Result:  Splits string according to some substring and returns it as a vector.
//-----------------------------------------------------------------------------
std::vector<std::string> split(std::string s, std::string t)
{
	std::vector<std::string> res;
	while(1)
	{
		size_t pos = s.find(t);
		if(pos == std::string::npos)
		{
			res.push_back(s);
			break;
		}
		res.push_back(s.substr(0, pos));
		s = s.substr(pos+1, s.size() - pos - 1);
	}
	return res;
}

// les index OBJ commencent a 1, et peuvent etre negatifs (relatifs a la fin)
static int fixIndex(int idx, size_t count)
{
	if (idx < 0)
		return (int)count + idx;
	return idx - 1;
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
Mesh::Mesh()
	:mLoaded(false), mVBO(0), mVAO(0), mMin(0.0f), mMax(0.0f)
{
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
Mesh::~Mesh()
{
	if (mLoaded)
	{
		glDeleteVertexArrays(1, &mVAO);
		glDeleteBuffers(1, &mVBO);
	}
}

//-----------------------------------------------------------------------------
// Loads a Wavefront OBJ model
//
// Version un peu plus tolerante que celle du cours :
//  - faces a 4+ sommets (decoupees en triangles)
//  - formats v, v/vt, v//vn, v/vt/vn
//  - si le fichier n'a pas de normales on les calcule par face
//-----------------------------------------------------------------------------
bool Mesh::loadOBJ(const std::string& filename)
{
	std::vector<glm::vec3> tempVertices;
	std::vector<glm::vec2> tempUVs;
	std::vector<glm::vec3> tempNormals;

	if (filename.find(".obj") == std::string::npos)
		return false;

	std::ifstream fin(filename, std::ios::in);
	if (!fin)
	{
		std::cerr << "Cannot open " << filename << std::endl;
		return false;
	}

	std::cout << "Loading OBJ file " << filename << " ..." << std::endl;

	std::string lineBuffer;
	while (std::getline(fin, lineBuffer))
	{
		std::stringstream ss(lineBuffer);
		std::string cmd;
		ss >> cmd;

		if (cmd == "v")
		{
			glm::vec3 vertex(0.0f);
			ss >> vertex.x >> vertex.y >> vertex.z;
			tempVertices.push_back(vertex);
		}
		else if (cmd == "vt")
		{
			glm::vec2 uv(0.0f);
			ss >> uv.x >> uv.y;
			tempUVs.push_back(uv);
		}
		else if (cmd == "vn")
		{
			glm::vec3 normal(0.0f, 1.0f, 0.0f);
			ss >> normal.x >> normal.y >> normal.z;
			tempNormals.push_back(glm::normalize(normal));
		}
		else if (cmd == "f")
		{
			std::vector<Vertex> face;
			std::vector<bool> hasNormal;
			std::string faceData;

			while (ss >> faceData)
			{
				std::vector<std::string> data = split(faceData, "/");
				Vertex v;
				v.position = glm::vec3(0.0f);
				v.normal = glm::vec3(0.0f);
				v.texCoords = glm::vec2(0.0f);
				bool n = false;

				if (data.size() > 0 && !data[0].empty())
					v.position = tempVertices[fixIndex(atoi(data[0].c_str()), tempVertices.size())];

				if (data.size() > 1 && !data[1].empty() && !tempUVs.empty())
					v.texCoords = tempUVs[fixIndex(atoi(data[1].c_str()), tempUVs.size())];

				if (data.size() > 2 && !data[2].empty() && !tempNormals.empty())
				{
					v.normal = tempNormals[fixIndex(atoi(data[2].c_str()), tempNormals.size())];
					n = true;
				}

				face.push_back(v);
				hasNormal.push_back(n);
			}

			// triangle fan : (0,1,2) (0,2,3) ...
			for (size_t i = 1; i + 1 < face.size(); i++)
			{
				Vertex tri[3] = { face[0], face[i], face[i + 1] };
				bool ok = hasNormal[0] && hasNormal[i] && hasNormal[i + 1];
				if (!ok)
				{
					glm::vec3 fn = glm::cross(tri[1].position - tri[0].position, tri[2].position - tri[0].position);
					if (glm::length(fn) > 0.0f)
						fn = glm::normalize(fn);
					for (int k = 0; k < 3; k++)
						tri[k].normal = fn;
				}
				mVertices.push_back(tri[0]);
				mVertices.push_back(tri[1]);
				mVertices.push_back(tri[2]);
			}
		}
	}

	fin.close();

	if (mVertices.empty())
	{
		std::cerr << "No faces in " << filename << std::endl;
		return false;
	}

	computeBounds();
	initBuffers();

	return (mLoaded = true);
}

//-----------------------------------------------------------------------------
// Sphere UV (pour la boule de bowling)
//-----------------------------------------------------------------------------
void Mesh::createSphere(float radius, int rings, int sectors)
{
	const float PI = glm::pi<float>();
	std::vector<Vertex> grid;

	for (int r = 0; r <= rings; r++)
	{
		float phi = PI * (float)r / rings;
		for (int s = 0; s <= sectors; s++)
		{
			float theta = 2.0f * PI * (float)s / sectors;
			Vertex v;
			v.normal = glm::vec3(sinf(phi) * cosf(theta), cosf(phi), sinf(phi) * sinf(theta));
			v.position = v.normal * radius;
			v.texCoords = glm::vec2((float)s / sectors, 1.0f - (float)r / rings);
			grid.push_back(v);
		}
	}

	mVertices.clear();
	for (int r = 0; r < rings; r++)
	{
		for (int s = 0; s < sectors; s++)
		{
			int a = r * (sectors + 1) + s;
			int b = a + sectors + 1;

			mVertices.push_back(grid[a]);
			mVertices.push_back(grid[a + 1]);
			mVertices.push_back(grid[b]);

			mVertices.push_back(grid[b]);
			mVertices.push_back(grid[a + 1]);
			mVertices.push_back(grid[b + 1]);
		}
	}

	computeBounds();
	initBuffers();
	mLoaded = true;
}

//-----------------------------------------------------------------------------
// Cylindre et cone (manege, poteaux, toit). Base en y = 0.
//-----------------------------------------------------------------------------
static Vertex makeVertex(const glm::vec3& p, const glm::vec3& n, const glm::vec2& uv)
{
	Vertex v;
	v.position = p;
	v.normal = n;
	v.texCoords = uv;
	return v;
}

void Mesh::addDisc(float radius, float y, int segments, bool facingUp)
{
	const float PI = glm::pi<float>();
	glm::vec3 n(0.0f, facingUp ? 1.0f : -1.0f, 0.0f);

	for (int i = 0; i < segments; i++)
	{
		float a0 = 2.0f * PI * i / segments;
		float a1 = 2.0f * PI * (i + 1) / segments;
		glm::vec3 p0(cosf(a0) * radius, y, sinf(a0) * radius);
		glm::vec3 p1(cosf(a1) * radius, y, sinf(a1) * radius);

		// uv "a plat", comme une texture posee sur le dessus
		glm::vec2 uv0(cosf(a0) * 0.5f + 0.5f, sinf(a0) * 0.5f + 0.5f);
		glm::vec2 uv1(cosf(a1) * 0.5f + 0.5f, sinf(a1) * 0.5f + 0.5f);

		mVertices.push_back(makeVertex(glm::vec3(0.0f, y, 0.0f), n, glm::vec2(0.5f)));
		if (facingUp)
		{
			mVertices.push_back(makeVertex(p1, n, uv1));
			mVertices.push_back(makeVertex(p0, n, uv0));
		}
		else
		{
			mVertices.push_back(makeVertex(p0, n, uv0));
			mVertices.push_back(makeVertex(p1, n, uv1));
		}
	}
}

void Mesh::createCylinder(float radius, float height, int segments)
{
	const float PI = glm::pi<float>();
	mVertices.clear();

	for (int i = 0; i < segments; i++)
	{
		float u0 = (float)i / segments, u1 = (float)(i + 1) / segments;
		float a0 = 2.0f * PI * u0, a1 = 2.0f * PI * u1;
		glm::vec3 n0(cosf(a0), 0.0f, sinf(a0)), n1(cosf(a1), 0.0f, sinf(a1));
		glm::vec3 b0 = n0 * radius, b1 = n1 * radius;
		glm::vec3 t0 = b0 + glm::vec3(0, height, 0), t1 = b1 + glm::vec3(0, height, 0);

		mVertices.push_back(makeVertex(b0, n0, glm::vec2(u0, 0.0f)));
		mVertices.push_back(makeVertex(t0, n0, glm::vec2(u0, 1.0f)));
		mVertices.push_back(makeVertex(b1, n1, glm::vec2(u1, 0.0f)));

		mVertices.push_back(makeVertex(b1, n1, glm::vec2(u1, 0.0f)));
		mVertices.push_back(makeVertex(t0, n0, glm::vec2(u0, 1.0f)));
		mVertices.push_back(makeVertex(t1, n1, glm::vec2(u1, 1.0f)));
	}

	addDisc(radius, height, segments, true);
	addDisc(radius, 0.0f, segments, false);

	computeBounds();
	initBuffers();
	mLoaded = true;
}

void Mesh::createCone(float radius, float height, int segments)
{
	const float PI = glm::pi<float>();
	mVertices.clear();

	glm::vec3 top(0.0f, height, 0.0f);
	for (int i = 0; i < segments; i++)
	{
		float u0 = (float)i / segments, u1 = (float)(i + 1) / segments;
		float a0 = 2.0f * PI * u0, a1 = 2.0f * PI * u1;
		float am = (a0 + a1) * 0.5f;

		// normale inclinee suivant la pente du cone
		glm::vec3 n0 = glm::normalize(glm::vec3(cosf(a0) * height, radius, sinf(a0) * height));
		glm::vec3 n1 = glm::normalize(glm::vec3(cosf(a1) * height, radius, sinf(a1) * height));
		glm::vec3 nm = glm::normalize(glm::vec3(cosf(am) * height, radius, sinf(am) * height));

		mVertices.push_back(makeVertex(glm::vec3(cosf(a0), 0.0f, sinf(a0)) * radius, n0, glm::vec2(u0, 0.0f)));
		mVertices.push_back(makeVertex(top, nm, glm::vec2((u0 + u1) * 0.5f, 1.0f)));
		mVertices.push_back(makeVertex(glm::vec3(cosf(a1), 0.0f, sinf(a1)) * radius, n1, glm::vec2(u1, 0.0f)));
	}

	addDisc(radius, 0.0f, segments, false);

	computeBounds();
	initBuffers();
	mLoaded = true;
}

void Mesh::computeBounds()
{
	mMin = mMax = mVertices[0].position;
	for (size_t i = 1; i < mVertices.size(); i++)
	{
		mMin = glm::min(mMin, mVertices[i].position);
		mMax = glm::max(mMax, mVertices[i].position);
	}
}

//-----------------------------------------------------------------------------
// Create and initialize the vertex buffer and vertex array object
// Must have valid, non-empty std::vector of Vertex objects.
//-----------------------------------------------------------------------------
void Mesh::initBuffers()
{
	glGenVertexArrays(1, &mVAO);
	glGenBuffers(1, &mVBO);

	glBindVertexArray(mVAO);
	glBindBuffer(GL_ARRAY_BUFFER, mVBO);
	glBufferData(GL_ARRAY_BUFFER, mVertices.size() * sizeof(Vertex), &mVertices[0], GL_STATIC_DRAW);

	// Vertex Positions
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	// Normals attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);

	// Vertex Texture Coords
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(6 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);

	// unbind to make sure other code does not change it somewhere else
	glBindVertexArray(0);
}

//-----------------------------------------------------------------------------
// Render the mesh
//-----------------------------------------------------------------------------
void Mesh::draw()
{
	if (!mLoaded) return;

	glBindVertexArray(mVAO);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)mVertices.size());
	glBindVertexArray(0);
}
