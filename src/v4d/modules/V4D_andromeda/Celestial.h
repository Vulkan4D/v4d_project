#pragma once
#include <v4d.h>
#include "celestials/CelestialType.hh"
#include "GalacticPosition.hpp"

class Celestial {
friend class StarSystem;
public:
	GalacticPosition galacticPosition;
protected:
	double age;
	double mass;
	double parentMass;
	double parentRadius; // special values may be -1 and -2 for the two binary members
	double parentOrbitalPlaneTiltDegrees;
	double maxChildOrbit;
	uint seed;
	
	mutable std::optional<double> _density = std::nullopt;
	mutable std::optional<double> _radius = std::nullopt;
	mutable std::optional<double> _orbitDistance = std::nullopt;
	mutable std::optional<double> _orbitPeriod = std::nullopt;
	mutable std::optional<double> _orbitalPlaneTiltDegrees = std::nullopt;
	mutable std::optional<double> _initialOrbitPosition = std::nullopt;
	mutable std::optional<bool> _tidallyLocked = std::nullopt;
	mutable std::optional<double> _tiltDegrees = std::nullopt;
	mutable std::optional<double> _rotationPeriod = std::nullopt;
	mutable std::optional<double> _initialRotation = std::nullopt;
	mutable std::optional<std::vector<std::shared_ptr<Celestial>>> _children = std::nullopt;
public:
	Celestial(GalacticPosition posInGalaxy, double age, double mass, double parentMass, double parentRadius, double parentOrbitalPlaneTiltDegrees, double forcedOrbitDistance, double maxChildOrbit, uint seed)
	: galacticPosition(posInGalaxy)
	, age(age)
	, mass(mass)
	, parentMass(parentMass)
	, parentRadius(parentRadius)
	, parentOrbitalPlaneTiltDegrees(parentOrbitalPlaneTiltDegrees)
	, maxChildOrbit(maxChildOrbit)
	, seed(seed)
	{if (forcedOrbitDistance > 0) _orbitDistance = forcedOrbitDistance;}
	
	double GetMass() const {return mass;} // Kg
	double GetAge() const {return age;} // Billions of years
	
	virtual double GetDensity() const; // kg/m3
	virtual double GetRadius() const; // Meters
	virtual double GetOrbitDistance() const;
	virtual double GetOrbitPeriod() const; // Seconds
	virtual double GetOrbitalPlaneTiltDegrees() const;
	virtual double GetInitialOrbitPosition() const;
	virtual bool GetTidallyLocked() const;
	virtual double GetTiltDegrees() const;
	virtual double GetRotationPeriod() const; // Seconds
	virtual double GetInitialRotation() const;
	virtual const std::vector<std::shared_ptr<Celestial>>& GetChildren() const;
	
	virtual CelestialType GetType() const = 0;
	
	constexpr int GetLevel() const {
		if (galacticPosition.level3) return 3;
		if (galacticPosition.level2) return 2;
		if (galacticPosition.level1) return 1;
		return 0;
	}
	
	std::shared_ptr<Celestial> GetChild(uint64_t index) const {
		if (index > 0 && index < GetChildren().size()) {
			return GetChildren()[index - 1];
		}
		return nullptr;
	}
	
	glm::dvec3 GetPositionInOrbit(double timestamp);
	
};

class BinaryCenter : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::BinaryCenter;}
};