// Metadata
#define _V4D_MODULE
#define THIS_MODULE_VENDOR_ID 16
#define THIS_MODULE_ID 1002
#define THIS_MODULE_REV 1
#define THIS_MODULE_NAME "test_planets_rtx"
#define THIS_MODULE_DESCRIPTION "test_planets_rtx"

// V4D Core Header
#include <v4d.h>

// // GLI
// #include <gli/gli.hpp>

#include "PlanetsRenderer/PlanetTerrain.hpp"
#include "PlanetsRenderer/PlanetAtmosphereShaderPipeline.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan::rtx;

#pragma region Graphics

PipelineLayout planetsMapGenLayout;

ComputeShaderPipeline 
	bumpMapsAltitudeGen{planetsMapGenLayout, "modules/test_planets_rtx/assets/shaders/planets.bump.altitude.map.comp"},
	bumpMapsNormalsGen{planetsMapGenLayout, "modules/test_planets_rtx/assets/shaders/planets.bump.normals.map.comp"}
	// ,mantleMapGen{planetsMapGenLayout, "modules/test_planets_rtx/assets/shaders/planets.mantle.map.comp"}
	// ,tectonicsMapGen{planetsMapGenLayout, "modules/test_planets_rtx/assets/shaders/planets.tectonics.map.comp"}
	// ,heightMapGen{planetsMapGenLayout, "modules/test_planets_rtx/assets/shaders/planets.height.map.comp"}
	// ,volcanoesMapGen{planetsMapGenLayout, "modules/test_planets_rtx/assets/shaders/planets.volcanoes.map.comp"}
	// ,liquidsMapGen{planetsMapGenLayout, "modules/test_planets_rtx/assets/shaders/planets.liquids.map.comp"}
;

PlanetAtmosphereShaderPipeline* planetAtmosphereShader = nullptr;

struct MapGenPushConstant {
	int planetIndex;
	float planetHeightVariation;
} mapGenPushConstant;

struct TerrainChunkPushConstant {
	int planetIndex;
	int chunkGeometryIndex;
	float solidRadius;
	int vertexSubdivisionsPerChunk;
	glm::vec2 uvMult;
	glm::vec2 uvOffset;
	alignas(16) glm::ivec3 chunkPosition;
	alignas(4) int face;
} terrainChunkPushConstant;

DescriptorSet mapsGenDescriptorSet, mapsSamplerDescriptorSet /*, planetsDescriptorSet*/;

#define MAX_PLANETS 1

// Image mantleMaps[MAX_PLANETS];
// Image tectonicsMaps[MAX_PLANETS];
// Image heightMaps[MAX_PLANETS];
// Image volcanoesMaps[MAX_PLANETS];
// Image liquidsMaps[MAX_PLANETS];

Image bumpMaps[1] {// xyz=normal, a=altitude
	Image { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 1, 1, {VK_FORMAT_R32G32B32A32_SFLOAT}},
	// Image { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 1, 1, {VK_FORMAT_R32G32B32A32_SFLOAT}},
};
bool bumpMapsGenerated = false;

// struct PlanetBuffer {
// 	// glm::dmat4 viewToPlanetPosMatrix {1};
// 	alignas(16) glm::vec3 northDir;
// };
// StagedBuffer planetsBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(PlanetBuffer)*MAX_PLANETS};

#pragma endregion

struct Planet {
	double solidRadius = 8000000;
	double atmosphereRadius = 8200000;
	double heightVariation = 10000;
	
	#pragma region cache
	
	bool mapsGenerated = false;
	
	#pragma endregion
	
	#pragma region Maps
	
	// CubeMapImage mantleMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}}; // platesDir, mantleHeightFactor, surfaceHeightFactor, hotSpots
	// CubeMapImage tectonicsMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}}; // divergent, convergent, transform, density
	// CubeMapImage heightMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R8_SNORM}}; // variation mapped as [-1.0 to 1.0] ==> [-12.0 to 12.0] km
	// CubeMapImage volcanoesMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R8_UNORM}}; // volcanoesMap
	// CubeMapImage liquidsMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R8_UNORM}}; // liquidMap
	
