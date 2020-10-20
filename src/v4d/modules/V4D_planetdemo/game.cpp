#include "common.hh"
#include "../V4D_flycam/common.hh"

// #define PHYSICS_GENERATE_COLLIDERS_UPON_OBJECT_GOING_THROUGH

using namespace v4d::graphics;
using namespace v4d::scene;

const double distanceFromChunkToGenerateCollider = 2000;

#pragma region Planet

struct Planet {
	double solidRadius = 8000000;
	double atmosphereRadius = 8200000;
	double heightVariation = 10000;
} planet;

#pragma endregion

#pragma region Terrain generator

PlanetTerrain* terrain = nullptr;

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

class TerrainGenerator {
	V4D_MODULE_CLASS_HEADER(TerrainGenerator
		,Init
		,GetHeightMap
		,GetColor
	)
	V4D_MODULE_FUNC_DECLARE(void, Init)
	V4D_MODULE_FUNC_DECLARE(double, GetHeightMap, glm::dvec3* const pos)
	V4D_MODULE_FUNC_DECLARE(glm::vec3, GetColor, double heightMap)
};
V4D_MODULE_CLASS_CPP(TerrainGenerator)

namespace TerrainGeneratorLib {
	bool running = false;
	TerrainGenerator* generatorLib = nullptr;
	std::thread* autoReloaderThread = nullptr;
	std::mutex mu;
	
