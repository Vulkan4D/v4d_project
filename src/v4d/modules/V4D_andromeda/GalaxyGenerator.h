#pragma once

#include <v4d.h>
#include <tgmath.h>
#include "GalacticPosition.hpp"

#pragma region Rendering options

constexpr double STAR_CHUNK_SIZE = 32;
constexpr double STAR_CHUNK_DENSITY_MULT = 0.2;
constexpr double STAR_MAX_VISIBLE_DISTANCE = 50; // light-years
constexpr int STAR_CHUNK_GEN_OFFSET = int(STAR_MAX_VISIBLE_DISTANCE / STAR_CHUNK_SIZE) + 1; // will try to generate chunks from -x to +x from current position in all axis

#pragma endregion

#pragma region Limits

constexpr double STARSYSTEM_RADIUS_MIN = 0.00001;
constexpr double STARSYSTEM_RADIUS_MAX = 0.1; // light-years
constexpr double STARSYSTEM_MASS_MIN_POW = 28;
constexpr double STARSYSTEM_MASS_MAX_POW = 32; // (0-4)*10^x
constexpr double STARSYSTEM_AGE_MIN = 0.001;
constexpr double STARSYSTEM_AGE_MAX = 14.0; // billions of years

#pragma endregion

class Celestial;
class StarSystem;

class GalaxyGenerator {
private:
	friend class StarSystem;
	friend class Celestial;
	// Cache
	static std::recursive_mutex cacheMutex;
	static std::unordered_map<uint64_t, std::shared_ptr<StarSystem>> starSystems;
	static std::unordered_map<uint64_t, std::shared_ptr<Celestial>> celestials;

public:

	#pragma region Galaxy generator tweeks
	
	static constexpr float spiralCloudsFactor = 1.0; // 0.0 to 2.0
	static constexpr float swirlTwist = 1.0; // -3.0 to +3.0 (typically between +- 0.2 and 1.5)
	static constexpr float swirlDetail = 1.0; // 0.0 to 1.0
	static constexpr float cloudsSize = 1.0; // 0.0 to 1.5
	static constexpr float cloudsFrequency = 1.0; // 0.0 to 2.0
	static constexpr float flatness = 1.0; // 0.0 to 2.0 (typically between 0.3 and 1.5 for a spiral, and 0.0 for elliptical)
	
	#pragma endregion
	
	static float GetGalaxyDensity(const glm::vec3& pos); // expects a pos with values between -1.0 and +1.0
	
	static bool GetStarSystemPresence(const glm::ivec3& posInGalaxy);
	
	static std::shared_ptr<StarSystem> GetStarSystem(const glm::ivec3& posInGalaxy);
	static std::shared_ptr<Celestial> GetCelestial(GalacticPosition galacticPosition);
	
	static void ClearCache();
	static void ClearStarSystemCache(const glm::ivec3& posInGalaxy);
	static void ClearCelestialCache(GalacticPosition posInGalaxy);
	
	// glm::vec4 GetStarSystemColor(const glm::ivec3& posInGalaxy) const {
	// 	float colorType = UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_COLOR);
	// 	float brightness = UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_BRIGHTNESS);
	// 	glm::vec3 color;
	// 	if (colorType <= 0.4) {
	// 		color = glm::vec3( 1.0 , 0.7 , 0.6 ); // 40% red
	// 		brightness *= 0.3;
	// 	} else if (colorType > 0.4 && colorType <= 0.7) {
	// 		color = glm::vec3( 1.0 , 1.0 , 0.8 ); // 30% yellow
	// 	} else if (colorType > 0.7 && colorType < 0.85) {
	// 		color = glm::vec3( 0.8 , 0.8 , 1.0 ); // 15% blue
	// 		brightness *= 3.0;
	// 	} else {
	// 		color = glm::vec3( 1.0 , 1.0 , 1.0 ); // 15% white
	// 		brightness *= 2.0;
	// 	}
	// 	return {color, brightness};
	// }
	
	static std::shared_ptr<Celestial> MakeCelestial(GalacticPosition galacticPosition, double mass, double parentMass, double parentRadius, double parentOrbitalPlaneTiltDegrees, double age, double forcedOrbitDistance, double maxOrbitRadius, uint seed);
	static std::shared_ptr<Celestial> MakeBinaryCenter(GalacticPosition galacticPosition, double mass, double parentMass, double parentRadius, double parentOrbitalPlaneTiltDegrees, double age, double forcedOrbitDistance, double maxOrbitRadius, uint seed);

};
