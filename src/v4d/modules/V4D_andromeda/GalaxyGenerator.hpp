#pragma once

#include <v4d.h>
#include "GalacticPosition.hpp"

#pragma region Rendering options

constexpr double STAR_CHUNK_SIZE = 32;
constexpr double STAR_CHUNK_DENSITY_MULT = 0.2;
constexpr double STAR_MAX_VISIBLE_DISTANCE = 50; // light-years
constexpr int STAR_CHUNK_GEN_OFFSET = int(STAR_MAX_VISIBLE_DISTANCE / STAR_CHUNK_SIZE) + 1; // will try to generate chunks from -x to +x from current position in all axis

#pragma endregion

class GalaxyGenerator {
	
	#pragma region Noise Functions
	
		// static uint RandomInt(uint& seed) {
		// 	// LCG values from Numerical Recipes
		// 	return (seed = 1664525 * seed + 1013904223);
		// }
		
		// static float RandomFloat(uint& seed) {
		// 	// Float version using bitmask from Numerical Recipes
		// 	const uint one = 0x3f800000;
		// 	const uint msk = 0x007fffff;
		// 	return glm::uintBitsToFloat(one | (msk & (RandomInt(seed) >> 9))) - 1;
		// }
		
		// static glm::vec3 RandomInUnitSphere(uint& seed) {
		// 	for (;;) {
		// 		glm::vec3 p = 2.0f * glm::vec3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed)) - 1.0f;
		// 		if (dot(p, p) < 1) return p;
		// 	}
		// }
		
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
		inline static float UniformFloatFromStarPos(glm::vec3 p, float seed) {
			return noise2(glm::vec2(noise3(glm::vec3(p.x*0.4321f, p.y*0.964f, p.z*0.15623f)), seed*8.12533f));
		}
		
	#pragma endregion

public:

	#pragma region Galaxy generator tweeks
	
	static constexpr float spiralCloudsFactor = 1.0; // 0.0 to 2.0
	static constexpr float swirlTwist = 1.0; // -3.0 to +3.0 (typically between +- 0.2 and 1.5)
	static constexpr float swirlDetail = 1.0; // 0.0 to 1.0
	static constexpr float cloudsSize = 1.0; // 0.0 to 1.5
	static constexpr float cloudsFrequency = 1.0; // 0.0 to 2.0
	static constexpr float flatness = 1.0; // 0.0 to 2.0 (typically between 0.3 and 1.5 for a spiral, and 0.0 for elliptical)
	
	#pragma endregion
	
	#pragma region Seeds
	
	static constexpr float SEED_STARSYSTEM_PRESENCE = 0;
	static constexpr float SEED_STARSYSTEM_OFFSET_X = 1;
	static constexpr float SEED_STARSYSTEM_OFFSET_Y = 2;
	static constexpr float SEED_STARSYSTEM_OFFSET_Z = 3;
	static constexpr float SEED_STARSYSTEM_COLOR = 4;
	static constexpr float SEED_STARSYSTEM_BRIGHTNESS = 5;
	
	#pragma endregion

	float GetGalaxyDensity(const glm::vec3& pos) const { // expects a pos with values between -1.0 and +1.0
		using namespace glm;
		
		const float len = dot(pos, pos);
		if (len > 1.0) return 0.0;

		const float squish = flatness * 65.0f;
		const float lenSquished = length(pos*vec3(1.0, squish + 1.0, 1.0));
		const float radiusGradient = (1.0-clamp(len*5.0f + abs(pos.y)*squish, 0.0f, 1.0f))*1.002f;

		// Core
		const float core = pow(1.0 - lenSquished*1.84 + lenSquished, 50.0);

		// Spiral
		const float swirl = len * swirlTwist * 40.0;
		const float spiralNoise = FastSimplex(vec3(
			pos.x * cos(swirl) - pos.z * sin(swirl),
			pos.y,
			pos.z * cos(swirl) + pos.x * sin(swirl)
		) * cloudsFrequency * 4.0f) / 2.0 + 0.5;
		const float spirale = (pow(spiralNoise, (1.15-swirlDetail)*10.0) + cloudsSize - len - (abs(pos.y)*squish)) * radiusGradient;

		return core + pow(max(0.0f, radiusGradient), 50.0f) / 2.0 + pow(spirale, 4.) * spiralCloudsFactor / 8.0;
	}
	
	bool GetStarSystemPresence(const glm::ivec3& posInGalaxy) const {
		return UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_PRESENCE) < glm::clamp(GetGalaxyDensity(GalacticPosition::ToGalaxyDensityPos(posInGalaxy)), 0.0f, 1.0f) * STAR_CHUNK_DENSITY_MULT;
	}
	
	glm::vec3 GetStarSystemOffset(const glm::ivec3& posInGalaxy) const {
		return (glm::vec3(
			UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_OFFSET_X),
			UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_OFFSET_Y),
			UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_OFFSET_Z)
		) - 0.5f) * 0.9f;
	}

	glm::vec4 GetStarSystemColor(const glm::ivec3& posInGalaxy) const {
		float colorType = UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_COLOR);
		float brightness = UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_BRIGHTNESS);
		glm::vec3 color;
		if (colorType <= 0.4) {
			color = glm::vec3( 1.0 , 0.7 , 0.6 ); // 40% red
			brightness *= 0.3;
		} else if (colorType > 0.4 && colorType <= 0.7) {
			color = glm::vec3( 1.0 , 1.0 , 0.8 ); // 30% yellow
		} else if (colorType > 0.7 && colorType < 0.85) {
			color = glm::vec3( 0.8 , 0.8 , 1.0 ); // 15% blue
			brightness *= 3.0;
		} else {
			color = glm::vec3( 1.0 , 1.0 , 1.0 ); // 15% white
			brightness *= 2.0;
		}
		return {color, brightness};
	}

};
