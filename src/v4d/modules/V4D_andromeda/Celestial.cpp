#include "Celestial.h"
#include "noise_functions.hpp"
#include "seeds.hh"
#include "GalaxyGenerator.h"
#include "StarSystem.h"

double Celestial::GetDensity() const { // kg/m3
	if (!_density.has_value()) {
		if (GetType() == CelestialType::BinaryCenter) {
			_density = 0;
		} else {
			uint seed = this->seed + SEED_CELESTIAL_DENSITY;
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
		if (parentMass == 0 || (GetLevel() == 1 && GetType() == CelestialType::BinaryCenter)) {
			_orbitDistance = 0;
		} else {
			uint seed = this->seed + SEED_CELESTIAL_ORBIT_DISTANCE;
			_orbitDistance = (parentRadius * 1.1 + GetRadius() * glm::pow(10.0, glm::mix(+1.81, +3.2, glm::pow(RandomFloat(seed), 1.6)))) * ((flags&CELESTIAL_FLAG_IS_BINARY_1 | flags&CELESTIAL_FLAG_IS_BINARY_2) ? 2.0 : 1.0);
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
		uint seed = this->seed + SEED_CELESTIAL_ORBITAL_PLANE_TILT;
		_orbitalPlaneTiltDegrees = parentOrbitalPlaneTiltDegrees + RandomFloat(seed) * GetIndex();
	}
	return _orbitalPlaneTiltDegrees.value();
}
double Celestial::GetInitialOrbitPosition() const {
	if (!_initialOrbitPosition.has_value()) {
		uint seed = this->seed + SEED_CELESTIAL_ORBIT_INITIAL_POSITION;
		uint parentSeed = this->parentSeed + SEED_CELESTIAL_ORBIT_INITIAL_POSITION_COMMON;
		if (flags&CELESTIAL_FLAG_IS_BINARY_1) {
			_initialOrbitPosition = RandomFloat(parentSeed)*TWOPI;
		} else if (flags&CELESTIAL_FLAG_IS_BINARY_2) {
			_initialOrbitPosition = RandomFloat(parentSeed)*TWOPI + PI; // 180 degrees from BINARY_1
		} else if (flags&CELESTIAL_FLAG_HAS_LAGRANGE_SIBLINGS) {
			_initialOrbitPosition = RandomFloat(parentSeed)*TWOPI;
		} else if (flags&CELESTIAL_FLAG_AT_L4) {
			_initialOrbitPosition = RandomFloat(parentSeed)*TWOPI + TWOPI/+6; // 30 degrees ahead of HAS_LAGRANGE_SIBLINGS
		} else if (flags&CELESTIAL_FLAG_AT_L5) {
			_initialOrbitPosition = RandomFloat(parentSeed)*TWOPI + TWOPI/-6; // 30 degrees behind HAS_LAGRANGE_SIBLINGS
		} else {
			_initialOrbitPosition = RandomFloat(seed) * TWOPI;
		}
	}
	return _initialOrbitPosition.value();
}
bool Celestial::GetTidallyLocked() const {
	if (!_tidallyLocked.has_value()) {
		uint seed = this->seed + SEED_CELESTIAL_TIDALLY_LOCKED;
		_tidallyLocked = false; //TODO
	}
	return _tidallyLocked.value();
}
double Celestial::GetTiltDegrees() const {
	if (!_tiltDegrees.has_value()) {
		uint seed = this->seed + SEED_CELESTIAL_TILT;
		_tiltDegrees = 0; //TODO
	}
	return _tiltDegrees.value();
}
double Celestial::GetRotationPeriod() const {
	if (!_rotationPeriod.has_value()) {
		uint seed = this->seed + SEED_CELESTIAL_ROTATION_PERIOD;
		_rotationPeriod = 0; //TODO
	}
	return _rotationPeriod.value();
}
double Celestial::GetInitialRotation() const {
	if (!_initialRotation.has_value()) {
		uint seed = this->seed + SEED_CELESTIAL_INITIAL_ROTATION;
		_initialRotation = 0; //TODO
	}
	return _initialRotation.value();
}
double Celestial::GetGravityAcceleration(double radius) const {
	return (G * GetMass()) / (radius*radius);
}
const std::vector<std::shared_ptr<Celestial>>& Celestial::GetChildren() const {
	if (!_children.has_value()) {
		uint parentSeed = this->seed + SEED_CELESTIAL_CHILDREN_COMMON;
		uint seed = parentSeed + SEED_CELESTIAL_CHILDREN;
		std::lock_guard lock(GalaxyGenerator::cacheMutex);
		std::vector<std::shared_ptr<Celestial>> children {};
		
		const int level = GetLevel();
		
		if (level == 2 && GetType() == CelestialType::BinaryCenter) {
			// Generate binary planets
			double age = GetAge();
			double mass = GetMass();
			double massDiff = glm::pow(RandomFloat(seed), 2.0) * 0.17;
			GalacticPosition childPosInGalaxy = galacticPosition;
			childPosInGalaxy.level3 = 1;
			children.push_back(GalaxyGenerator::MakeCelestial(childPosInGalaxy, age, mass * (0.5-massDiff), /*parent*/mass, /*parentRadius*/0, parentOrbitalPlaneTiltDegrees, 0, maxChildOrbit, RandomInt(seed), this->seed, CELESTIAL_FLAG_IS_BINARY_1));
			childPosInGalaxy.level3 = 2;
			children.push_back(GalaxyGenerator::MakeCelestial(childPosInGalaxy, age, mass * (0.5+massDiff), /*parent*/mass, /*parentRadius*/0, parentOrbitalPlaneTiltDegrees, children[0]->GetOrbitDistance(), maxChildOrbit, RandomInt(seed), this->seed, CELESTIAL_FLAG_IS_BINARY_2));
		} else {
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
					
					double childMass = LogarithmicMix(1e12, mass * 0.05, RandomFloat(seed));
					
					age *= 1.0 + glm::mix(-0.9, +0.85, CenterDistributedCurve(RandomFloat(seed)));
					if (RandomFloat(seed) > 0.99) age = glm::mix(STARSYSTEM_AGE_MIN, STARSYSTEM_AGE_MAX, RandomFloat(seed)); // < 1% chances of being a foreign object (completely different age)
					
					if (age < 0.0001) break; // early break, maybe this can be used in the future to to something cool instead without affecting existing stuff
					
					bool mayHaveSiblingsInLagrangePoint = childLevel >= 2 && childMass > 1e23 && (mass / childMass) > 25.0 && children.size()+4 < maxChildren && RandomFloat(seed) > 0.5;
					
					std::shared_ptr<Celestial> child = nullptr;
					double childTotalOrbitRadius;
					
					if (childLevel == 2 && RandomFloat(seed) > 0.95) { // 5% chances of having a binary planet
						while (childMass >= 1e26) childMass /= 4.0; // no binary gas giants
						
						child = GalaxyGenerator::MakeBinaryCenter(childPosInGalaxy, age, childMass, /*parent*/mass, /*parent*/orbitRadius, parentOrbitalPlaneTiltDegrees, 0, maxChildOrbit, RandomInt(seed), mayHaveSiblingsInLagrangePoint? this->seed : parentSeed, mayHaveSiblingsInLagrangePoint? CELESTIAL_FLAG_HAS_LAGRANGE_SIBLINGS : 0);
						childTotalOrbitRadius = child->GetOrbitDistance() * 1.1;
						
						// Check for errors
						if (childTotalOrbitRadius > maxChildOrbit) {
							GalaxyGenerator::ClearCelestialCache(childPosInGalaxy.rawValue);
							break;
						}
						children.push_back(child);
					} else {
						child = GalaxyGenerator::MakeCelestial(childPosInGalaxy, age, childMass, /*parent*/mass, /*parent*/orbitRadius, parentOrbitalPlaneTiltDegrees, 0, maxChildOrbit, RandomInt(seed), mayHaveSiblingsInLagrangePoint? this->seed : parentSeed, mayHaveSiblingsInLagrangePoint? CELESTIAL_FLAG_HAS_LAGRANGE_SIBLINGS : 0);
						childTotalOrbitRadius = child->GetOrbitDistance() + child->GetRadius() * 2.0;
						
						// Check for errors
						if (childTotalOrbitRadius > maxChildOrbit) {
							GalaxyGenerator::ClearCelestialCache(childPosInGalaxy.rawValue);
							break;
						}
						children.push_back(child);
					}
					
					orbitRadius = childTotalOrbitRadius * 1.2;
					
					// L4 & L5 lagrange points
					if (mayHaveSiblingsInLagrangePoint) {
						if (RandomFloat(seed) > 0.3) {
							// Add L4
							switch (childLevel) {
								case 2:
									childPosInGalaxy.level2 = children.size() + 1;
									break;
								case 3:
									childPosInGalaxy.level3 = children.size() + 1;
									break;
							}
							children.push_back(GalaxyGenerator::MakeCelestial(childPosInGalaxy, LogarithmicMix(0.0001, age * 0.1, RandomFloat(seed)), childMass * RandomFloat(seed) * 0.04, /*parent*/mass, /*parent*/orbitRadius, parentOrbitalPlaneTiltDegrees, child->GetOrbitDistance(), maxChildOrbit, RandomInt(seed), this->seed, CELESTIAL_FLAG_AT_L4));
						}
						if (RandomFloat(seed) > 0.3) {
							// Add L5
							switch (childLevel) {
								case 2:
									childPosInGalaxy.level2 = children.size() + 1;
									break;
								case 3:
									childPosInGalaxy.level3 = children.size() + 1;
									break;
							}
							children.push_back(GalaxyGenerator::MakeCelestial(childPosInGalaxy, LogarithmicMix(0.0001, age * 0.1, RandomFloat(seed)), childMass * RandomFloat(seed) * 0.04, /*parent*/mass, /*parent*/orbitRadius, parentOrbitalPlaneTiltDegrees, child->GetOrbitDistance(), maxChildOrbit, RandomInt(seed), this->seed, CELESTIAL_FLAG_AT_L5));
						}
					}
					
					if (orbitRadius > maxChildOrbit) break;
				}
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
			case 1: return {(flags&CELESTIAL_FLAG_IS_BINARY_2)? -orbitDistance : orbitDistance,0,0};
			case 2: return {(flags&CELESTIAL_FLAG_AT_L4)? +orbitDistance*0.2 : ((flags&CELESTIAL_FLAG_AT_L5)? -orbitDistance*0.2 : 0) ,orbitDistance,0};
			case 3: return {(flags&CELESTIAL_FLAG_AT_L4)? +orbitDistance*0.2 : ((flags&CELESTIAL_FLAG_AT_L5)? -orbitDistance*0.2 : 0) ,0,(flags&CELESTIAL_FLAG_IS_BINARY_2)? -orbitDistance : orbitDistance};
		}
	}
	
	const double tilt = glm::radians(GetOrbitalPlaneTiltDegrees());
	const double M = glm::sqrt(parentMass * G / glm::pow(orbitDistance, 3.0)) * (timestamp + SEED_TIMESTAMP_OFFSET) + GetInitialOrbitPosition();
	return glm::dvec3(
		glm::cos(tilt) * glm::cos(M),
		glm::sin(tilt) * glm::cos(M),
		glm::sin(M)
	) * orbitDistance;
}

void Celestial::RenderUpdate(glm::dvec3 position, glm::dvec3 cameraPosition, double sizeInScreen/* > 0.001 */) const {
	auto& sphere = renderableEntities["sphere"];
	if (!sphere) {
		sphere = v4d::graphics::RenderableGeometryEntity::Create(THIS_MODULE, GetID());
		sphere->generator = [this](v4d::graphics::RenderableGeometryEntity* entity, v4d::graphics::vulkan::Device* device){
			double radius = GetRadius();
			entity->Allocate(device, "V4D_raytracing:aabb_sphere");
			entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
			entity->Add_meshVertexColorU8()->AllocateBuffers(device, {255,0,0,255});
		};
	}
	sphere->SetWorldTransform(glm::translate(glm::dmat4(1), position));
}


double Celestial::GetRevolutionTime(double orbitRadius, double parentMass) {
	if (orbitRadius == 0) return 0;
	return 2.0 * PI * glm::sqrt(glm::pow(orbitRadius, 3.0) / (parentMass * G));
}
