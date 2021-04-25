#pragma once

#include <v4d.h>

inline static uint RandomInt(uint& seed) {
	// LCG values from Numerical Recipes
	return (seed = 1664525 * seed + 1013904223);
}

inline static uint RandomInt(uint& seed, int min, int max) {
	return (RandomInt(seed) % (max - min)) + min;
}

inline static float RandomFloat(uint& seed) {
	// Float version using bitmask from Numerical Recipes
	const uint one = 0x3f800000;
	const uint msk = 0x007fffff;
	return glm::uintBitsToFloat(one | (msk & (RandomInt(seed) >> 9))) - 1;
}

inline static glm::vec2 RandomInUnitCircle(uint& seed) {
	for (;;) {
		glm::vec2 p = 2.0f * glm::vec2(RandomFloat(seed), RandomFloat(seed)) - 1.0f;
		if (dot(p, p) < 1) return p;
	}
}

inline static glm::vec3 RandomInUnitSphere(uint& seed) {
	for (;;) {
		glm::vec3 p = 2.0f * glm::vec3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed)) - 1.0f;
		if (dot(p, p) < 1) return p;
	}
}

inline static glm::vec2 RandomInUnitSquare(uint& seed) {
	return glm::vec2(RandomFloat(seed), RandomFloat(seed)) * 2.0f - 1.0f;
}

inline static glm::vec3 RandomInUnitCube(uint& seed) {
	return glm::vec3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed)) * 2.0f - 1.0f;
}

template<int N = 3, typename FLOAT = float>
inline static glm::vec<N, FLOAT> RandomInTriangle(uint& seed, glm::vec<N, FLOAT> vertex1, glm::vec<N, FLOAT> vertex2) { // vertex0 is assumed at origin
	FLOAT u1 = RandomFloat(seed);
	FLOAT u2 = RandomFloat(seed);
	if (u1+u2 > 1) {
		u1 = 1.0f - u1;
		u2 = 1.0f - u2;
	}
	return vertex1*u1 + vertex2*u2;
}