	// gli::texture_cube heightMapTexture;
	
	// // temperature k, radiation rad, moisture norm, wind m/s
	// CubeMapImage weatherMapAvg { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	// CubeMapImage weatherMapMinimum { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	// CubeMapImage weatherMapMaximum { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	// CubeMapImage weatherMapCurrent { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	
	// void CreateMaps(Device* device, double scale = 1.0) {
	// 	int mapSize = std::max(64, int(scale * std::min(8000000.0, solidRadius) / 2000.0 * 3.141592654)); // 1 km / pixel (considering maximum radius of 8000km)
	// // 	// max image width : 12566
	// // 	mantleMap.Create(device, mapSize/16); // max 785 px = 57 MB
	// // 	tectonicsMap.Create(device, mapSize/8); // max 1570 px = 226 MB
	// 	heightMap.Create(device, mapSize); // max 12566 px = 904 MB
	// // 	volcanoesMap.Create(device, mapSize/4); // max 3141 px = 57 MB
	// // 	liquidsMap.Create(device, mapSize/4); // max 3141 px = 57 MB
	// // 	int weatherMapSize = std::max(8, int(mapSize / 100)); // max 125 px = 1.5 MB x4
	// // 	weatherMapAvg.Create(device, weatherMapSize);
	// // 	weatherMapMinimum.Create(device, weatherMapSize);
	// // 	weatherMapMaximum.Create(device, weatherMapSize);
	// // 	weatherMapCurrent.Create(device, weatherMapSize);
	// }
	
	// void DestroyMaps(Device* device) {
	// // 	mantleMap.Destroy(device);
	// // 	tectonicsMap.Destroy(device);
	// 	heightMap.Destroy(device);
	// // 	volcanoesMap.Destroy(device);
	// // 	liquidsMap.Destroy(device);
	// // 	weatherMapAvg.Destroy(device);
	// // 	weatherMapMinimum.Destroy(device);
	// // 	weatherMapMaximum.Destroy(device);
	// // 	weatherMapCurrent.Destroy(device);
		
	// 	mapsGenerated = false;
	// }
	
	// void GenerateMaps(Device* device, VkCommandBuffer commandBuffer) {
	// 	if (!mapsGenerated) {
	// 		mapGenPushConstant.planetIndex = 0;
	// 		mapGenPushConstant.planetHeightVariation = (float)heightVariation;
			
	// // 		/*First Pass*/
		
	// // 		mantleMapGen.SetGroupCounts(mantleMap.width, mantleMap.height, mantleMap.arrayLayers);
	// // 		mantleMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
			
	// // 		tectonicsMapGen.SetGroupCounts(tectonicsMap.width, tectonicsMap.height, tectonicsMap.arrayLayers);
	// // 		tectonicsMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
			
	// 		heightMapGen.SetGroupCounts(heightMap.width, heightMap.height, heightMap.arrayLayers);
	// 		heightMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
			
	// // 		volcanoesMapGen.SetGroupCounts(volcanoesMap.width, volcanoesMap.height, volcanoesMap.arrayLayers);
	// // 		volcanoesMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
			
	// // 		liquidsMapGen.SetGroupCounts(liquidsMap.width, liquidsMap.height, liquidsMap.arrayLayers);
	// // 		liquidsMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
			
	// // 		// Need pipeline barrier before other passes
			
			
	// 		mapsGenerated = true;
		
	// // 		VkMemoryBarrier barrier {};
	// // 			barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	// // 			barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	// // 			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	// // 		device->CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
	// 	} else {
	// 		// heightMapTexture = gli::make_texture_cube()
	// 	}
	// }
	
	#pragma endregion
	
} planet;

PlanetTerrain* terrain = nullptr;

// PipelineLayout terrainVertexComputeLayout;
// ComputeShaderPipeline terrainVertexPosCompute {terrainVertexComputeLayout, "modules/test_planets_rtx/assets/shaders/planets.terrain.vertexpos.comp"};
// ComputeShaderPipeline terrainVertexNormalCompute {terrainVertexComputeLayout, "modules/test_planets_rtx/assets/shaders/planets.terrain.vertexnormal.comp"};

