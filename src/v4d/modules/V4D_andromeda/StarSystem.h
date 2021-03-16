#pragma once
#include <v4d.h>
#include "Celestial.h"

class StarSystem {
	using CentralCelestialBodyList = std::array<std::shared_ptr<Celestial>, 3>;
	
	mutable std::optional<double> _radiusFactor = std::nullopt; // 0.0 - 1.0
	mutable std::optional<double> _radius = std::nullopt; // light-years
	mutable std::optional<double> _mass = std::nullopt;
	mutable std::optional<glm::dvec3> _offsetLY = std::nullopt;
	mutable std::optional<double> _age = std::nullopt;
	mutable std::optional<double> _orbitalPlaneTiltDegrees = std::nullopt;
	mutable std::optional<int> _nbPlanetaryOrbits = std::nullopt;
	mutable std::optional<int> _nbCentralBodies = std::nullopt;
	mutable std::optional<uint> _planetarySeed = std::nullopt;
	mutable std::optional<CentralCelestialBodyList> _centralCelestialBodies = std::nullopt;
public:
	GalacticPosition posInGalaxy;
	StarSystem(const glm::ivec3& posInGalaxy) : posInGalaxy(posInGalaxy) {}
	StarSystem(GalacticPosition posInGalaxy) : posInGalaxy(posInGalaxy) {
		this->posInGalaxy.celestialId = 0;
		this->posInGalaxy.isParentId = 0;
	}
	
	// Properties
	double GetRadiusFactor() const;
	double GetRadius() const; // Meters
	double GetMass() const; // Kg
	glm::dvec3 GetOffsetLY() const;
	double GetAge() const; // Billions of years
	double GetOrbitalPlaneTiltDegrees() const;
	int GetNbPlanetaryOrbits() const;
	int GetNbCentralBodies() const;
	uint GetPlanetarySeed() const;
	const CentralCelestialBodyList& GetCentralCelestialBodies() const;
	glm::vec4 GetVisibleColor() const;
	std::shared_ptr<Celestial> GetChild(uint64_t index) const;
};
