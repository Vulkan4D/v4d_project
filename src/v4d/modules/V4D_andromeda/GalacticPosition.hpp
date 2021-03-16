#pragma once

// Conversions (light-year, Astronomical unit, kilometer, meter)

// #define LY2M(n)		((n) * 9.461E+15)
// #define M2LY(n)		((n) * 1.056970721911E-16)
#define LY2M(n)		((n) * 1.0E+16)
#define M2LY(n)		((n) * 1.0E-16)
#define LY2M_INT(n)		((n) * int64_t(1.0E+16))
#define M2LY_INT(n)		((n) / int64_t(1.0E+16))

// #define AU2LY(n)	((n) * 1.5812056939013E-5)
// #define LY2AU(n)	((n) * 63242.878763782)
// #define AU2KM(n)	((n) * 149597870.7)
// #define KM2AU(n)	((n) * 6.6845871222684E-9)
// #define AU2M(n)		((n) * 149597870700)
// #define M2AU(n)		((n) * 6.6845871222684E-12)
// #define LY2KM(n)	((n) * 9461000000000)
// #define KM2LY(n)	((n) * 1.056970721911E-13)

struct GalacticPosition {
	static constexpr int X_BOTTOM_VALUE = 1;
	static constexpr int Y_BOTTOM_VALUE = 1;
	static constexpr int Z_BOTTOM_VALUE = 1;
	static constexpr int X_TOP_VALUE = 262'143;
	static constexpr int Y_TOP_VALUE = 4095;
	static constexpr int Z_TOP_VALUE = 262'143;
	static constexpr int GALAXY_SCALE = 250'000; // max diameter in light-years
	static constexpr int MAX_PLANETARY_ORBITS = 3;
	static constexpr int MAX_CENTRAL_BODIES = 2;
	static constexpr int MAX_PLANETS = 30;
	static constexpr int MAX_MOONS = 250;
	
	static glm::vec3 ToGalaxyDensityPos(const glm::dvec3& posInGalaxy) { // converts range from BOTTOM_VALUE TOP_VALUE to range ~ -1.0 +1.0 (may overflow a little bit because GALAXY_SCALE can be smaller than TOP_VALUE)
		return (posInGalaxy - (glm::dvec3(X_TOP_VALUE, Y_TOP_VALUE, Z_TOP_VALUE) / 2.0)) / double(GALAXY_SCALE) * 2.0;
	}
	
	union {
		uint64_t rawValue;
		struct {
			uint64_t value : 63;
			uint64_t : 1;
		};
		struct {
			uint64_t parentId : 63;
			uint64_t isParentId : 1;
		};
		struct {
			uint64_t posInGalaxy_x : 18; // 1-262143
			uint64_t posInGalaxy_y : 12; // 1-4095
			uint64_t posInGalaxy_z : 18; // 1-262143
			uint64_t level1 : 2; // 2 central bodies (black hole | star | planet) / 3 planetary orbits + one reserved for future use (=3)
			uint64_t level2 : 5; // 30 stars/planets + one reserved for future use (=31)
			uint64_t level3 : 8; // 250 moons + belt(=251) + comet(=252) + three reserved for future use (=253,254,255)
			uint64_t : 1;
		};
		struct {
			uint64_t posInGalaxy : 48;
			uint64_t celestialId : 15;
			uint64_t : 1;
		};
	};
	
	GalacticPosition() : rawValue(0) {static_assert(sizeof(GalacticPosition) == 8);}
	GalacticPosition(uint64_t rawValue) : rawValue(rawValue) {}
	GalacticPosition(const glm::ivec3& posInGalaxy)
		: posInGalaxy_x(posInGalaxy.x)
		, posInGalaxy_y(posInGalaxy.y)
		, posInGalaxy_z(posInGalaxy.z)
	{
		celestialId = 0;
		isParentId = 0;
	}
	
	operator glm::ivec3() const {
		return {posInGalaxy_x, posInGalaxy_y, posInGalaxy_z};
	}
	