void ComputeChunkVertices(Device* device, VkCommandBuffer commandBuffer, PlanetTerrain::Chunk* chunk, int& chunksToGeneratePerFrame) {
	if (chunk->active) {
		
		// subChunks
		{
			std::scoped_lock lock(chunk->subChunksMutex);
			for (auto* subChunk : chunk->subChunks) {
				ComputeChunkVertices(device, commandBuffer, subChunk, chunksToGeneratePerFrame);
			}
		}
		
		// std::scoped_lock lock(chunk->stateMutex);
		
		if (chunk->obj && chunk->meshGenerated) {
			switch (chunk->computedLevel) {
				case 0: {
				// 	if (chunksToGeneratePerFrame--<0) return;
					
				// 	// chunk->obj->Lock();
						chunk->obj->SetGeometriesDirty();
						chunk->obj->PushGeometries(device, commandBuffer);
				// // 	chunk->obj->Unlock();
					
				// 	#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
				// 		VkBufferMemoryBarrier barrier1 {};
				// 			barrier1.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				// 			barrier1.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
				// 			barrier1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
				// 			barrier1.offset = chunk->obj->GetFirstGeometryVertexOffset() * sizeof(Geometry::VertexBuffer_T);
				// 			barrier1.size = PlanetTerrain::nbVerticesPerChunk * sizeof(Geometry::VertexBuffer_T);
				// 			barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				// 			barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				// 			barrier1.buffer = Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer.buffer;
				// 		device->CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrier1, 0, nullptr);
				// 	#endif
					
				// 	terrainChunkPushConstant.chunkGeometryIndex = chunk->geometry->geometryOffset;
				// 	terrainChunkPushConstant.chunkPosition = chunk->centerPos;
				// 	terrainChunkPushConstant.face = chunk->face;
				// 	terrainChunkPushConstant.uvMult = {chunk->uvMult, chunk->uvMult};
				// 	terrainChunkPushConstant.uvOffset = {chunk->uvOffsetX, chunk->uvOffsetY};
					
				// 	// Compute positions
				// 	terrainVertexPosCompute.SetGroupCounts(PlanetTerrain::vertexSubdivisionsPerChunk+1, PlanetTerrain::vertexSubdivisionsPerChunk+1, 1);
				// 	terrainVertexPosCompute.Execute(device, commandBuffer, 1, &terrainChunkPushConstant);
					
				// 	VkBufferMemoryBarrier barrier2 {};
				// 		barrier2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				// 		barrier2.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				// 		barrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
				// 		barrier2.offset = chunk->obj->GetFirstGeometryVertexOffset() * sizeof(Geometry::VertexBuffer_T);
				// 		barrier2.size = PlanetTerrain::nbVerticesPerChunk * sizeof(Geometry::VertexBuffer_T);
				// 		barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				// 		barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				// 		#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
				// 			barrier2.buffer = Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer.buffer;
				// 		#else
				// 			barrier2.buffer = Geometry::globalBuffers.vertexBuffer.buffer;
				// 		#endif
				// 	device->CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &barrier2, 0, nullptr);
					
				// 	// Compute normals
				// 	terrainVertexNormalCompute.SetGroupCounts(PlanetTerrain::vertexSubdivisionsPerChunk+1, PlanetTerrain::vertexSubdivisionsPerChunk+1, 1);
				// 	terrainVertexNormalCompute.Execute(device, commandBuffer, 1, &terrainChunkPushConstant);
					
				// 	// pull computed vertices
				// 	#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
				// 		chunk->obj->GetGeometries()[0].geometry->Pull(device, commandBuffer, Geometry::GlobalGeometryBuffers::BUFFER_VERTEX);
				// 	#endif
					
				// 	chunk->computedLevel = 1;
				// }break;
				// case 1: {
				// 	chunk->obj->Lock();
						chunk->computedLevel = 2;
						chunk->obj->SetGenerated();
						// chunk->RefreshVertices();
						if (chunk->geometry->blas) {
							chunk->geometry->blas->built = false;
							LOG_WARN("BLAS already created but should not be...")
						}
						chunk->geometry->active = true;
					// chunk->obj->Unlock();
				}break;
			}
		}
	}
}

