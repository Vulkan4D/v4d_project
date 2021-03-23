#pragma once
#include "PlanetRenderer/PlanetTerrain.h"

class V4DLIB TerrainGenerator {
	V4D_MODULE_CLASS_HEADER(TerrainGenerator
		,Init
		,GetHeightMap
		,GetColor
	)
	V4D_MODULE_FUNC_DECLARE(void, Init)
	V4D_MODULE_FUNC_DECLARE(double, GetHeightMap, glm::dvec3* const pos, double solidRadius, double heightVariation)
	V4D_MODULE_FUNC_DECLARE(glm::vec3, GetColor, double heightMap)
};

struct V4DLIB TerrainGeneratorLib {
	static bool running;
	static TerrainGenerator* generatorLib;
	static std::thread* autoReloaderThread;
	static std::mutex mu;
	
	static void Unload();
	static void Load();
	static void Start();
	static void Stop();
};
