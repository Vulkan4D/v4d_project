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
	
	planetsRenderer->planetTerrains.emplace_back(new PlanetTerrain{
		24600000,
		23950000,
		10000,
		// {0, 15806000, -18000000}
		{0, 0, 0}
	});
	
}

V4DMODULE void V4D_ModuleDestroy() {
	
	// Planets Renderer
	for (auto* planetTerrain : planetsRenderer->planetTerrains) {
		delete planetTerrain;
	}
	planetsRenderer->planetTerrains.clear();
	V4D_UNLOAD_SUBMODULES(PlanetsRenderer)
	
}
