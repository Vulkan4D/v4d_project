// Metadata
#define _V4D_MODULE
#define THIS_MODULE_VENDOR_ID 16
#define THIS_MODULE_ID 1
#define THIS_MODULE_REV 1
#define THIS_MODULE_NAME "Galaxy4D"
#define THIS_MODULE_DESCRIPTION "Full-scale space sim"

// V4D Core Header
#include <v4d.h>
#include "PlanetsRenderer/PlanetsRenderer.hpp"

PlanetsRenderer* planetsRenderer = nullptr;

V4DMODULE void V4D_ModuleCreate() {
	
	// Planets Renderer
	planetsRenderer = V4D_LOAD_SUBMODULE(PlanetsRenderer)
	planetsRenderer->planetaryTerrains.emplace_back(new PlanetaryTerrain{
		24000000,
		23900000,
		10000,
		// {0, 21740000, -10000000}
		{0, 28000000, -10000000}
		// {20000000, 20000000, -20000000}
	});
	planetsRenderer->planetaryTerrains.emplace_back(new PlanetaryTerrain{
		2400000,
		2390000,
		1000,
		{0, -50000000, -20000000},
	})->lightIntensity = 100;
	
}

V4DMODULE void V4D_ModuleDestroy() {
	
	// Planets Renderer
	for (auto* planetaryTerrain : planetsRenderer->planetaryTerrains) {
		delete planetaryTerrain;
	}
	planetsRenderer->planetaryTerrains.clear();
	V4D_UNLOAD_SUBMODULES(PlanetsRenderer)
	
}
