#include "GalaxyGenerator.h"
#include "noise_functions.hpp"
#include "seeds.hh"

#include "celestials/CelestialType.hh"
#include "Celestial.h"
#include "StarSystem.h"

#include "celestials/Star.h"
#include "celestials/Planet.h"
#include "celestials/Asteroid.h"
#include "celestials/BlackHole.h"
#include "celestials/GasGiant.h"

std::recursive_mutex GalaxyGenerator::cacheMutex {};
std::unordered_map<uint64_t, std::shared_ptr<StarSystem>> GalaxyGenerator::starSystems {};
std::unordered_map<uint64_t, std::shared_ptr<Celestial>> GalaxyGenerator::celestials {};


float GalaxyGenerator::GetGalaxyDensity(const glm::vec3& pos) { // expects a pos with values between -1.0 and +1.0
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

bool GalaxyGenerator::GetStarSystemPresence(const glm::ivec3& posInGalaxy) {
	return UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_PRESENCE) < glm::clamp(GetGalaxyDensity(GalacticPosition::ToGalaxyDensityPos(posInGalaxy)), 0.0f, 1.0f) * STAR_CHUNK_DENSITY_MULT;
}

std::shared_ptr<StarSystem> GalaxyGenerator::GetStarSystem(const glm::ivec3& posInGalaxy) {
	if (posInGalaxy.x < GalacticPosition::X_BOTTOM_VALUE
		|| posInGalaxy.y < GalacticPosition::Y_BOTTOM_VALUE
		|| posInGalaxy.z < GalacticPosition::Z_BOTTOM_VALUE
		|| posInGalaxy.x > GalacticPosition::X_TOP_VALUE
		|| posInGalaxy.y > GalacticPosition::Y_TOP_VALUE
		|| posInGalaxy.z > GalacticPosition::Z_TOP_VALUE
		) {
		return nullptr;
	}
	GalacticPosition galacticPosition(posInGalaxy);
	std::lock_guard lock(cacheMutex);
	try {
		return starSystems.at(galacticPosition.posInGalaxy);
	} catch (std::out_of_range) {
		if (GetStarSystemPresence(posInGalaxy)) {
			starSystems[galacticPosition.posInGalaxy] = std::make_shared<StarSystem>(galacticPosition);
		}
	}
	return starSystems[galacticPosition.posInGalaxy];
}

void GalaxyGenerator::ClearCache() {
	std::lock_guard lock(cacheMutex);
	celestials.clear();
	starSystems.clear();
}

void GalaxyGenerator::ClearStarSystemCache(const glm::ivec3& posInGalaxy) {
	std::lock_guard lock(cacheMutex);
	if (starSystems.count(GalacticPosition(posInGalaxy).posInGalaxy)) {
		starSystems.erase(GalacticPosition(posInGalaxy).posInGalaxy);
	}
}

void GalaxyGenerator::ClearCelestialCache(GalacticPosition posInGalaxy) {
	std::lock_guard lock(cacheMutex);
	if (celestials.count(posInGalaxy.rawValue)) {
		celestials.erase(posInGalaxy.rawValue);
	}
}


std::shared_ptr<Celestial> GalaxyGenerator::MakeCelestial(GalacticPosition galacticPosition, double age, double mass, double parentMass, double parentRadius, double parentOrbitalPlaneTiltDegrees, double forcedOrbitDistance, double maxOrbitRadius, uint seed, uint parentSeed, uint32_t flags) {
	std::lock_guard lock(cacheMutex);
	try {
		return celestials.at(galacticPosition.rawValue);
	} catch(std::out_of_range) {
		if (mass < 1E21) {
			return celestials[galacticPosition.rawValue] = std::make_shared<Asteroid>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed, parentSeed, flags);
		}
		if (mass < 1E26) {
			return celestials[galacticPosition.rawValue] = std::make_shared<Planet>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed, parentSeed, flags);
		}
		if (mass < 1E28) {
			return celestials[galacticPosition.rawValue] = std::make_shared<GasGiant>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed, parentSeed, flags);
		}
		if (mass < 1E29) {
			return celestials[galacticPosition.rawValue] = std::make_shared<BrownDwarf>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed, parentSeed, flags);
		}
		if (mass < 1E32) {
			if (RandomFloat(seed) < 0.002) return celestials[galacticPosition.rawValue] = std::make_shared<BlackHole>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed, parentSeed, flags);
			return celestials[galacticPosition.rawValue] = std::make_shared<Star>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed, parentSeed, flags);
		}
		if (mass < 1E35) {
			if (RandomFloat(seed) < 0.0001) return celestials[galacticPosition.rawValue] = std::make_shared<BlackHole>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed, parentSeed, flags);
			return celestials[galacticPosition.rawValue] = std::make_shared<HyperGiant>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed, parentSeed, flags);
		}
		return celestials[galacticPosition.rawValue] = std::make_shared<SuperMassiveBlackHole>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed, parentSeed, flags);
	}
}

std::shared_ptr<Celestial> GalaxyGenerator::MakeBinaryCenter(GalacticPosition galacticPosition, double age, double mass, double parentMass, double parentRadius, double parentOrbitalPlaneTiltDegrees, double forcedOrbitDistance, double maxOrbitRadius, uint seed, uint parentSeed, uint32_t flags) {
	std::lock_guard lock(cacheMutex);
	return celestials[galacticPosition.rawValue] = std::make_shared<BinaryCenter>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed, parentSeed, flags);
}

std::shared_ptr<Celestial> GalaxyGenerator::GetCelestial(GalacticPosition galacticPosition) {
	std::lock_guard lock(cacheMutex);
	if (galacticPosition.IsCelestial()) {
		try {
			return celestials.at(galacticPosition.rawValue);
		} catch (std::out_of_range) {
			auto starSystem = GetStarSystem(glm::ivec3(galacticPosition.posInGalaxy_x, galacticPosition.posInGalaxy_y, galacticPosition.posInGalaxy_z));
			if (starSystem) {
				auto level1 = starSystem->GetChild(galacticPosition.level1);
				if (level1) {
					if (galacticPosition.level2) {
						auto level2 = level1->GetChild(galacticPosition.level2);
						if (level2) {
							if (galacticPosition.level3) {
								return level2->GetChild(galacticPosition.level3);
							}
							return level2;
						}
						return nullptr;
					}
					return level1;
				}
			}
		}
	}
	return nullptr;
}
