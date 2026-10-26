#include "CameraPath.h"
#include <cmath>

static glm::vec3 catmullRom(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, float t)
{
	float t2 = t * t;
	float t3 = t2 * t;
	return 0.5f * ((2.0f * p1) +
	               (-p0 + p2) * t +
	               (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
	               (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

CameraPath::CameraPath(float secondsPerKey)
	: mSecondsPerKey(secondsPerKey)
{}

void CameraPath::addKey(const glm::vec3& position, const glm::vec3& target)
{
	Key k;
	k.position = position;
	k.target = target;
	mKeys.push_back(k);
}

float CameraPath::getDuration() const
{
	return mKeys.size() * mSecondsPerKey;
}

void CameraPath::evaluate(float t, glm::vec3& position, glm::vec3& target) const
{
	int n = (int)mKeys.size();
	if (n == 0)
		return;
	if (n < 2)
	{
		position = mKeys[0].position;
		target = mKeys[0].target;
		return;
	}

	t = fmodf(t, getDuration());
	int i = (int)(t / mSecondsPerKey);
	float local = (t - i * mSecondsPerKey) / mSecondsPerKey;

	const Key& k0 = mKeys[(i - 1 + n) % n];
	const Key& k1 = mKeys[i % n];
	const Key& k2 = mKeys[(i + 1) % n];
	const Key& k3 = mKeys[(i + 2) % n];

	position = catmullRom(k0.position, k1.position, k2.position, k3.position, local);
	target = catmullRom(k0.target, k1.target, k2.target, k3.target, local);
}