namespace TerrainGeneratorLib {
	bool running = false;
	std::string libPath {"/home/olivier/projects/v4d_project/dynamicLibs/terrainGenerator/build/terrainGenerator"};
	v4d::io::SharedLibraryLoader libLoader;
	v4d::io::SharedLibraryInstance* generatorLib = nullptr;
	std::thread* autoReloaderThread = nullptr;
	
	double lastModifiedTime = 0;
	void Load() {
		generatorLib = libLoader.Load("terrainGenerator", libPath);
		if (!generatorLib) {
			LOG_ERROR("Error loading dynamic library 'terrainGenerator' from " << libPath)
			return;
		}
		LOAD_DLL_FUNC(generatorLib, void, init)
		if (init) {
			init();
		} else {
			LOG_ERROR("Error loading function pointer 'init' from dynamic library 'terrainGenerator'")
			return;
		}
		LOAD_DLL_FUNC(generatorLib, double, generateHeightMap, glm::dvec3* const)
		if (!generatorLib) {
			LOG_ERROR("Error loading function pointer 'generateHeightMap' from dynamic library 'terrainGenerator'")
			return;
		}
		PlanetTerrain::generatorFunction = generateHeightMap;
		// for each planet
			if (terrain) {
				std::scoped_lock lock(terrain->planetMutex);
				terrain->RemoveBaseChunks();
				terrain->AddBaseChunks();
			}
		//
		LOG_SUCCESS("Dynamic library 'terrainGenerator' Loaded")
		// PlanetTerrain::StartChunkGenerator();
	}
	void Unload() {
		// PlanetTerrain::EndChunkGenerator();
		PlanetTerrain::generatorFunction = nullptr;
		libLoader.Unload("terrainGenerator");
	}
	void Start() {
		if (running) return;
		Load();
		running = true;
		autoReloaderThread = new std::thread{[&]{
			LOG("Started autoReload dynamic library thread...")
			while (running) {
				auto t = v4d::io::FilePath(generatorLib->path).GetLastWriteTime();
				if (lastModifiedTime == 0) {
					lastModifiedTime = t;
				} else if (t > lastModifiedTime) {
					LOG_WARN("Dynamic library 'terrainGenerator' has been modified. Reloading it...")
					lastModifiedTime = 0;
					Unload();
					SLEEP(100ms)
					Load();
				}
				SLEEP(500ms)
			}
		}};
	}
	void Stop() {
		if (!running) return;
		running = false;
		if (autoReloaderThread && autoReloaderThread->joinable()) {
			autoReloaderThread->join();
		}
		delete autoReloaderThread;
		autoReloaderThread = nullptr;
		Unload();
	}
};

class Rendering : public v4d::modules::Rendering {
public:
	Rendering() {}
	int OrderIndex() const override {return 0;}
	
