#pragma once

#include <v4d.h>

static constexpr double G = 6.67408E-11;
static constexpr double PI = 3.14159265359;
static constexpr double TWOPI = 2.0 * PI;

static uint RandomInt(uint& seed) {
	// LCG values from Numerical Recipes
	return (seed = 1664525 * seed + 1013904223);
}

static float RandomFloat(uint& seed) {
	// Float version using bitmask from Numerical Recipes
	const uint one = 0x3f800000;
	const uint msk = 0x007fffff;
	return glm::uintBitsToFloat(one | (msk & (RandomInt(seed) >> 9))) - 1;
}

static glm::vec3 RandomInUnitSphere(uint& seed) {
	for (;;) {
		glm::vec3 p = 2.0f * glm::vec3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed)) - 1.0f;
		if (dot(p, p) < 1) return p;
	}
}

static glm::vec3 _random3(const glm::vec3& pos) {
	using namespace glm;
	float j = 4096.0*sin(dot(pos,vec3(17.0, 59.4, 15.0)));
	vec3 r;
	r.z = fract(512.0*j);
	j *= .125;
	r.x = fract(512.0*j);
	j *= .125;
	r.y = fract(512.0*j);
	return r-0.5f;
}

static float FastSimplex(const glm::vec3& pos) {
	using namespace glm;
	
	const float F3 = 0.3333333;
	const float G3 = 0.1666667;

	const vec3 s = floor(pos + dot(pos, vec3(F3)));
	const vec3 x = pos - s + dot(s, vec3(G3));

	const vec3 e = step(vec3(0.0), x - vec3(x.y, x.z, x.x));
	const vec3 i1 = e * (1.0f - vec3(e.z, e.x, e.y));
	const vec3 i2 = 1.0f - vec3(e.z, e.x, e.y) * (1.0f - e);

	const vec3 x1 = x - i1 + G3;
	const vec3 x2 = x - i2 + 2.0f * G3;
	const vec3 x3 = x - 1.0f + 3.0f * G3;

	vec4 w(
		dot(x, x),
		dot(x1, x1),
		dot(x2, x2),
		dot(x3, x3)
	);
	vec4 d(
		dot(_random3(s), x),
		dot(_random3(s + i1), x1),
		dot(_random3(s + i2), x2),
		dot(_random3(s + 1.0f), x3)
	);
	w = max(0.6f - w, 0.0f);
	w *= w;
	w *= w;
	d *= w;

	return (dot(d, vec4(52.0)));
}

inline static float noise2(glm::vec2 p) {
	return glm::fract(glm::sin(glm::dot(p, glm::vec2(12.9898, 4.1414))) * 43758.5453);
}
inline static float noise3(glm::vec3 p) {
	return glm::fract(glm::sin(glm::dot(p, glm::vec3(13.657,9.558,11.606))) * 24097.524);
}

inline static float UniformFloatFromStarPos(glm::ivec3 p, float seed) { // returns a uniformly-distributed float between 0.0 and 1.0
	return noise2(glm::vec2(noise3(glm::vec3(p.x*0.4321f, p.y*0.964f, p.z*0.15623f)), seed*8.12533f));
}

inline static double CenterDistributedCurve(double rand01) { // given a uniformly-distributed float between 0.0 and 1.0, it returns a new float that is distributed towards the center. May be called recursively for steeper central distribution.
	return glm::pow((rand01 * 2.0 - 1.0) * 0.7937005259840996, 3.0) + 0.5;
}

inline static double LogarithmicMix(double min, double max, double x) {
	double logMin = glm::log2(min);
	double logMax = glm::log2(max);
	double e = glm::mix(logMin, logMax, x);
	return glm::pow(2.0, e);
}
