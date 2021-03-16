#pragma once

#include <v4d.h>
#include "GalacticPosition.hpp"
#include <tgmath.h>

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

#pragma region Seeds

constexpr float SEED_STARSYSTEM_PRESENCE = 0;
constexpr float SEED_STARSYSTEM_MASS = 1;
constexpr float SEED_STARSYSTEM_RADIUS = 2;
constexpr float SEED_STARSYSTEM_OFFSET_X = 3;
constexpr float SEED_STARSYSTEM_OFFSET_Y = 4;
constexpr float SEED_STARSYSTEM_OFFSET_Z = 5;
constexpr float SEED_STARSYSTEM_AGE = 6;
constexpr float SEED_STARSYSTEM_ORBITAL_PLANE_TILT = 7;
constexpr float SEED_STARSYSTEM_NB_ORBITS = 8;
constexpr float SEED_STARSYSTEM_PLANETARY_SEED = 9;
constexpr float SEED_STARSYSTEM_PLANETARY_SEED_2 = 181210.63;

#pragma endregion

#pragma region Noise Functions

	static uint RandomInt(uint& seed) {
		// LCG values from Numerical Recipes
		return (seed = 1664525 * seed + 1013904223);
	}
	
	static float RandomFloat(uint& seed) {
		// Float version using bitmask from Numerical Recipes
		const uint one = 0x3f800000;
		const uint msk = 0x007fffff;
		return glm::uintBitsToFloat(one | (msk & (RandomInt(seed) >> 9))) - 1;
	}
	
	static glm::vec3 RandomInUnitSphere(uint& seed) {
		for (;;) {
			glm::vec3 p = 2.0f * glm::vec3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed)) - 1.0f;
			if (dot(p, p) < 1) return p;
		}
	}
	
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
	
	inline static float UniformFloatFromStarPos(glm::ivec3 p, float seed) { // returns a uniformly-distributed float between 0.0 and 1.0
		return noise2(glm::vec2(noise3(glm::vec3(p.x*0.4321f, p.y*0.964f, p.z*0.15623f)), seed*8.12533f));
	}
	
	inline static double CenterDistributedCurve(double rand01) { // given a uniformly-distributed float between 0.0 and 1.0, it returns a new float that is distributed towards the center. May be called recursively for steeper central distribution.
		return glm::pow((rand01 * 2.0 - 1.0) * 0.7937005259840996, 3.0) + 0.5;
	}
	
	inline static double LogarithmicMix(double min, double max, double x) {
		double logMin = glm::log2(min);
		double logMax = glm::log2(max);
		double e = glm::mix(logMin, logMax, x);
		return glm::pow(2.0, e);
	}
	
#pragma endregion

class StarSystem;
class Celestial;
enum class CelestialType;

class GalaxyGenerator {
public:
	static constexpr double G = 6.67408E-11;
	static constexpr double PI = 3.14159265359;
	static constexpr double TWOPI = 2.0 * PI;

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
	
	static float GetGalaxyDensity(const glm::vec3& pos) { // expects a pos with values between -1.0 and +1.0
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
	
	static bool GetStarSystemPresence(const glm::ivec3& posInGalaxy) {
		return UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_PRESENCE) < glm::clamp(GetGalaxyDensity(GalacticPosition::ToGalaxyDensityPos(posInGalaxy)), 0.0f, 1.0f) * STAR_CHUNK_DENSITY_MULT;
	}
	
