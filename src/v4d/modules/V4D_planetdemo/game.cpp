#include "common.hh"

using namespace v4d::graphics;

#pragma region Planet

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

#pragma endregion

#pragma region Terrain generator

PlanetTerrain* terrain = nullptr;

// PipelineLayout terrainVertexComputeLayout;
// ComputeShaderPipeline terrainVertexPosCompute {terrainVertexComputeLayout, "modules/test_planets_rtx/assets/shaders/planets.terrain.vertexpos.comp"};
// ComputeShaderPipeline terrainVertexNormalCompute {terrainVertexComputeLayout, "modules/test_planets_rtx/assets/shaders/planets.terrain.vertexnormal.comp"};

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

class TerrainGenerator {
	V4D_MODULE_CLASS_H(TerrainGenerator
		,Init
		,GetHeightMap
		,GetColor
	)
	V4D_MODULE_FUNC(void, Init)
	V4D_MODULE_FUNC(double, GetHeightMap, glm::dvec3* const pos)
	V4D_MODULE_FUNC(glm::vec3, GetColor, double heightMap)
};
V4D_MODULE_CLASS_CPP(TerrainGenerator)

namespace TerrainGeneratorLib {
	bool running = false;
	TerrainGenerator* generatorLib = nullptr;
	std::thread* autoReloaderThread = nullptr;
	std::mutex mu;
	
	void Load() {
		std::lock_guard generatorLock(mu);
		if (generatorLib) {
			TerrainGenerator::UnloadModule(THIS_MODULE);
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
			if (terrain) {
				std::scoped_lock lock(terrain->planetMutex);
				#ifdef _DEBUG
					terrain->totalChunkTimeNb = 0;
					terrain->totalChunkTime = 0;
				#endif
				terrain->RemoveBaseChunks();
				terrain->AddBaseChunks();
			}
		//
		LOG_SUCCESS("terrain generator submodule Loaded")
		// PlanetTerrain::StartChunkGenerator();
	}
	void Unload() {
		// PlanetTerrain::EndChunkGenerator();
		PlanetTerrain::generatorFunction = nullptr;
		PlanetTerrain::generateColor = nullptr;
		TerrainGenerator::UnloadModule(THIS_MODULE);
	}
	void Start() {
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

#pragma endregion

ObjectInstance* sun = nullptr;
PlanetAtmosphereShaderPipeline* planetAtmosphereShader = nullptr;
Device* renderingDevice = nullptr;

extern "C" {
	
	void ModuleLoad() {
		// Load Dependencies
		V4D_Renderer::LoadModule(THIS_MODULE);
		V4D_Input::LoadModule("V4D_sample");
		V4D_Game::LoadModule("V4D_sample");
	}
	
	void ModuleSetCustomPtr(int what, void* ptr) {
		switch (what) {
			case ATMOSPHERE_SHADER: 
				planetAtmosphereShader = (PlanetAtmosphereShaderPipeline*) ptr;
				break;
		}
	}
	
	void LoadScene(Scene& scene) {
		TerrainGeneratorLib::Start();
		
		// Light source
		sun = scene.AddObjectInstance();
		sun->Configure([](ObjectInstance* obj){
			obj->SetSphereLightSource("light", 700000000, 1e24f);
		}, {-1.496e+11,0, 0});
				
						// // Planet
						// scene.objectInstances.emplace_back(new ObjectInstance("planet_raymarching"))->Configure([](ObjectInstance* obj){
						// 	obj->SetSphereGeometry((float)planet.solidRadius, {1,0,0, 1}, 0/*planet index*/);
						// }, {0,planet.solidRadius*2,0});
				
	}
	
	void UnloadScene(Scene& scene) {
		TerrainGeneratorLib::Stop();
	}
	
	// Images / Buffers / Pipelines
	void CreateResources(Device* device) {
		renderingDevice = device;
		// Chunk Generator
		PlanetTerrain::StartChunkGenerator();
	}
	
	void DestroyResources(Device* device) {
		// Chunk Generator
		PlanetTerrain::EndChunkGenerator();
		if (terrain) {
			terrain->atmosphere.Free(device);
			delete terrain;
			terrain = nullptr;
			if (planetAtmosphereShader) {
				planetAtmosphereShader->planets.clear();
				planetAtmosphereShader->lightSources.clear();
			}
		}
	}
	
	void Update(Scene& scene) {
		std::lock_guard generatorLock(TerrainGeneratorLib::mu);
		// For each planet
			if (!terrain) {
				terrain = new PlanetTerrain {planet.atmosphereRadius, planet.solidRadius, planet.heightVariation, {0,planet.atmosphereRadius*1.5,0}};
				{
					std::lock_guard lock(terrain->planetMutex);
					terrain->scene = &scene;
					sun->GenerateGeometries();
					if (planetAtmosphereShader) {
						planetAtmosphereShader->planets.push_back(terrain);
						planetAtmosphereShader->lightSources.push_back(sun->GetLightSources()[0]);
					}
					if (terrain->atmosphere.radius > 0) {
						renderingDevice->RunSingleTimeCommands(renderingDevice->GetQueue("graphics"), [](auto cmdBuffer){
							terrain->atmosphere.Allocate(renderingDevice, cmdBuffer);
						});
					}
				}
			}
		//
		
		
						// // // for each planet
						// 	int planetIndex = 0;
						// 	auto* planetBuffer = &((PlanetBuffer*)(planetsBuffer.stagingBuffer.data))[planetIndex];
						// // 	planetBuffer->viewToPlanetPosMatrix = glm::inverse(scene.camera.viewMatrix * scene.objectInstances[1]->GetWorldTransform());
						// 	planetBuffer->northDir = glm::normalize(glm::transpose(glm::inverse(glm::dmat3(terrain->matrix))) * glm::dvec3(0,1,0));
						// // //
						// auto cmdBuffer = r->BeginSingleTimeCommands(*graphicsQueue);
						// planetsBuffer.Update(r->renderingDevice, cmdBuffer);
						// r->EndSingleTimeCommands(*graphicsQueue, cmdBuffer);
				
		
		
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
			terrain->cameraAltitudeAboveTerrain = glm::length(terrain->cameraPos) - terrain->GetHeightMap(glm::normalize(terrain->cameraPos), 0.5);
			
			for (auto* chunk : terrain->chunks) {
				chunk->BeforeRender();
			}
			
		//
	}
	
	void Update2(Scene& scene) {
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
	
	void Compute(VkCommandBuffer commandBuffer) {
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
	
	#ifdef _ENABLE_IMGUI
		void RunImGui() {
			// for each planet
				ImGui::Separator();
				ImGui::Text("Planet");
				if (terrain) {
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
		#ifdef _DEBUG
			void RunImGuiDebug() {
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
					ImGui::Text("Chunk generator queue : %d", (int)PlanetTerrain::chunkGeneratorQueue.size());
					ImGui::Text("AvgChunkTime: %d ms", (int)std::round(float(terrain->totalChunkTime)/terrain->totalChunkTimeNb));
				}
			}
		#endif
	#endif
	
}
