#include "common.hh"
#include "../V4D_flycam/common.hh"

// #define PHYSICS_GENERATE_COLLIDERS_UPON_OBJECT_GOING_THROUGH

using namespace v4d::graphics;
using namespace v4d::scene;
using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::vulkan::rtx;

const double distanceFromChunkToGenerateCollider = 2000;

#pragma region Planet

struct Planet {
	double solidRadius = 8000000;
	double atmosphereRadius = 8200000;
	double heightVariation = 10000;
} planet;

#pragma endregion

const glm::dvec3 sun1Position = {-1.496e+11,0, 0};
const glm::dvec3 sun2Position = {1.296e+11,-6.496e+10, 0};

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
ObjectInstancePtr sun2 = nullptr;
PlanetAtmosphereShaderPipeline* planetAtmosphereShader = nullptr;
Device* renderingDevice = nullptr;
Scene* scene = nullptr;
Renderer* r = nullptr;
V4D_Mod* mainRenderModule = nullptr;
PlayerView* playerView = nullptr;

#ifdef PHYSICS_GENERATE_COLLIDERS_UPON_OBJECT_GOING_THROUGH
	std::vector<glm::dvec3> collidingObjectPositions {};
#endif

#pragma region Graphics stuff

PipelineLayout planetsMapGenLayout;

ComputeShaderPipeline 
	bumpMapsAltitudeGen{planetsMapGenLayout, "modules/V4D_planetdemo/assets/shaders/planets.bump.altitude.map.comp"},
	bumpMapsNormalsGen{planetsMapGenLayout, "modules/V4D_planetdemo/assets/shaders/planets.bump.normals.map.comp"}
;

RasterShaderPipeline* planetTerrainRasterShader = nullptr;

struct MapGenPushConstant {
	int planetIndex;
	float planetHeightVariation;
} mapGenPushConstant;

DescriptorSet mapsGenDescriptorSet, mapsSamplerDescriptorSet;

#define MAX_PLANETS 1

Image bumpMaps[1] {// xyz=normal, a=altitude
	Image { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 1, 1, {VK_FORMAT_R32G32B32A32_SFLOAT}},
};
bool bumpMapsGenerated = false;

#pragma endregion

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