	static std::shared_ptr<StarSystem> GetStarSystem(const glm::ivec3& posInGalaxy) {
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
	
	static std::shared_ptr<Celestial> GetCelestial(GalacticPosition galacticPosition);
	
	static void ClearCache() {
		std::lock_guard lock(cacheMutex);
		celestials.clear();
		starSystems.clear();
	}
	
	static void ClearStarSystemCache(const glm::ivec3& posInGalaxy) {
		std::lock_guard lock(cacheMutex);
		if (starSystems.count(GalacticPosition(posInGalaxy).posInGalaxy)) {
			starSystems.erase(GalacticPosition(posInGalaxy).posInGalaxy);
		}
	}
	
	static void ClearCelestialCache(GalacticPosition posInGalaxy) {
		std::lock_guard lock(cacheMutex);
		if (celestials.count(posInGalaxy.rawValue)) {
			celestials.erase(posInGalaxy.rawValue);
		}
	}
	
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

class StarSystem {
	mutable std::optional<double> _radiusFactor = std::nullopt; // 0.0 - 1.0
	mutable std::optional<double> _radius = std::nullopt; // light-years
	mutable std::optional<double> _mass = std::nullopt;
	mutable std::optional<glm::dvec3> _offsetLY = std::nullopt;
	mutable std::optional<double> _age = std::nullopt;
	mutable std::optional<double> _orbitalPlaneTiltDegrees = std::nullopt;
	mutable std::optional<int> _nbPlanetaryOrbits = std::nullopt;
	mutable std::optional<int> _nbCentralBodies = std::nullopt;
	mutable std::optional<uint> _planetarySeed = std::nullopt;
	mutable std::optional<std::array<std::shared_ptr<Celestial>, 3>> _centralCelestialBodies = std::nullopt;
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
	const auto& GetCentralCelestialBodies() const;
	glm::vec4 GetVisibleColor() const;
	std::shared_ptr<Celestial> GetChild(uint64_t index) const;
};

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

#pragma region Celestial Types

enum class CelestialType {
	BinaryCenter,
	RingObject,
	Asteroid,
	Planet,
	GasGiant,
	BrownDwarf,
	Star,
	HyperGiant,
	BlackHole,
	SuperMassiveBlackHole,
};

class BinaryCenter : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::BinaryCenter;}
};

class Asteroid : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::Asteroid;}
};

class Planet : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::Planet;}
};

class GasGiant : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::GasGiant;}
};

class BrownDwarf : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::BrownDwarf;}
};

class Star : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::Star;}
};

class HyperGiant : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::HyperGiant;}
};

class BlackHole : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::BlackHole;}
};

class SuperMassiveBlackHole : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::SuperMassiveBlackHole;}
};

#pragma endregion

#pragma region GalaxyGenerator Implementation

std::recursive_mutex GalaxyGenerator::cacheMutex {};
std::unordered_map<uint64_t, std::shared_ptr<StarSystem>> GalaxyGenerator::starSystems {};
std::unordered_map<uint64_t, std::shared_ptr<Celestial>> GalaxyGenerator::celestials {};

std::shared_ptr<Celestial> GalaxyGenerator::MakeCelestial(GalacticPosition galacticPosition, double age, double mass, double parentMass, double parentRadius, double parentOrbitalPlaneTiltDegrees, double forcedOrbitDistance, double maxOrbitRadius, uint seed) {
	std::lock_guard lock(cacheMutex);
	try {
		return celestials.at(galacticPosition.rawValue);
	} catch(std::out_of_range) {
		if (mass < 1E21) {
			return celestials[galacticPosition.rawValue] = std::make_shared<Asteroid>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed);
		}
		if (mass < 1E26) {
			return celestials[galacticPosition.rawValue] = std::make_shared<Planet>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed);
		}
		if (mass < 1E28) {
			return celestials[galacticPosition.rawValue] = std::make_shared<GasGiant>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed);
		}
		if (mass < 1E29) {
			return celestials[galacticPosition.rawValue] = std::make_shared<BrownDwarf>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed);
		}
		if (mass < 1E32) {
			if (RandomFloat(seed) < 0.002) return celestials[galacticPosition.rawValue] = std::make_shared<BlackHole>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed);
			return celestials[galacticPosition.rawValue] = std::make_shared<Star>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed);
		}
		if (mass < 1E35) {
			if (RandomFloat(seed) < 0.0001) return celestials[galacticPosition.rawValue] = std::make_shared<BlackHole>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed);
			return celestials[galacticPosition.rawValue] = std::make_shared<HyperGiant>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed);
		}
		return celestials[galacticPosition.rawValue] = std::make_shared<SuperMassiveBlackHole>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed);
	}
}

