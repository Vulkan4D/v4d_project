
// Conversions (light-year, Astronomical unit, kilometer, meter)
#define AU2LY(n)	(n * 1.5812056939013E-5)
#define LY2AU(n)	(n * 63242.878763782)
#define AU2KM(n)	(n * 149597870.7)
#define KM2AU(n)	(n * 6.6845871222684E-9)
#define AU2M(n)		(n * 149597870700)
#define M2AU(n)		(n * 6.6845871222684E-12)
#define LY2KM(n)	(n * 9461000000000)
#define KM2LY(n)	(n * 1.056970721911E-13)
#define LY2M(n)		(n * 9.461E+15)
#define M2LY(n)		(n * 1.056970721911E-16)

struct GalacticPosition {
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
			uint64_t level1 : 2; // 3 stars
			uint64_t level2 : 2; // 3 stars/planets
			uint64_t level3 : 5; // 31 stars/planets
			uint64_t level4 : 6; // 63 planets/moons
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
	
	bool IsValid() const {return value != 0 && (!isParentId || parentId > 0);}
	bool IsParent() const {return IsValid() && isParentId;}
	bool IsReferenceFrame() const {return IsValid() && !IsParent() && celestialId == 0;}
	bool IsCelestial() const {return IsValid() && !IsParent() && celestialId > 0;}
	bool IsAsteroid() const {return IsCelestial() && level4 == 63;}
	
	void SetParent(uint64_t parentId) {
		this->parentId = parentId;
		this->isParentId = 1;
		assert(IsParent());
	}
	
	void SetCelestial(uint32_t posInGalaxy_x, uint32_t posInGalaxy_y, uint32_t posInGalaxy_z, uint32_t level1, uint32_t level2, uint32_t level3, uint32_t level4) {
		this->posInGalaxy_x = posInGalaxy_x;
		this->posInGalaxy_y = posInGalaxy_y;
		this->posInGalaxy_z = posInGalaxy_z;
		this->level1 = level1;
		this->level2 = level2;
		this->level3 = level3;
		this->level4 = level4;
		this->isParentId = 0;
		assert(IsCelestial());
	}
	
	void SetCelestial(uint64_t posInGalaxy, uint32_t level1, uint32_t level2, uint32_t level3, uint32_t level4) {
		this->posInGalaxy = posInGalaxy;
		this->level1 = level1;
		this->level2 = level2;
		this->level3 = level3;
		this->level4 = level4;
		this->isParentId = 0;
		assert(IsCelestial());
	}
	
	void SetCelestial(uint32_t posInGalaxy_x, uint32_t posInGalaxy_y, uint32_t posInGalaxy_z, uint32_t celestialId) {
		this->posInGalaxy_x = posInGalaxy_x;
		this->posInGalaxy_y = posInGalaxy_y;
		this->posInGalaxy_z = posInGalaxy_z;
		this->celestialId = celestialId;
		this->isParentId = 0;
		assert(IsCelestial());
	}
	
	void SetCelestial(uint64_t posInGalaxy, uint32_t celestialId) {
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

struct AsteroidPosition {
	union {
		uint64_t rawValue;
		struct {
			uint64_t arc : 32;
			uint64_t radius : 31;
			uint64_t : 1;
		};
	};
	AsteroidPosition(uint64_t rawValue) : rawValue(rawValue) {static_assert(sizeof(AsteroidPosition) == 8);}
};
