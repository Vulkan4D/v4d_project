#pragma once
#include <v4d.h>
#include <optional>
#include <vector>
#include <memory>
#include <unordered_map>
#include "utilities/graphics/RenderableGeometryEntity.h"

#include "celestials/CelestialType.hh"
#include "GalacticPosition.hpp"
#include "GalaxyGenerator.h"

static const uint32_t CELESTIAL_FLAG_IS_CENTER_STAR = 1;
static const uint32_t CELESTIAL_FLAG_IS_BINARY_1 = 1<<1;
static const uint32_t CELESTIAL_FLAG_IS_BINARY_2 = 1<<2;
static const uint32_t CELESTIAL_FLAG_HAS_LAGRANGE_SIBLINGS = 1<<3;
static const uint32_t CELESTIAL_FLAG_AT_L4 = 1<<4;
static const uint32_t CELESTIAL_FLAG_AT_L5 = 1<<5;

class Celestial {
friend class StarSystem;
public:
	GalacticPosition galacticPosition;
protected:
	double age;
	double mass;
	double parentMass;
	double parentRadius;
	double parentOrbitalPlaneTiltDegrees;
	double maxChildOrbit;
	uint seed;
	uint parentSeed;
	uint32_t flags;
	
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
	Celestial(GalacticPosition posInGalaxy, double age, double mass, double parentMass, double parentRadius, double parentOrbitalPlaneTiltDegrees, double forcedOrbitDistance, double maxChildOrbit, uint seed, uint parentSeed, uint32_t flags)
	: galacticPosition(posInGalaxy)
	, age(age)
	, mass(mass)
	, parentMass(parentMass)
	, parentRadius(parentRadius)
	, parentOrbitalPlaneTiltDegrees(parentOrbitalPlaneTiltDegrees)
	, maxChildOrbit(maxChildOrbit)
	, seed(seed)
	, parentSeed(parentSeed)
	, flags(flags)
	{if (forcedOrbitDistance > 0) _orbitDistance = forcedOrbitDistance;}
	
	~Celestial() {
		RenderDelete();
	}
	
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
	
	mutable std::unordered_map<std::string, std::shared_ptr<v4d::graphics::RenderableGeometryEntity>> renderableEntities {};
	
	virtual CelestialType GetType() const = 0;
	
	virtual void RenderUpdate(glm::dvec3 position, glm::dvec3 cameraPosition, double sizeInScreen) const;
	
	virtual void RenderDelete() const {
		for (auto&[name, entity] : renderableEntities) if (entity) {
			entity->Destroy();
		}
		renderableEntities.clear();
	}
	
	constexpr uint64_t GetID() const {
		return galacticPosition.value;
	}
	
	constexpr int GetLevel() const {
		if (galacticPosition.level3) return 3;
		if (galacticPosition.level2) return 2;
		if (galacticPosition.level1) return 1;
		return 0;
	}
	
	constexpr int GetIndex() const {
		if (galacticPosition.level3) return galacticPosition.level3;
		if (galacticPosition.level2) return galacticPosition.level2;
		if (galacticPosition.level1) return galacticPosition.level1;
		return 0;
	}
	
	constexpr bool Is(uint32_t flag) const {
		return flags & flag;
	}
	
	std::shared_ptr<Celestial> GetChild(uint64_t index) const {
		if (index > 0 && index < GetChildren().size()) {
			return GetChildren()[index - 1];
		}
		return nullptr;
	}
	
	glm::dvec3 GetPositionInOrbit(double timestamp);
	
	template<typename T = glm::dvec3>
	T GetAbsolutePositionInOrbit(double timestamp) {
		T positionInOrbit = GetPositionInOrbit(timestamp);
		GalacticPosition galacticPosition = this->galacticPosition;
		if (GetLevel() > 2) {
			galacticPosition.level3 = 0;
			positionInOrbit += GalaxyGenerator::GetCelestial(galacticPosition)->GetPositionInOrbit(timestamp);
		}
		if (GetLevel() > 1) {
			galacticPosition.level2 = 0;
			positionInOrbit += GalaxyGenerator::GetCelestial(galacticPosition)->GetPositionInOrbit(timestamp);
		}
		return positionInOrbit;
	}

	static double GetRevolutionTime(double orbitRadius, double parentMass);
};

class BinaryCenter : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::BinaryCenter;}
	virtual void RenderUpdate(glm::dvec3, glm::dvec3, double) const override {}
};