	// Init/Shaders
	void Init() override {
		
	}
	void InitDeviceFeatures() {
		renderer->deviceFeatures.shaderStorageImageWriteWithoutFormat = VK_TRUE;
	}
	void InitLayouts(std::map<std::string, DescriptorSet*>& descriptorSets, std::unordered_map<std::string, Image*>&, std::unordered_map<std::string, PipelineLayout*>& pipelineLayouts) override {
		
		descriptorSets["mapsGen"] = &mapsGenDescriptorSet;
		// descriptorSets["planets"] = &planetsDescriptorSet;
		descriptorSets["mapsSampler"] = &mapsSamplerDescriptorSet;
		planetsMapGenLayout.AddDescriptorSet(descriptorSets["0_base"]);
		planetsMapGenLayout.AddDescriptorSet(&mapsGenDescriptorSet);
		planetsMapGenLayout.AddPushConstant<MapGenPushConstant>(VK_SHADER_STAGE_COMPUTE_BIT);
		pipelineLayouts["rayTracing"]->AddDescriptorSet(&mapsSamplerDescriptorSet);
		// pipelineLayouts["rayTracing"]->AddDescriptorSet(&planetsDescriptorSet);
		
		mapsGenDescriptorSet.AddBinding_imageView_array(0, bumpMaps, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		// mapsGenDescriptorSet.AddBinding_imageView_array(1, mantleMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		// mapsGenDescriptorSet.AddBinding_imageView_array(2, tectonicsMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		// mapsGenDescriptorSet.AddBinding_imageView_array(3, heightMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		// mapsGenDescriptorSet.AddBinding_imageView_array(4, volcanoesMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		// mapsGenDescriptorSet.AddBinding_imageView_array(5, liquidsMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		
		mapsSamplerDescriptorSet.AddBinding_combinedImageSampler_array(0, bumpMaps, 1, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		// mapsSamplerDescriptorSet.AddBinding_combinedImageSampler_array(1, mantleMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
		// mapsSamplerDescriptorSet.AddBinding_combinedImageSampler_array(2, tectonicsMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
		// mapsSamplerDescriptorSet.AddBinding_combinedImageSampler_array(3, heightMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
		// mapsSamplerDescriptorSet.AddBinding_combinedImageSampler_array(4, volcanoesMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
		// mapsSamplerDescriptorSet.AddBinding_combinedImageSampler_array(5, liquidsMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
		
		// planetsDescriptorSet.AddBinding_storageBuffer(0, &planetsBuffer.deviceLocalBuffer, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
		
		// terrainVertexComputeLayout.AddDescriptorSet(descriptorSets["0_base"]);
		// terrainVertexComputeLayout.AddDescriptorSet(descriptorSets["1_rayTracing"]);
		// terrainVertexComputeLayout.AddDescriptorSet(&mapsSamplerDescriptorSet);
		// // terrainVertexComputeLayout.AddDescriptorSet(planetsDescriptorSet);
		// terrainVertexComputeLayout.AddPushConstant<TerrainChunkPushConstant>(VK_SHADER_STAGE_COMPUTE_BIT);
		
		planetAtmosphereShader = new PlanetAtmosphereShaderPipeline {*pipelineLayouts["lighting"], {
			"modules/test_planets_rtx/assets/shaders/planetAtmosphere.vert",
			"modules/test_planets_rtx/assets/shaders/planetAtmosphere.frag",
		}};
		planetAtmosphereShader->atmospherePushConstantIndex = pipelineLayouts["lighting"]->AddPushConstant<PlanetAtmosphereShaderPipeline::PlanetAtmospherePushConstant>(VK_SHADER_STAGE_ALL_GRAPHICS);
	}
	void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders, ShaderBindingTable* shaderBindingTable, std::unordered_map<std::string, PipelineLayout*>& pipelineLayouts) override {
		// Geometry::rayTracingShaderOffsets["planet_raymarching"] = shaderBindingTable->AddHitShader("modules/test_planets_rtx/assets/shaders/planets.raymarching.rchit", "", "modules/test_planets_rtx/assets/shaders/planets.raymarching.rint");
		Geometry::rayTracingShaderOffsets["planet_terrain"] = shaderBindingTable->AddHitShader("modules/test_planets_rtx/assets/shaders/planets.terrain.rchit");
		
		// Atmosphere
		planetAtmosphereShader->AddVertexInputBinding(sizeof(PlanetAtmosphere::Vertex), VK_VERTEX_INPUT_RATE_VERTEX, PlanetAtmosphere::Vertex::GetInputAttributes());
		planetAtmosphereShader->depthStencilState.depthTestEnable = VK_FALSE;
		planetAtmosphereShader->depthStencilState.depthWriteEnable = VK_FALSE;
		planetAtmosphereShader->rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
		#ifdef PLANETARY_ATMOSPHERE_MESH_USE_TRIANGLE_STRIPS
			planetAtmosphereShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			planetAtmosphereShader->inputAssembly.primitiveRestartEnable = VK_TRUE;
		#else
			planetAtmosphereShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		#endif
		shaders["lighting_1"].push_back(planetAtmosphereShader); //TODO this line should be called before ConfigureShaders() ?
		
	}
	void ReadShaders() override {
		bumpMapsAltitudeGen.ReadShaders();
		bumpMapsNormalsGen.ReadShaders();
		
		// mantleMapGen.ReadShaders();
		// tectonicsMapGen.ReadShaders();
		// heightMapGen.ReadShaders();
		// volcanoesMapGen.ReadShaders();
		// liquidsMapGen.ReadShaders();
		
		// terrainVertexPosCompute.ReadShaders();
		// terrainVertexNormalCompute.ReadShaders();
	}
	
	// Images / Buffers / Pipelines
	void CreateResources() override {
		// Bump Maps
		bumpMaps[0].samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].Create(renderingDevice, 4096);
		renderer->TransitionImageLayout(bumpMaps[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		
		// bumpMaps[1].Create(renderingDevice, 4096);
		// renderer->TransitionImageLayout(bumpMaps[1], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		
		// planet.CreateMaps(renderingDevice);
		
		// renderer->TransitionImageLayout(planet.mantleMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		// renderer->TransitionImageLayout(planet.tectonicsMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		// renderer->TransitionImageLayout(planet.heightMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		// renderer->TransitionImageLayout(planet.volcanoesMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		// renderer->TransitionImageLayout(planet.liquidsMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		
		// // Set Maps
		// mantleMaps[0] = planet.mantleMap;
		// tectonicsMaps[0] = planet.tectonicsMap;
		// heightMaps[0] = planet.heightMap;
		// volcanoesMaps[0] = planet.volcanoesMap;
		// liquidsMaps[0] = planet.liquidsMap;
		
		// Chunk Generator
		PlanetTerrain::StartChunkGenerator();
	}
	void DestroyResources() override {
		// Chunk Generator
		PlanetTerrain::EndChunkGenerator();
		
		// planet.DestroyMaps(renderingDevice);
		bumpMaps[0].Destroy(renderingDevice);
		// bumpMaps[1].Destroy(renderingDevice);
		bumpMapsGenerated = false;
		
		if (terrain) {
			terrain->atmosphere.Free(renderingDevice);
			delete terrain;
			terrain = nullptr;
			planetAtmosphereShader->planets.clear();
			planetAtmosphereShader->lightSources.clear();
		}
	}
	void AllocateBuffers() override {
		// planetsBuffer.Allocate(renderingDevice);
	}
	void FreeBuffers() override {
		// planetsBuffer.Free(renderingDevice);
	}
	void CreatePipelines(std::unordered_map<std::string, Image*>&) override {
		planetsMapGenLayout.Create(renderingDevice);
		// terrainVertexComputeLayout.Create(renderingDevice);
		
		bumpMapsAltitudeGen.CreatePipeline(renderingDevice);
		bumpMapsNormalsGen.CreatePipeline(renderingDevice);
		
		// mantleMapGen.CreatePipeline(renderingDevice);
		// tectonicsMapGen.CreatePipeline(renderingDevice);
		// heightMapGen.CreatePipeline(renderingDevice);
		// volcanoesMapGen.CreatePipeline(renderingDevice);
		// liquidsMapGen.CreatePipeline(renderingDevice);
		
		// terrainVertexPosCompute.CreatePipeline(renderingDevice);
		// terrainVertexNormalCompute.CreatePipeline(renderingDevice);
	}
	void DestroyPipelines() override {
		bumpMapsAltitudeGen.DestroyPipeline(renderingDevice);
		bumpMapsNormalsGen.DestroyPipeline(renderingDevice);
		
		// mantleMapGen.DestroyPipeline(renderingDevice);
		// tectonicsMapGen.DestroyPipeline(renderingDevice);
		// heightMapGen.DestroyPipeline(renderingDevice);
		// volcanoesMapGen.DestroyPipeline(renderingDevice);
		// liquidsMapGen.DestroyPipeline(renderingDevice);
		
		// terrainVertexPosCompute.DestroyPipeline(renderingDevice);
		// terrainVertexNormalCompute.DestroyPipeline(renderingDevice);

		// terrainVertexComputeLayout.Destroy(renderingDevice);
		planetsMapGenLayout.Destroy(renderingDevice);
	}
	
	ObjectInstance* sun = nullptr;
	
	// Scene
	void LoadScene(Scene& scene) override {
		TerrainGeneratorLib::Start();
		
		// Light source
		sun = scene.AddObjectInstance();
		sun->Configure([](ObjectInstance* obj){
			obj->SetSphereLightSource("light", 700000000, 1e24f);
		}, {10,-1.496e+11,300});
				
						// // Planet
						// scene.objectInstances.emplace_back(new ObjectInstance("planet_raymarching"))->Configure([](ObjectInstance* obj){
						// 	obj->SetSphereGeometry((float)planet.solidRadius, {1,0,0, 1}, 0/*planet index*/);
						// }, {0,planet.solidRadius*2,0});
				
		planetAtmosphereShader->viewMatrix = &scene.camera.viewMatrix;
	}
	
	void UnloadScene(Scene&) override {
		TerrainGeneratorLib::Stop();
	}
	
	// Frame Update
	void FrameUpdate(Scene& scene) override {
		if (!terrain) {
			terrain = new PlanetTerrain {planet.atmosphereRadius, planet.solidRadius, planet.heightVariation, {0,planet.atmosphereRadius*2,0}};
			{
				std::lock_guard lock(terrain->planetMutex);
				terrain->scene = &scene;
				planetAtmosphereShader->planets.push_back(terrain);
				sun->GenerateGeometries();
				planetAtmosphereShader->lightSources.push_back(sun->GetLightSources()[0]);
			}
		}
		
						// // // for each planet
						// 	int planetIndex = 0;
						// 	auto* planetBuffer = &((PlanetBuffer*)(planetsBuffer.stagingBuffer.data))[planetIndex];
						// // 	planetBuffer->viewToPlanetPosMatrix = glm::inverse(scene.camera.viewMatrix * scene.objectInstances[1]->GetWorldTransform());
						// 	planetBuffer->northDir = glm::normalize(glm::transpose(glm::inverse(glm::dmat3(terrain->matrix))) * glm::dvec3(0,1,0));
						// // //
						// auto cmdBuffer = renderer->BeginSingleTimeCommands(*graphicsQueue);
						// planetsBuffer.Update(renderingDevice, cmdBuffer);
						// renderer->EndSingleTimeCommands(*graphicsQueue, cmdBuffer);
				
		
		
		// for each planet
			std::lock_guard lock(terrain->chunksMutex);
			
			// //TODO Planet rotation
			// static v4d::Timer time(true);
			// // terrain->rotationAngle = time.GetElapsedSeconds()/1000000000;
			// // terrain->rotationAngle = time.GetElapsedSeconds()/30;
			// terrain->rotationAngle += 0.0001;
			
			terrain->RefreshMatrix();
			
			// Camera position relative to planet
			terrain->cameraPos = glm::inverse(terrain->matrix) * glm::dvec4(scene.camera.worldPosition, 1);
			if (PlanetTerrain::generatorFunction) {
				terrain->cameraAltitudeAboveTerrain = glm::length(terrain->cameraPos) - terrain->GetHeightMap(glm::normalize(terrain->cameraPos), 0.5);
			}
			
			for (auto* chunk : terrain->chunks) {
				chunk->BeforeRender();
			}
			
		//
	}
	void LowPriorityFrameUpdate() override {
		// for each planet
			if (terrain) {
				std::lock_guard lock(terrain->chunksMutex);
				for (auto* chunk : terrain->chunks) {
					chunk->Process(); // need to process after compute, because we will compute on next lowPriority frame, because otherwise the computed geometries get overridden by the ones in the staging buffer
				}
				// terrain->Optimize();
			}
		//
		// PlanetTerrain::CollectGarbage(renderingDevice);
	}
	
	// Render frame
	void RunDynamicGraphicsTop(VkCommandBuffer commandBuffer, std::unordered_map<std::string, Image*>&) {
		// for each planet
			if (terrain && terrain->atmosphere.radius > 0) {
				terrain->atmosphere.Allocate(renderingDevice, commandBuffer);
			}
		//
	}
	void RunDynamicLowPriorityCompute(VkCommandBuffer commandBuffer) override {
		if (!bumpMapsGenerated) {
			bumpMapsAltitudeGen.SetGroupCounts(bumpMaps[0].width, bumpMaps[0].height, bumpMaps[0].arrayLayers);
			bumpMapsAltitudeGen.Execute(renderingDevice, commandBuffer);
			
			VkImageMemoryBarrier barrier = {};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = bumpMaps[0].image;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = 0;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			renderingDevice->CmdPipelineBarrier(
				commandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
	
			bumpMapsNormalsGen.SetGroupCounts(bumpMaps[0].width, bumpMaps[0].height, bumpMaps[0].arrayLayers);
			bumpMapsNormalsGen.Execute(renderingDevice, commandBuffer);
			bumpMapsGenerated = true;
		}
		// for each planet
			if (terrain) {
				// planet.GenerateMaps(renderingDevice, commandBuffer);
				terrainChunkPushConstant.planetIndex = 0;
				terrainChunkPushConstant.solidRadius = float(terrain->solidRadius);
				terrainChunkPushConstant.vertexSubdivisionsPerChunk = PlanetTerrain::vertexSubdivisionsPerChunk;
				std::lock_guard lock(terrain->chunksMutex);
				int chunksToGeneratePerFrame = 8;
				for (auto* chunk : terrain->chunks) {
					ComputeChunkVertices(renderingDevice, commandBuffer, chunk, chunksToGeneratePerFrame);
				}
			}
		//
	}
	void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {
		
	}

	#ifdef _ENABLE_IMGUI
		void RunImGui() override {
			// for each planet
				ImGui::Separator();
				ImGui::Text("Planet");
				ImGui::Text("Chunk generator queue : %d", (int)PlanetTerrain::chunkGeneratorQueue.size());
				if (terrain) {
					std::lock_guard lock(terrain->planetMutex);
					float altitude = (float)terrain->cameraAltitudeAboveTerrain;
					if (std::abs(altitude) < 1.0) {
						ImGui::Text("Altitude above terrain: %d mm", (int)std::ceil(altitude*1000.0));
					} else if (std::abs(altitude) < 1000.0) {
						ImGui::Text("Altitude above terrain: %d m", (int)std::ceil(altitude));
					} else {
						ImGui::Text("Altitude above terrain: %d km", (int)std::ceil(altitude/1000.0));
					}
					if (terrain->atmosphere.radius > 0) {
						ImGui::Separator();
						ImGui::Text("Atmosphere");
						ImGui::SliderFloat("density", &terrain->atmosphere.densityFactor, 0.0f, 1.0f);
						ImGui::ColorEdit3("color", (float*)&terrain->atmosphere.color);
						ImGui::Separator();
					}
					ImGui::Separator();
					ImGui::Text("Sun");
					static glm::vec3 sunPosition = glm::normalize(glm::dvec3(sun->GetWorldTransform()[3]));
					static double sunDistance = glm::distance(glm::dvec3(sun->GetWorldTransform()[3]), terrain->absolutePosition);
					static float intensity = std::log10(sun->GetLightSources()[0]->intensity);
					ImGui::SliderFloat("Intensity", &intensity, 20, 27);
					sun->GetLightSources()[0]->intensity = std::pow(10.0f, intensity);
					float* pos = (float*)&sunPosition;
					ImGui::SliderFloat3("Position", pos, -1, 1);
					sun->SetWorldTransform(glm::translate(glm::dmat4(1), terrain->absolutePosition + glm::normalize(glm::dvec3(sunPosition)) * sunDistance));
				}
			//
		}
	#endif
};

V4DMODULE void V4D_ModuleCreate() {
	V4D_LOAD_SUBMODULE(Rendering)
}

V4DMODULE void V4D_ModuleDestroy() {
	V4D_UNLOAD_SUBMODULES(Rendering)
	delete planetAtmosphereShader;
}
