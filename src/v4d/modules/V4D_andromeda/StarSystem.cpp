#include "StarSystem.h"
#include "noise_functions.hpp"
#include "seeds.hh"
#include "GalaxyGenerator.h"

double StarSystem::GetRadiusFactor() const {
	if (!_radiusFactor.has_value()) {
		_radiusFactor = CenterDistributedCurve(CenterDistributedCurve(UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_RADIUS)));
	}
	return _radiusFactor.value();
}
double StarSystem::GetRadius() const {
	if (!_radius.has_value()) {
		_radius = LY2M(LogarithmicMix(STARSYSTEM_RADIUS_MIN, STARSYSTEM_RADIUS_MAX, GetRadiusFactor()));
	}
	return _radius.value();
}
double StarSystem::GetMass() const {
	if (!_mass.has_value()) {
		_mass = UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_MASS) * 4.0 * glm::pow(10.0, glm::mix(STARSYSTEM_MASS_MIN_POW, STARSYSTEM_MASS_MAX_POW, GetRadiusFactor()));
	}
	return _mass.value();
}
glm::dvec3 StarSystem::GetOffsetLY() const {
	if (!_offsetLY.has_value()) {
		_offsetLY = (glm::dvec3(
			glm::sqrt(UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_OFFSET_X)),
			glm::sqrt(UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_OFFSET_Y)),
			glm::sqrt(UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_OFFSET_Z))
		) - 0.5) * (1.0 - M2LY(GetRadius())*2.0);
	}
	return _offsetLY.value();
}
double StarSystem::GetAge() const {
	if (!_age.has_value()) {
		_age = glm::mix(STARSYSTEM_AGE_MIN, STARSYSTEM_AGE_MAX, UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_AGE) * (1.0 - GetRadiusFactor()));
	}
	return _age.value();
}
double StarSystem::GetOrbitalPlaneTiltDegrees() const {
	if (!_orbitalPlaneTiltDegrees.has_value()) {
		_orbitalPlaneTiltDegrees = UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_ORBITAL_PLANE_TILT) * 360.0;
	}
	return _orbitalPlaneTiltDegrees.value();
}
int StarSystem::GetNbPlanetaryOrbits() const {
	if (!_nbPlanetaryOrbits.has_value()) {
		_nbPlanetaryOrbits = glm::clamp(glm::round(glm::pow(UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_NB_ORBITS), 2.0) * 3.0), 1.0, 3.0);
	}
	return _nbPlanetaryOrbits.value();
}
int StarSystem::GetNbCentralBodies() const {
	if (!_nbCentralBodies.has_value()) {
		switch (GetNbPlanetaryOrbits()) {
			case 1:
				_nbCentralBodies = 1;
				break;
			case 2:
			case 3:
				_nbCentralBodies = 2;
				break;
		}
	}
	return _nbCentralBodies.value();
}
uint StarSystem::GetPlanetarySeed() const {
	if (!_planetarySeed.has_value()) {
		_planetarySeed = (int)glm::round(double(UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_PLANETARY_SEED)) * SEED_STARSYSTEM_PLANETARY_SEED_2);
	}
	return _planetarySeed.value();
}
const StarSystem::CentralCelestialBodyList& StarSystem::GetCentralCelestialBodies() const {
	if (!_centralCelestialBodies.has_value()) {
		std::lock_guard lock(GalaxyGenerator::cacheMutex);
		std::array<std::shared_ptr<Celestial>, 3> centralCelestialBodies {nullptr, nullptr, nullptr};
		
		GalacticPosition childPosInGalaxy = posInGalaxy;
		uint parentSeed = GetPlanetarySeed();
		uint seed = parentSeed + SEED_STARSYSTEM_CENTRAL_BODIES;
		double age = GetAge();
		double parentMass = GetMass();
		double mass = parentMass * glm::mix(0.95, 0.999, RandomFloat(seed));
		double parentOrbitalPlaneTiltDegrees = GetOrbitalPlaneTiltDegrees();
		double maxChildOrbit = GetRadius();
		
		int n = GetNbCentralBodies();
		int orbits = GetNbPlanetaryOrbits();
		if (n == 1) {
			// Single star
			childPosInGalaxy.level1 = 1;
			centralCelestialBodies[1] = GalaxyGenerator::MakeCelestial(childPosInGalaxy, age, mass, /*parentMass*/0, /*parentRadius*/0, parentOrbitalPlaneTiltDegrees, /*forcedOrbitDistance*/0, maxChildOrbit, RandomInt(seed), parentSeed, CELESTIAL_FLAG_IS_CENTER_STAR);
		} else if (n == 2) {
			// Binary star
			double massDiff = glm::pow(RandomFloat(seed), 2.0) * 0.17;
			childPosInGalaxy.level1 = 1;
			centralCelestialBodies[1] = GalaxyGenerator::MakeCelestial(childPosInGalaxy, age, mass * (0.5-massDiff), /*parentMass*/mass, /*parentRadius*/0, parentOrbitalPlaneTiltDegrees, /*forcedOrbitDistance*/0, maxChildOrbit, RandomInt(seed), parentSeed, CELESTIAL_FLAG_IS_BINARY_1);
			childPosInGalaxy.level1 = 2;
			centralCelestialBodies[2] = GalaxyGenerator::MakeCelestial(childPosInGalaxy, age, mass * (0.5+massDiff), /*parentMass*/mass, /*parentRadius*/0, parentOrbitalPlaneTiltDegrees, /*forcedOrbitDistance*/centralCelestialBodies[1]->GetOrbitDistance(), maxChildOrbit, RandomInt(seed), parentSeed, CELESTIAL_FLAG_IS_BINARY_2);
			if (orbits == 3) {
				childPosInGalaxy.level1 = 0;
				centralCelestialBodies[0] = GalaxyGenerator::MakeBinaryCenter(childPosInGalaxy, age, mass, /*parentMass*/0, /*parentRadius*/centralCelestialBodies[1]->GetOrbitDistance() * 5.0, parentOrbitalPlaneTiltDegrees, /*forcedOrbitDistance*/0, maxChildOrbit, RandomInt(seed), parentSeed, 0);
			}
		}
		_centralCelestialBodies = centralCelestialBodies;
	}
	return _centralCelestialBodies.value();
}
glm::vec4 StarSystem::GetVisibleColor() const {
	return glm::vec4(1,1,1, glm::pow(log10(GetMass()) - 28, 8.0) * 0.02); // Very quick approximation for performance reasons, good enough for now...
}
std::shared_ptr<Celestial> StarSystem::GetChild(uint64_t index) const {
	if (index <= GalacticPosition::MAX_CENTRAL_BODIES) {
		return GetCentralCelestialBodies()[index];
	}
	return nullptr;
}
