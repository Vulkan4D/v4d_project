#include "TerrainGeneratorLib.h"

V4D_MODULE_CLASS_CPP(TerrainGenerator)

bool TerrainGeneratorLib::running = false;
TerrainGenerator* TerrainGeneratorLib::generatorLib = nullptr;
std::thread* TerrainGeneratorLib::autoReloaderThread = nullptr;
std::mutex TerrainGeneratorLib::mu;

void TerrainGeneratorLib::Unload() {
	PlanetTerrain::EndChunkGenerator();
	PlanetTerrain::generatorFunction = nullptr;
	PlanetTerrain::generateColor = nullptr;
	TerrainGenerator::UnloadModule(THIS_MODULE);
}
void TerrainGeneratorLib::Load() {
	std::lock_guard generatorLock(mu);
	if (generatorLib) {
		Unload();
	}
	generatorLib = TerrainGenerator::LoadModule(THIS_MODULE);
	if (!generatorLib) {
		LOG_ERROR("Error loading terrain generator submodule")
		return;
	}
	if (generatorLib->Init) {
		generatorLib->Init();
	} else {
		LOG_ERROR("Error loading 'Init' function pointer from terrain generator submodule")
		return;
	}
	if (!generatorLib->GetHeightMap) {
		LOG_ERROR("Error loading 'GetHeightMap' function pointer from terrain generator submodule")
		return;
	}
	if (!generatorLib->GetColor) {
		LOG_ERROR("Error loading 'GetHeightMap' function pointer from terrain generator submodule")
		return;
	}
	PlanetTerrain::generatorFunction = generatorLib->GetHeightMap;
	PlanetTerrain::generateColor = generatorLib->GetColor;
	// for each planet
		for (auto&[id,terrain] : PlanetTerrain::terrains) if (terrain) {
			std::scoped_lock lock(terrain->planetMutex);
			// #ifdef _DEBUG
				terrain->totalChunkTimeNb = 0;
				terrain->totalChunkTime = 0;
			// #endif
			terrain->RemoveBaseChunks();
			terrain->AddBaseChunks();
		}
	//
	LOG_SUCCESS("terrain generator submodule Loaded")
	PlanetTerrain::StartChunkGenerator();
}
void TerrainGeneratorLib::Start() {
	if (running) return;
	Load();
	running = true;
	autoReloaderThread = new std::thread{[&]{
		LOG("Started autoReload thread for terrain generator submodule...")
		while (running) {
			if (v4d::io::FilePath::FileExists(generatorLib->ModuleLibraryFilePath()+".new")) {
				LOG_WARN("terrain generator submodule has been modified. Reloading it...")
				Load();
			}
			SLEEP(100ms)
		}
	}};
}
void TerrainGeneratorLib::Stop() {
	if (!running) return;
	running = false;
	if (autoReloaderThread && autoReloaderThread->joinable()) {
		autoReloaderThread->join();
	}
	delete autoReloaderThread;
	autoReloaderThread = nullptr;
	Unload();
}
