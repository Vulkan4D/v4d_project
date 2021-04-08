#pragma once
#include "PlanetRenderer/PlanetTerrain.h"

class TerrainGenerator {
	V4D_MODULE_CLASS_HEADER(TerrainGenerator
		,Init
		,GetHeightMap
		,GetColor
	)
	V4D_MODULE_FUNC_DECLARE(void, Init)
	V4D_MODULE_FUNC_DECLARE(double, GetHeightMap, TERRAIN_GENERATOR_LIB_HEIGHTMAP_ARGS)
	V4D_MODULE_FUNC_DECLARE(glm::vec3, GetColor, double heightMap)
};

struct TerrainGeneratorLib {
	static bool running;
	static TerrainGenerator* generatorLib;
	static std::thread* autoReloaderThread;
	static std::mutex mu;
	
	static void Unload();
	static void Load();
	static void Start();
	static void Stop();
};
