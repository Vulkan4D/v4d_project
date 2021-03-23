#include "PlanetTerrain.h"

Device* PlanetTerrain::renderingDevice = nullptr;

// Buffer pools
v4d::Timer PlanetTerrain::lastGarbageCollectionTime {true};

// Chunk Generator
std::vector<PlanetTerrain::Chunk*> PlanetTerrain::chunkGeneratorQueue {};
std::vector<std::thread> PlanetTerrain::chunkGeneratorThreads {};
std::mutex PlanetTerrain::chunkGeneratorQueueMutex {};
std::condition_variable PlanetTerrain::chunkGeneratorEventVar {};
std::atomic<bool> PlanetTerrain::chunkGeneratorActive = false;
double (*PlanetTerrain::generatorFunction)(glm::dvec3* const, double solidRadius, double heightVariation) = nullptr;
glm::vec3 (*PlanetTerrain::generateColor)(double heightMap) = nullptr;
bool PlanetTerrain::generateAabbChunks = false;
float PlanetTerrain::targetVertexSeparationInMeters = 0.50; // 0.02 - 0.50

std::unordered_map<uint64_t, PlanetTerrain*> PlanetTerrain::terrains {};
