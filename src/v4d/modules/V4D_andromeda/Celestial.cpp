#include "Celestial.h"
#include "noise_functions.hpp"
#include "seeds.hh"
#include "GalaxyGenerator.h"

double Celestial::GetDensity() const { // kg/m3
	if (!_density.has_value()) {
		if (GetType() == CelestialType::BinaryCenter) {
			_density = 0;
		} else {
			uint seed = this->seed + 1;
			switch (GetType()) {
				case CelestialType::Asteroid:
					_density = glm::mix(1000.0, 8000.0, CenterDistributedCurve(RandomFloat(seed)));
					break;
				case CelestialType::Planet:
					_density = glm::mix(2500.0, 8000.0, CenterDistributedCurve(RandomFloat(seed)));
					break;
				case CelestialType::GasGiant:
					_density = glm::mix(400.0, 2500.0, CenterDistributedCurve(RandomFloat(seed)));
					break;
				case CelestialType::BrownDwarf:
					_density = glm::pow(10.0, glm::mix(+4, +6, CenterDistributedCurve(RandomFloat(seed))));
					break;
				case CelestialType::Star:
					// main sequence: 1E-4 to 1E+5
					// red dwarf : 1E+3 to 1E+7
					// white dwarf : 1E+7 to 1E+10
					_density = glm::pow(10.0, glm::mix(-4, +10, glm::pow(RandomFloat(seed), 4.0)));
					break;
				case CelestialType::HyperGiant:
					_density = glm::mix(1E-6, 1E-4, RandomFloat(seed));
					break;
				case CelestialType::BlackHole:
					_density = glm::mix(1E17, 1E18, RandomFloat(seed));
					break;
				case CelestialType::SuperMassiveBlackHole:
					_density = glm::mix(3E17, 5E18, RandomFloat(seed));
					break;
			}
		}
	}
	return _density.value();
}
double Celestial::GetRadius() const {
	if (!_radius.has_value()) {
		if (GetType() == CelestialType::BinaryCenter) {
			_radius = 0;
		} else if (GetType() == CelestialType::BlackHole || GetType() == CelestialType::SuperMassiveBlackHole) {
			_radius = 1; //TODO calculate event horizon instead
		} else {
			_radius = glm::pow(3.0 * (GetMass() / GetDensity()) / (4.0 * PI), 1.0/3);
		}
	}
	return _radius.value();
}
double Celestial::GetOrbitDistance() const {
	if (!_orbitDistance.has_value()) {
		if (parentMass == 0 || parentRadius == 0) {
			_orbitDistance = 0;
		} else {
			uint seed = this->seed + 2;
			_orbitDistance = (glm::abs(parentRadius) * 1.1 + GetRadius() * glm::pow(10.0, glm::mix(+1.81, +3.2, glm::pow(RandomFloat(seed), 1.6)))) * (parentRadius < 0 ? 2.0 : 1.0);
		}
	}
	return _orbitDistance.value();
}
double Celestial::GetOrbitPeriod() const {
	if (!_orbitPeriod.has_value()) {
		const double orbitDistance = GetOrbitDistance();
		if (orbitDistance == 0) return 0;
		return TWOPI * glm::sqrt(glm::pow(orbitDistance, 3.0) / (parentMass * G));
	}
	return _orbitPeriod.value();
}
double Celestial::GetOrbitalPlaneTiltDegrees() const {
	if (!_orbitalPlaneTiltDegrees.has_value()) {
		uint seed = this->seed + 3;
		_orbitalPlaneTiltDegrees = parentOrbitalPlaneTiltDegrees; //TODO
	}
	return _orbitalPlaneTiltDegrees.value();
}
double Celestial::GetInitialOrbitPosition() const {
	if (!_initialOrbitPosition.has_value()) {
		uint seed = this->seed + 4;
		if (parentRadius == -1) {
			_initialOrbitPosition = 0;
		} else if (parentRadius ==-2) {
			_initialOrbitPosition = PI;
		} else {
			_initialOrbitPosition = RandomFloat(seed) * TWOPI;
		}
	}
	return _initialOrbitPosition.value();
}
bool Celestial::GetTidallyLocked() const {
	if (!_tidallyLocked.has_value()) {
		uint seed = this->seed + 5;
		_tidallyLocked = false; //TODO
	}
	return _tidallyLocked.value();
}
double Celestial::GetTiltDegrees() const {
	if (!_tiltDegrees.has_value()) {
		uint seed = this->seed + 6;
		_tiltDegrees = 0; //TODO
	}
	return _tiltDegrees.value();
}
double Celestial::GetRotationPeriod() const {
	if (!_rotationPeriod.has_value()) {
		uint seed = this->seed + 7;
		_rotationPeriod = 0; //TODO
	}
	return _rotationPeriod.value();
}
double Celestial::GetInitialRotation() const {
	if (!_initialRotation.has_value()) {
		uint seed = this->seed + 8;
		_initialRotation = 0; //TODO
	}
	return _initialRotation.value();
}
const std::vector<std::shared_ptr<Celestial>>& Celestial::GetChildren() const {
	if (!_children.has_value()) {
		uint seed = this->seed + 9;
		std::lock_guard lock(GalaxyGenerator::cacheMutex);
		std::vector<std::shared_ptr<Celestial>> children {};
		
		const int level = GetLevel();
		int childLevel;
		int maxChildren;
		switch (level) {
			case 0:
			case 1:
				maxChildren = glm::round(CenterDistributedCurve(RandomFloat(seed)) * GalacticPosition::MAX_PLANETS);
				childLevel = 2;
				break;
			case 2:
				maxChildren = glm::round(glm::pow(RandomFloat(seed), 2.0) * glm::mix(3.0, double(GalacticPosition::MAX_MOONS), glm::clamp(glm::smoothstep(23.0, 27.0, log10(GetMass())), 0.0, 1.0)));
				childLevel = 3;
				break;
			case 3:
				maxChildren = 0;
				break;
		}
		
		if (maxChildren > 0) {
			double age = GetAge();
			double mass = GetMass();
			double orbitRadius = GetRadius();
			if (orbitRadius == 0) orbitRadius = parentRadius;
			double parentOrbitalPlaneTiltDegrees = GetOrbitalPlaneTiltDegrees();
			double orbitDistance = GetOrbitDistance();
			double maxChildOrbit = orbitDistance==0? this->maxChildOrbit : glm::min(this->maxChildOrbit, orbitDistance * 0.02);
			
			while (children.size() < maxChildren) {
				GalacticPosition childPosInGalaxy = galacticPosition;
				switch (childLevel) {
					case 2:
						childPosInGalaxy.level2 = children.size() + 1;
						break;
					case 3:
						childPosInGalaxy.level3 = children.size() + 1;
						break;
				}
				
				double childMass = mass * glm::pow(RandomFloat(seed), 10.0) * 0.02;
				
				if (mass < 1E11) break; // don't make very small objects (like asteroids < 500m diameter)
				
				age *= 1.0 + glm::mix(-0.9, +0.85, CenterDistributedCurve(RandomFloat(seed)));
				if (RandomFloat(seed) > 0.99) age = glm::mix(STARSYSTEM_AGE_MIN, STARSYSTEM_AGE_MAX, RandomFloat(seed)); // < 1% chances of being a foreign object (completely different age)
				
				if (age < 0.0001) break;
				
				if (childLevel == 2 && RandomFloat(seed) > 0.99) {
					
					break; //TODO binary
					
				} else {
					auto child = GalaxyGenerator::MakeCelestial(childPosInGalaxy, age, childMass, mass, /*parent*/orbitRadius, parentOrbitalPlaneTiltDegrees, 0, maxChildOrbit, RandomInt(seed));
					double childTotalOrbitRadius = child->GetOrbitDistance() + child->GetRadius() * 2.0;
					
					// Check for errors
					if (childTotalOrbitRadius > maxChildOrbit) {
						GalaxyGenerator::ClearCelestialCache(childPosInGalaxy.rawValue);
						break;
					}
					children.push_back(child);
					orbitRadius = childTotalOrbitRadius * 1.2;
				}
				
				if (orbitRadius > maxChildOrbit) break;
			}
		}
		
		_children = children;
	}
	return _children.value();
}

glm::dvec3 Celestial::GetPositionInOrbit(double timestamp) {
	const double orbitDistance = GetOrbitDistance();
	if (orbitDistance == 0) return {0,0,0};
	
	if (timestamp == 0) {
		switch(GetLevel()) {
			case 1: return {parentRadius==-2? -orbitDistance : orbitDistance,0,0};
			case 2: return {0,orbitDistance,0};
			case 3: return {0,0,orbitDistance};
		}
	}
	
	const double tilt = glm::radians(GetOrbitalPlaneTiltDegrees());
	const double M = glm::sqrt(parentMass * G / glm::pow(orbitDistance, 3.0)) * timestamp + GetInitialOrbitPosition();
	return glm::dvec3(
		glm::cos(tilt) * glm::cos(M),
		glm::sin(tilt) * glm::cos(M),
		glm::sin(M)
	) * orbitDistance;
}