	constexpr bool IsValid() const {return rawValue != 0 && (!isParentId || parentId > 0) && (isParentId || (posInGalaxy_x >= X_BOTTOM_VALUE && posInGalaxy_y >= Y_BOTTOM_VALUE && posInGalaxy_z >= Z_BOTTOM_VALUE && posInGalaxy_x <= X_TOP_VALUE && posInGalaxy_y <= Y_TOP_VALUE && posInGalaxy_z <= Z_TOP_VALUE));}
	constexpr bool IsParent() const {return IsValid() && isParentId;}
	constexpr bool IsReferenceFrame() const {return IsValid() && !IsParent() && celestialId == 0;}
	constexpr bool IsCelestial() const {return IsValid() && !IsParent() && celestialId > 0 && level1 < 3 && level2 < 31 && level3 < 251;}
	
	// Extra (requires an additional 64-bit int)
	constexpr bool IsBeltObject() const {return IsValid() && !IsParent() && level3 == 251;}
	constexpr bool IsComet() const {return IsValid() && !IsParent() && level3 == 252;}
	// reserved for future use (may also make use of the additional 64-bit int if needed)
	constexpr bool _IsReservedLevel1_() const {return IsValid() && !IsParent() && level1 == 3;}
	constexpr bool _IsReservedLevel2_() const {return IsValid() && !IsParent() && level2 == 31;}
	constexpr bool _IsReservedLevel3_1_() const {return IsValid() && !IsParent() && level3 == 253;}
	constexpr bool _IsReservedLevel3_2_() const {return IsValid() && !IsParent() && level3 == 254;}
	constexpr bool _IsReservedLevel3_3_() const {return IsValid() && !IsParent() && level3 == 255;}
	
	void SetParent(uint64_t parentId) {
		this->parentId = parentId;
		this->isParentId = 1;
		assert(IsParent());
	}
	
	void SetCelestial(uint32_t posInGalaxy_x, uint32_t posInGalaxy_y, uint32_t posInGalaxy_z, uint8_t level1, uint8_t level2, uint8_t level3) {
		this->posInGalaxy_x = posInGalaxy_x;
		this->posInGalaxy_y = posInGalaxy_y;
		this->posInGalaxy_z = posInGalaxy_z;
		this->level1 = level1;
		this->level2 = level2;
		this->level3 = level3;
		this->isParentId = 0;
		assert(IsCelestial());
	}
	
	void SetCelestial(uint64_t posInGalaxy, uint8_t level1, uint8_t level2, uint8_t level3) {
		this->posInGalaxy = posInGalaxy;
		this->level1 = level1;
		this->level2 = level2;
		this->level3 = level3;
		this->isParentId = 0;
		assert(IsCelestial());
	}
	void SetCelestial(uint32_t posInGalaxy_x, uint32_t posInGalaxy_y, uint32_t posInGalaxy_z, uint16_t celestialId) {
		this->posInGalaxy_x = posInGalaxy_x;
		this->posInGalaxy_y = posInGalaxy_y;
		this->posInGalaxy_z = posInGalaxy_z;
		this->celestialId = celestialId;
		this->isParentId = 0;
		assert(IsCelestial());
	}
	
	void SetCelestial(uint64_t posInGalaxy, uint16_t celestialId) {
		this->posInGalaxy = posInGalaxy;
		this->celestialId = celestialId;
		this->isParentId = 0;
		assert(IsCelestial());
	}
	
	void SetReferenceFrame(uint32_t posInGalaxy_x, uint32_t posInGalaxy_y, uint32_t posInGalaxy_z) {
		this->posInGalaxy_x = posInGalaxy_x;
		this->posInGalaxy_y = posInGalaxy_y;
		this->posInGalaxy_z = posInGalaxy_z;
		this->celestialId = 0;
		this->isParentId = 0;
		assert(IsReferenceFrame());
	}
	
	void SetReferenceFrame(uint64_t posInGalaxy) {
		this->posInGalaxy = posInGalaxy;
		this->celestialId = 0;
		this->isParentId = 0;
		assert(IsReferenceFrame());
	}
	
};

struct BeltObjectPosition {
	union {
		uint64_t rawValue;
		struct {
			uint64_t arc : 32;
			uint64_t radius : 31;
			uint64_t : 1;
		};
	};
	BeltObjectPosition(uint64_t rawValue) : rawValue(rawValue) {static_assert(sizeof(BeltObjectPosition) == 8);}
};