V4D_MODULE_CLASS(V4D_Mod) {
	
	V4D_MODULE_FUNC(void, ModuleLoad) {
		// Load Dependencies
		mainRenderModule = V4D_Mod::LoadModule("V4D_hybrid");
		playerView = (PlayerView*)V4D_Mod::LoadModule("V4D_flycam")->ModuleGetCustomPtr(0);
		// playerView->camSpeed = 100000;
		auto worldPosition = glm::dvec3{-493804, -7.27024e+06, 3.33978e+06};
		playerView->SetInitialPositionAndView(worldPosition, glm::normalize(sun1Position), glm::normalize(worldPosition), true);
	}
	
	V4D_MODULE_FUNC(void, ModuleUnload) {
		if (planetAtmosphereShader) delete planetAtmosphereShader;
		if (planetTerrainRasterShader) delete planetTerrainRasterShader;
	}
	
	V4D_MODULE_FUNC(void, InitRenderer, Renderer* _r) {
		r = _r;
	}
	
	V4D_MODULE_FUNC(void, InitVulkanDeviceFeatures) {
		r->deviceFeatures.shaderStorageImageWriteWithoutFormat = VK_TRUE;
	}
	
	V4D_MODULE_FUNC(void, LoadScene, Scene* _s) {
		scene = _s;
		
		static const int maxTerrainChunksInMemory = 1800; // 1.0 Gb (577 kb per chunk)
		v4d::scene::Geometry::globalBuffers.objectBuffer.Extend(maxTerrainChunksInMemory);
		v4d::scene::Geometry::globalBuffers.geometryBuffer.Extend(maxTerrainChunksInMemory);
		v4d::scene::Geometry::globalBuffers.vertexBuffer.Extend(maxTerrainChunksInMemory * PlanetTerrain::nbVerticesPerChunk);
		v4d::scene::Geometry::globalBuffers.indexBuffer.Extend(maxTerrainChunksInMemory * PlanetTerrain::nbIndicesPerChunk);
		
		TerrainGeneratorLib::Start();
		
		// Light source
		sun = scene->AddObjectInstance();
		sun->Configure([](ObjectInstance* obj){
			obj->SetSphereLightSource("light", 700000000, 1e24f);
		}, sun1Position);
		
		sun2 = scene->AddObjectInstance();
		sun2->Configure([](ObjectInstance* obj){
			obj->SetSphereLightSource("light", 700000000, 5e22f);
		}, sun2Position);
		
	}
	
	V4D_MODULE_FUNC(void, UnloadScene) {
		TerrainGeneratorLib::Stop();
	}
	
	// Images / Buffers / Pipelines
	V4D_MODULE_FUNC(void, CreateVulkanResources2, Device* device) {
		renderingDevice = device;
		// Chunk Generator
		PlanetTerrain::StartChunkGenerator();
	}
	
	V4D_MODULE_FUNC(void, DestroyVulkanResources2, Device* device) {
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
	
	V4D_MODULE_FUNC(void, BeginFrameUpdate) {
		// LOG(playerView->worldPosition.x << ", " << playerView->worldPosition.y << ", " << playerView->worldPosition.z)
		
		std::lock_guard generatorLock(TerrainGeneratorLib::mu);
		// For each planet
			if (!terrain) {
				// terrain = new PlanetTerrain {planet.atmosphereRadius, planet.solidRadius, planet.heightVariation, {0,planet.atmosphereRadius*1.5,0}};
				terrain = new PlanetTerrain {planet.atmosphereRadius, planet.solidRadius, planet.heightVariation, {0,0,0}};
				{
					std::lock_guard lock(terrain->planetMutex);
					terrain->scene = scene;
					sun->GenerateGeometries();
					sun2->GenerateGeometries();
					if (planetAtmosphereShader) {
						planetAtmosphereShader->planets.push_back(terrain);
						planetAtmosphereShader->lightSources.push_back(sun->GetLightSources()[0]);
						planetAtmosphereShader->lightSources.push_back(sun2->GetLightSources()[0]);
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
	
	V4D_MODULE_FUNC(void, BeginSecondaryFrameUpdate) {
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
	
	V4D_MODULE_FUNC(void, SecondaryFrameCompute, VkCommandBuffer commandBuffer) {
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
	
	V4D_MODULE_FUNC(void, DrawUi2) {
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
					ImGui::Text("Sun1");
					static glm::vec3 sunPosition = glm::normalize(glm::dvec3(sun->GetWorldTransform()[3]));
					static double sunDistance = glm::distance(glm::dvec3(sun->GetWorldTransform()[3]), terrain->absolutePosition);
					static float intensity = std::log10(sun->GetLightSources()[0]->intensity);
					float* pos = (float*)&sunPosition;
					ImGui::SliderFloat("Intensity1", &intensity, 20, 27);
					ImGui::SliderFloat3("Position1", pos, -1, 1);
					ImGui::ColorEdit3("Color1", (float*)&sun->GetLightSources()[0]->color);
					sun->GetLightSources()[0]->intensity = std::pow(10.0f, intensity);
					sun->SetWorldTransform(glm::translate(glm::dmat4(1), terrain->absolutePosition + glm::normalize(glm::dvec3(sunPosition)) * sunDistance));
					
					ImGui::Separator();
					ImGui::Text("Sun2");
					static glm::vec3 sunPosition2 = glm::normalize(glm::dvec3(sun2->GetWorldTransform()[3]));
					static double sunDistance2 = glm::distance(glm::dvec3(sun2->GetWorldTransform()[3]), terrain->absolutePosition);
					static float intensity2 = std::log10(sun2->GetLightSources()[0]->intensity);
					float* pos2 = (float*)&sunPosition2;
					ImGui::SliderFloat("Intensity2", &intensity2, 20, 27);
					ImGui::SliderFloat3("Position2", pos2, -1, 1);
					ImGui::ColorEdit3("Color2", (float*)&sun2->GetLightSources()[0]->color);
					sun2->GetLightSources()[0]->intensity = std::pow(10.0f, intensity2);
					sun2->SetWorldTransform(glm::translate(glm::dmat4(1), terrain->absolutePosition + glm::normalize(glm::dvec3(sunPosition2)) * sunDistance2));
					
				}
			//
		#endif
	}
	V4D_MODULE_FUNC(void, DrawUiDebug2) {
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
	
	V4D_MODULE_FUNC(void, InitVulkanLayouts) {
		auto* rasterVisibilityPipelineLayout = mainRenderModule->GetPipelineLayout("pl_visibility_raster");
		auto* rayTracingVisibilityPipelineLayout = mainRenderModule->GetPipelineLayout("pl_visibility_rays");
		auto* rayTracingLightingPipelineLayout = mainRenderModule->GetPipelineLayout("pl_lighting_rays");
		auto* fogPipelineLayout = mainRenderModule->GetPipelineLayout("pl_fog_raster");
		
		r->descriptorSets["mapsGen"] = &mapsGenDescriptorSet;
		r->descriptorSets["mapsSampler"] = &mapsSamplerDescriptorSet;
		planetsMapGenLayout.AddDescriptorSet(r->descriptorSets["set0_base"]);
		planetsMapGenLayout.AddDescriptorSet(&mapsGenDescriptorSet);
		planetsMapGenLayout.AddPushConstant<MapGenPushConstant>(VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet.AddBinding_imageView_array(0, bumpMaps, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsSamplerDescriptorSet.AddBinding_combinedImageSampler_array(0, bumpMaps, 1, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		
		rayTracingVisibilityPipelineLayout->AddDescriptorSet(&mapsSamplerDescriptorSet);
		rayTracingLightingPipelineLayout->AddDescriptorSet(&mapsSamplerDescriptorSet);
		rasterVisibilityPipelineLayout->AddDescriptorSet(&mapsSamplerDescriptorSet);
		
		// Atmosphere raster shader
		planetAtmosphereShader = new PlanetAtmosphereShaderPipeline {*fogPipelineLayout, {
			"modules/V4D_planetdemo/assets/shaders/planetAtmosphere.vert",
			"modules/V4D_planetdemo/assets/shaders/planetAtmosphere.frag",
		}};
		planetAtmosphereShader->camera = &scene->camera;
		planetAtmosphereShader->atmospherePushConstantIndex = fogPipelineLayout->AddPushConstant<PlanetAtmosphereShaderPipeline::PlanetAtmospherePushConstant>(VK_SHADER_STAGE_ALL_GRAPHICS);
		mainRenderModule->AddShader("sg_fog", planetAtmosphereShader);
		
		// Terrain raster shader
		planetTerrainRasterShader = new RasterShaderPipeline(*rasterVisibilityPipelineLayout, {
			Geometry::geometryRenderTypes["terrain"].rasterShader->GetShaderPath("vert"),
			"modules/V4D_planetdemo/assets/shaders/planets.terrain.frag",
		});
		mainRenderModule->AddShader("sg_visibility", planetTerrainRasterShader);
	}
	
	V4D_MODULE_FUNC(void, ConfigureShaders) {
		auto* shaderBindingTableVisibility = mainRenderModule->GetShaderBindingTable("sbt_visibility");
		auto* shaderBindingTableLighting = mainRenderModule->GetShaderBindingTable("sbt_lighting");
		
		Geometry::geometryRenderTypes["planet_terrain"].sbtOffset = 
			shaderBindingTableVisibility->AddHitShader("modules/V4D_planetdemo/assets/shaders/planets.terrain.rchit");
			shaderBindingTableLighting->AddHitShader("modules/V4D_planetdemo/assets/shaders/planets.terrain.rchit");
		Geometry::geometryRenderTypes["planet_terrain"].rasterShader = planetTerrainRasterShader;
		
		// Atmosphere
		planetAtmosphereShader->AddVertexInputBinding(sizeof(PlanetAtmosphere::Vertex), VK_VERTEX_INPUT_RATE_VERTEX, PlanetAtmosphere::Vertex::GetInputAttributes());
		#ifdef PLANETARY_ATMOSPHERE_MESH_USE_TRIANGLE_STRIPS
			planetAtmosphereShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			planetAtmosphereShader->inputAssembly.primitiveRestartEnable = VK_TRUE;
		#else
			planetAtmosphereShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		#endif
	}
	
	V4D_MODULE_FUNC(void, ReadShaders) {
		bumpMapsAltitudeGen.ReadShaders();
		bumpMapsNormalsGen.ReadShaders();
	}
	
	// Images / Buffers / Pipelines
	V4D_MODULE_FUNC(void, CreateVulkanResources) {
		// Bump Maps
		bumpMaps[0].samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].Create(r->renderingDevice, 4096);
		
		auto cmdBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("transfer"));
			r->TransitionImageLayout(cmdBuffer, bumpMaps[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("transfer"), cmdBuffer);
	}
	
	V4D_MODULE_FUNC(void, DestroyVulkanResources) {
		bumpMaps[0].Destroy(r->renderingDevice);
		bumpMapsGenerated = false;
	}
	
	V4D_MODULE_FUNC(void, CreateVulkanPipelines) {
		planetsMapGenLayout.Create(r->renderingDevice);
		bumpMapsAltitudeGen.CreatePipeline(r->renderingDevice);
		bumpMapsNormalsGen.CreatePipeline(r->renderingDevice);
	}
	
	V4D_MODULE_FUNC(void, DestroyVulkanPipelines) {
		bumpMapsAltitudeGen.DestroyPipeline(r->renderingDevice);
		bumpMapsNormalsGen.DestroyPipeline(r->renderingDevice);
		planetsMapGenLayout.Destroy(r->renderingDevice);
	}
	
	V4D_MODULE_FUNC(void, SecondaryRenderUpdate2, VkCommandBuffer cmdBuffer) {
		if (!bumpMapsGenerated) {
			
			bumpMapsAltitudeGen.SetGroupCounts(bumpMaps[0].width, bumpMaps[0].height, bumpMaps[0].arrayLayers);
			bumpMapsAltitudeGen.Execute(r->renderingDevice, cmdBuffer);
			
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
			r->renderingDevice->CmdPipelineBarrier(
				cmdBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
	
			bumpMapsNormalsGen.SetGroupCounts(bumpMaps[0].width, bumpMaps[0].height, bumpMaps[0].arrayLayers);
			bumpMapsNormalsGen.Execute(r->renderingDevice, cmdBuffer);
				
			bumpMapsGenerated = true;
		}
	}
	
};