std::shared_ptr<Celestial> GalaxyGenerator::MakeBinaryCenter(GalacticPosition galacticPosition, double age, double mass, double parentMass, double parentRadius, double parentOrbitalPlaneTiltDegrees, double forcedOrbitDistance, double maxOrbitRadius, uint seed) {
	std::lock_guard lock(cacheMutex);
	return celestials[galacticPosition.rawValue] = std::make_shared<BinaryCenter>(galacticPosition, age, mass, parentMass, parentRadius, parentOrbitalPlaneTiltDegrees, forcedOrbitDistance, maxOrbitRadius, seed);
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

#pragma endregion

#pragma region Star System

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
		_planetarySeed = UniformFloatFromStarPos(posInGalaxy, SEED_STARSYSTEM_PLANETARY_SEED) * SEED_STARSYSTEM_PLANETARY_SEED_2;
	}
	return _planetarySeed.value();
}
const auto& StarSystem::GetCentralCelestialBodies() const {
	if (!_centralCelestialBodies.has_value()) {
		std::lock_guard lock(GalaxyGenerator::cacheMutex);
		std::array<std::shared_ptr<Celestial>, 3> centralCelestialBodies {nullptr, nullptr, nullptr};
		
		GalacticPosition childPosInGalaxy = posInGalaxy;
		uint seed = GetPlanetarySeed();
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
			centralCelestialBodies[1] = GalaxyGenerator::MakeCelestial(childPosInGalaxy, age, mass, /*parentMass*/0, /*parentRadius*/0, parentOrbitalPlaneTiltDegrees, /*forcedOrbitDistance*/0, maxChildOrbit, RandomInt(seed));
		} else if (n == 2) {
			// Binary star
			double massDiff = glm::pow(RandomFloat(seed), 2.0) * 0.17;
			childPosInGalaxy.level1 = 1;
			centralCelestialBodies[1] = GalaxyGenerator::MakeCelestial(childPosInGalaxy, age, mass * (0.5-massDiff), /*parentMass*/mass, /*parentRadius*/-1, parentOrbitalPlaneTiltDegrees, /*forcedOrbitDistance*/0, maxChildOrbit, RandomInt(seed));
			childPosInGalaxy.level1 = 2;
			centralCelestialBodies[2] = GalaxyGenerator::MakeCelestial(childPosInGalaxy, age, mass * (0.5+massDiff), /*parentMass*/mass, /*parentRadius*/-2, parentOrbitalPlaneTiltDegrees, /*forcedOrbitDistance*/centralCelestialBodies[1]->GetOrbitDistance(), maxChildOrbit, RandomInt(seed));
			if (orbits == 3) {
				childPosInGalaxy.level1 = 0;
				centralCelestialBodies[0] = GalaxyGenerator::MakeBinaryCenter(childPosInGalaxy, age, mass, /*parentMass*/0, /*parentRadius*/0, parentOrbitalPlaneTiltDegrees, (centralCelestialBodies[1]->GetOrbitDistance() + centralCelestialBodies[2]->GetOrbitDistance()) * 12.0, maxChildOrbit, RandomInt(seed));
			}
		}
		_centralCelestialBodies = centralCelestialBodies;
	}
	return _centralCelestialBodies.value();
}
glm::vec4 StarSystem::GetVisibleColor() const {
	return glm::vec4(1,1,1, glm::pow(log10(GetMass()) - 28, 2.0) * 0.01); // Very quick approximation for performance reasons, good enough for now...
}
std::shared_ptr<Celestial> StarSystem::GetChild(uint64_t index) const {
	if (index <= GalacticPosition::MAX_CENTRAL_BODIES) {
		return GetCentralCelestialBodies()[index];
	}
	return nullptr;
}

#pragma endregion

#pragma region Celestial

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
			_radius = glm::pow(3.0 * (GetMass() / GetDensity()) / (4.0 * GalaxyGenerator::PI), 1.0/3);
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
		return GalaxyGenerator::TWOPI * glm::sqrt(glm::pow(orbitDistance, 3.0) / (parentMass * GalaxyGenerator::G));
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
			_initialOrbitPosition = GalaxyGenerator::PI;
		} else {
			_initialOrbitPosition = RandomFloat(seed) * GalaxyGenerator::TWOPI;
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
	const double M = glm::sqrt(parentMass * GalaxyGenerator::G / glm::pow(orbitDistance, 3.0)) * timestamp + GetInitialOrbitPosition();
	return glm::dvec3(
		glm::cos(tilt) * glm::cos(M),
		glm::sin(tilt) * glm::cos(M),
		glm::sin(M)
	) * orbitDistance;
}

#pragma endregion
