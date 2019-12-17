#pragma once

#include <v4d.h>

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

struct GalaxyID {
	union {
		long id = 0;
		struct {
			// Fixed Universe Grid Size of 10MLY (-32,768 to +32,767)
			int gridX : 16;
			int gridY : 16;
			int gridZ : 16;
			// Based on UniverseGridSizeAt(gridX, gridY, gridZ)
			// 	(0) or (-2,-1,+0,+1) or (-16 to +15)
			int subGridX : 5;
			int subGridY : 5;
			int subGridZ : 5;
			//
			int _unusedBit : 1;
		} position;
	};
};

struct CelestialSystemID {
	union {
		long id = 0;
		struct {
			// Based on galaxy.GetStarClusterGridSize()
			// 	(0) or (-128 to +127) or (-512 to +511) or (-2048 to +2047)
			int starClusterX : 12;
			int starClusterY : 12;
			int starClusterZ : 12;
			// Based on galaxy.GetCelestialSystemGridSizeAtStarCluster(starClusterX, starClusterY, starClusterZ)
			// 	(0) or (-8 to +7) or (-32 to +31) or (-256 to +255)
			int gridX : 9;
			int gridY : 9;
			int gridZ : 9;
			//
			int _unusedBit : 1;
		} position;
	};
};

struct CelestialID {
	union {
		long id = 0;
		struct {
			ulong level1 : 4; // 0-15 (0 = celestial at center of celestial system)
			ulong level2 : 8; // 1-255 or 0 = current is previous level
			ulong level3 : 8; // 1-255 or 0 = current is previous level
			ulong level4 : 43; // 1 to 8 trillion or 0 = current is previous level
			int _unusedBit : 1;
		} index;
	};
};

struct UniversalPosition {
	enum : byte {
		INVALID = 0,
		UNIVERSE_GRID_AU = 1,
		RELATIVE_TO_CELESTIAL = 2,
	} type;
	union {
		long universeX = 0;
		GalaxyID galaxyID;
	};
	union {
		long universeY = 0;
		CelestialSystemID systemID;
	};
	union {
		long universeZ = 0;
		CelestialID celestialID;
	};
	
	UniversalPosition(byte t = INVALID)
	 : type(t) {}
	UniversalPosition(long x, long y, long z, byte t = UNIVERSE_GRID_AU)
	 : type(t), universeX(x), universeY(y), universeZ(z) {}
	UniversalPosition(GalaxyID galaxy, CelestialSystemID celestialSystem = 0, CelestialID celestial = 0)
	 : type(RELATIVE_TO_CELESTIAL), galaxyID(galaxy.id), celestialSystemID(celestialSystem.id), celestialID(celestial.id) {}
};