	void Unload() {
		PlanetTerrain::EndChunkGenerator();
		PlanetTerrain::generatorFunction = nullptr;
		PlanetTerrain::generateColor = nullptr;
		TerrainGenerator::UnloadModule(THIS_MODULE);
	}
	void Load() {
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
		PlanetTerrain::StartChunkGenerator();
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

ObjectInstancePtr sun = nullptr;
PlanetAtmosphereShaderPipeline* planetAtmosphereShader = nullptr;
Device* renderingDevice = nullptr;
Scene* scene = nullptr;

#ifdef PHYSICS_GENERATE_COLLIDERS_UPON_OBJECT_GOING_THROUGH
	std::vector<glm::dvec3> collidingObjectPositions {};
#endif

void ComputeChunkVertices(Device* device, VkCommandBuffer commandBuffer, PlanetTerrain::Chunk* chunk, int& chunksToGeneratePerFrame) {
	if (chunk->active) {
		
		// subChunks
		{
			std::scoped_lock lock(chunk->subChunksMutex);
			for (auto* subChunk : chunk->subChunks) {
				ComputeChunkVertices(device, commandBuffer, subChunk, chunksToGeneratePerFrame);
			}
		}
		
		if (chunk->obj && chunk->meshGenerated) {
			if (chunk->computedLevel == 0) {
				chunk->obj->Lock();
					chunk->obj->physicsActive = false;
					chunk->geometry->colliderType = v4d::scene::Geometry::ColliderType::TRIANGLE_MESH;
					chunk->obj->SetGeometriesDirty();
					chunk->obj->PushGeometries(device, commandBuffer);
					chunk->computedLevel = 2;
					chunk->obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
					if (chunk->geometry->blas) {
						chunk->geometry->blas->built = false;
						LOG_WARN("BLAS already created but should not be...")
					}
					chunk->geometry->active = true;
					chunk->geometry->colliderDirty = true;
					chunk->obj->SetGenerated();
				chunk->obj->Unlock();
			}
			#ifdef PHYSICS_GENERATE_COLLIDERS_UPON_OBJECT_GOING_THROUGH
				else if (!chunk->obj->physicsActive) {
					for (auto& pos : collidingObjectPositions) {
						if (glm::distance(chunk->centerPos, pos) < chunk->obj->boundingDistance) {
							chunk->obj->physicsActive = true;
							chunk->obj->physicsDirty = true;
							break;
						}
					}
				}
			#else
				else if (!chunk->obj->physicsActive) {
					if (chunk->distanceFromCamera < distanceFromChunkToGenerateCollider) {
						chunk->obj->physicsActive = true;
						chunk->obj->physicsDirty = true;
					}
				} else if (chunk->obj->physicsActive) {
					if (chunk->distanceFromCamera > distanceFromChunkToGenerateCollider*1.5) {
						chunk->obj->physicsActive = false;
						chunk->obj->physicsDirty = true;
					}
				}
			#endif
		}
	}
}

V4D_MODULE_CLASS(V4D_Game) {
	
	V4D_MODULE_FUNC(void, ModuleLoad) {
		// Load Dependencies
		V4D_Renderer::LoadModule(THIS_MODULE);
		((PlayerView*)V4D_Game::LoadModule("V4D_flycam")->ModuleGetCustomPtr(0))->camSpeed = 100000;
	}
	
	V4D_MODULE_FUNC(void, ModuleSetCustomPtr, int what, void* ptr) {
		switch (what) {
			case ATMOSPHERE_SHADER: 
				planetAtmosphereShader = (PlanetAtmosphereShaderPipeline*) ptr;
				break;
		}
	}
	
	V4D_MODULE_FUNC(void, Init, Scene* _s) {
		scene = _s;
		
		static const int maxTerrainChunksInMemory = 1800; // 1.0 Gb (577 kb per chunk)
		v4d::scene::Geometry::globalBuffers.objectBuffer.Extend(maxTerrainChunksInMemory);
		v4d::scene::Geometry::globalBuffers.geometryBuffer.Extend(maxTerrainChunksInMemory);
		v4d::scene::Geometry::globalBuffers.vertexBuffer.Extend(maxTerrainChunksInMemory * PlanetTerrain::nbVerticesPerChunk);
		v4d::scene::Geometry::globalBuffers.indexBuffer.Extend(maxTerrainChunksInMemory * PlanetTerrain::nbIndicesPerChunk);
	}
	
	V4D_MODULE_FUNC(void, LoadScene) {
		TerrainGeneratorLib::Start();
		
		// Light source
		sun = scene->AddObjectInstance();
		sun->Configure([](ObjectInstance* obj){
			obj->SetSphereLightSource("light", 700000000, 1e24f);
		}, {-1.496e+11,0, 0});
		
	}
	
	V4D_MODULE_FUNC(void, UnloadScene) {
		TerrainGeneratorLib::Stop();
	}
	
	// Images / Buffers / Pipelines
	V4D_MODULE_FUNC(void, RendererCreateResources, Device* device) {
		renderingDevice = device;
		// Chunk Generator
		PlanetTerrain::StartChunkGenerator();
	}
	
	V4D_MODULE_FUNC(void, RendererDestroyResources, Device* device) {
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
	
	V4D_MODULE_FUNC(void, RendererFrameUpdate) {
		std::lock_guard generatorLock(TerrainGeneratorLib::mu);
		// For each planet
			if (!terrain) {
				terrain = new PlanetTerrain {planet.atmosphereRadius, planet.solidRadius, planet.heightVariation, {0,planet.atmosphereRadius*1.5,0}};
				{
					std::lock_guard lock(terrain->planetMutex);
					terrain->scene = scene;
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
						// // 	planetBuffer->viewToPlanetPosMatrix = glm::inverse(scene->camera.viewMatrix * scene->objectInstances[1]->GetWorldTransform());
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
			terrain->cameraPos = glm::inverse(terrain->matrix) * glm::dvec4(scene->camera.worldPosition, 1);
			terrain->cameraAltitudeAboveTerrain = glm::length(terrain->cameraPos) - terrain->GetHeightMap(glm::normalize(terrain->cameraPos), 0.5);
			
			for (auto* chunk : terrain->chunks) {
				chunk->BeforeRender();
			}
			
		//
	}
	
	V4D_MODULE_FUNC(void, RendererFrameUpdate2) {
		// for each planet
			if (terrain) {
				std::lock_guard lock(terrain->chunksMutex);
				for (auto* chunk : terrain->chunks) {
					chunk->Process();
				}
				// terrain->Optimize();
			}
		//
		// PlanetTerrain::CollectGarbage(renderingDevice);
	}
	
	V4D_MODULE_FUNC(void, RendererFrameCompute, VkCommandBuffer commandBuffer) {
		// for each planet
			if (terrain) {
				scene->gravityVector = glm::normalize(terrain->cameraPos) * -9.8;
				
				#ifdef PHYSICS_GENERATE_COLLIDERS_UPON_OBJECT_GOING_THROUGH
					collidingObjectPositions.clear();
					const double maxTerrainHeight = terrain->solidRadius + terrain->heightVariation;
					scene->Lock();
						for (auto obj : scene->objectInstances) {
							if (obj && obj->IsPhysicsActive() && obj->rigidbodyType == v4d::scene::ObjectInstance::RigidBodyType::DYNAMIC && obj->physicsObject) {
								auto positionRelativeToPlanetCenter = obj->GetWorldPosition() - terrain->absolutePosition;
								auto distanceFromPlanetCenter = positionRelativeToPlanetCenter.length() - obj->boundingDistance;
								if (distanceFromPlanetCenter < maxTerrainHeight) {
									if (distanceFromPlanetCenter < terrain->GetHeightMap(glm::normalize(positionRelativeToPlanetCenter), 0) + 5/* threshold of 5 meters above terrain */) {
										collidingObjectPositions.push_back(positionRelativeToPlanetCenter);
									}
								}
							}
						}
					scene->Unlock();
				#endif
				
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
	
	V4D_MODULE_FUNC(void, RendererRunUi) {
		#ifdef _ENABLE_IMGUI
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
		#endif
	}
	V4D_MODULE_FUNC(void, RendererRunUiDebug) {
		// #ifdef _DEBUG
			#ifdef _ENABLE_IMGUI
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
			#endif
		// #endif
	}
	
};
