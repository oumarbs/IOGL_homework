//-----------------------------------------------------------------------------
// Trajet de camera pour la visite automatique.
// Les points sont relies par une spline de Catmull-Rom (boucle fermee).
//-----------------------------------------------------------------------------
#ifndef CAMERA_PATH_H
#define CAMERA_PATH_H

#include <vector>
#include "glm/glm.hpp"

class CameraPath
{
public:
	CameraPath(float secondsPerKey = 4.0f);

	void addKey(const glm::vec3& position, const glm::vec3& target);
	void evaluate(float t, glm::vec3& position, glm::vec3& target) const;

	float getDuration() const;

private:
	struct Key
	{
		glm::vec3 position;
		glm::vec3 target;
	};

	std::vector<Key> mKeys;
	float mSecondsPerKey;
};

#endif
