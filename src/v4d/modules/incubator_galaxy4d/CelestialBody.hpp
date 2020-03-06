#pragma once

#include <v4d.h>
#include "UniversalPosition.hpp"

// enum class MATTER_STATES : uint8_t {
// 	ultrasolid = 0x1,
// 	solid = 0x2,
// 	glass_transition = 0x4,
// 	liquid = 0x8,
// 	gas = 0x10,
// 	plasma = 0x20,
// 	bec = 0x40,
// 	// ???? = 0x80,
// };

// struct CelestialSurfaceLayer {
// 	double startRadius, endRadius; // meters
// 	double minTemp, maxTemp; // kelvin
// 	double minPressure, maxPressure;  //   kg/m² (1 ~= 0.01 kPa)    // OR MAYBE    kPa (1 ~= 100 kg/m²)
// 	uint8_t states;
// 	std::map<int8_t, float> composition;
// };

struct CelestialBody {
	// Position/ID
	GalaxyID galaxyID;
	CelestialSystemID systemID;
	CelestialID celestialID;
	
	// Attributes
	enum : uint64_t {
		fusion = 0x1,
		atmosphere = 0x2,
		rocky_surface = 0x4,
		plasma_surface = 0x8,
		light_emission = 0x10,
		spherical_body = 0x20,
		has_moons = 0x40,
		has_rings = 0x80,
		//... 64 attributes maximum...
	};
	uint64_t attributes; // bits
	
	// Properties
	double age; // Millions of years
	double mass; // kilograms
	double influenceRadius; // meters
	
	// Orbit
	double orbitDistance; // meters
	glm::dvec3 orbitAxis; // normalized
	
	// Rotation
	double rotationSpeed; // degrees per hour (or arc-second per second, or arc-minute per minute... all the same)
	glm::dvec3 rotationAxis; // normalized
	
	// Layers
	double coreRadius;
	double mantleRadius;
	double surfaceRadius;
	
	// Satellites
	int nbSatellites = 0;
	
	// Specific to planets with atmosphere
	double atmosphereRadius {0};
	double atmosphereDensity {0};
	glm::vec3 atmosphereColor {0};
	
	std::vector<CelestialBody> satellites {};
	
	// std::vector<CelestialSurfaceLayer> layers {};
	
	void GenerateSatellites() {
		
	}
	
	double GetGravityAccelerationAtDistance(double distance) {
		return 0;//TODO
	}
	
};
