#define _V4D_MODULE
#include <v4d.h>

#include "PlanetsRenderer/PlanetTerrain.hpp"
#include "../V4D_flycam/common.hh"

using namespace v4d::graphics;
using namespace v4d::graphics::Mesh;
using namespace v4d::scene;
using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::vulkan::rtx;

const double distanceFromChunkToGenerateCollider = 1'000;

std::shared_ptr<RenderableGeometryEntity> sun = nullptr;
std::shared_ptr<RenderableGeometryEntity> sun2 = nullptr;
Device* renderingDevice = nullptr;
Scene* scene = nullptr;
Renderer* r = nullptr;
V4D_Mod* mainRenderModule = nullptr;
PlayerView* playerView = nullptr;
bool automaticChunkSubdivisionDistanceFactor = true;

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
				// #ifdef _DEBUG
					terrain->totalChunkTimeNb = 0;
					terrain->totalChunkTime = 0;
				// #endif
				terrain->RemoveBaseChunks();
				terrain->AddBaseChunks();
			}
		//
		LOG_SUCCESS("terrain generator submodule Loaded")
		PlanetTerrain::StartChunkGenerator(renderingDevice);
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

#pragma region Graphics stuff

PipelineLayout planetsMapGenLayout;

ComputeShaderPipeline 
	bumpMapsAltitudeGen{planetsMapGenLayout, "modules/V4D_planetdemo/assets/shaders/planet.bump.altitude.map.comp"},
	bumpMapsNormalsGen{planetsMapGenLayout, "modules/V4D_planetdemo/assets/shaders/planet.bump.normals.map.comp"}
;

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

void RefreshChunk(PlanetTerrain::Chunk* chunk) {
	if (chunk->active) {
		{// subChunks
			std::scoped_lock lock(chunk->subChunksMutex);
			for (auto* subChunk : chunk->subChunks) {
				RefreshChunk(subChunk);
			}
		}
		if (chunk->entity) {
			if (chunk->computedLevel == 1) {
				auto lock = RenderableGeometryEntity::GetLock();
				chunk->computedLevel = 2;
				chunk->entity->generator = [](RenderableGeometryEntity*, Device*){};
			} else if (chunk->computedLevel == 2) {
				if (!chunk->colliderActive) {
					if (chunk->distanceFromCamera < distanceFromChunkToGenerateCollider) {
						auto physics = chunk->entity->physics.Lock();
						if (physics) {
							physics->rigidbodyType = PhysicsInfo::RigidBodyType::STATIC;
							physics->colliderType = PhysicsInfo::ColliderType::MESH;
							physics->colliderDirty = true;
							physics->physicsDirty = true;
							chunk->colliderActive = true;
						}
					}
				} else {
					if (chunk->distanceFromCamera > distanceFromChunkToGenerateCollider*2.0) {
						auto physics = chunk->entity->physics.Lock();
						if (physics) {
							physics->rigidbodyType = PhysicsInfo::RigidBodyType::NONE;
							physics->colliderType = PhysicsInfo::ColliderType::NONE;
							physics->physicsDirty = true;
							physics->colliderDirty = true;
							chunk->colliderActive = false;
						}
					}
				}
			}
		}
	}
}

V4D_MODULE_CLASS(V4D_Mod) {
	
	V4D_MODULE_FUNC(void, ModuleLoad) {
		// Load Dependencies
		mainRenderModule = V4D_Mod::LoadModule(APP_MAIN_RENDER_MODULE);
		playerView = (PlayerView*)V4D_Mod::LoadModule("V4D_flycam")->ModuleGetCustomPtr(0);
		// playerView->camSpeed = 100000;
		auto worldPosition = glm::dvec3{-493804, -7.27024e+06, 3.33978e+06};
		playerView->SetInitialPositionAndView(worldPosition, glm::normalize(sun1Position), glm::normalize(worldPosition), true);
	}
	
	V4D_MODULE_FUNC(void, InitRenderer, Renderer* _r) {
		r = _r;
	}
	
	V4D_MODULE_FUNC(void, LoadScene, Scene* _s) {
		scene = _s;
	}
	
	// Images / Buffers / Pipelines
	V4D_MODULE_FUNC(void, CreateVulkanResources2, Device* device) {
		renderingDevice = device;
		// Chunk Generator
		TerrainGeneratorLib::Start();
		PlanetTerrain::StartChunkGenerator(device);
	}
	
	V4D_MODULE_FUNC(void, DestroyVulkanResources2, Device* device) {
		// Chunk Generator
		PlanetTerrain::EndChunkGenerator();
		TerrainGeneratorLib::Stop();
		if (terrain) {
			delete terrain;
			terrain = nullptr;
		}
		if (sun) {
			sun->Destroy();
			sun = nullptr;
		}
		if (sun2) {
			sun2->Destroy();
			sun2 = nullptr;
		}
	}
	
	V4D_MODULE_FUNC(void, BeginFrameUpdate) {
		// LOG(playerView->worldPosition.x << ", " << playerView->worldPosition.y << ", " << playerView->worldPosition.z)
		
		// Light sources
		if (!sun) {
			sun = RenderableGeometryEntity::Create(THIS_MODULE);
			sun->SetInitialTransform(glm::translate(glm::dmat4(1), sun1Position));
			sun->generator = [](auto* entity, Device* device){
				float lightIntensity = 1e24f;
				float radius = 700000000;
				entity->Allocate(device, "V4D_raytracing:aabb_sphere.light")->material.visibility.emission = lightIntensity;
				entity->rayTracingMask = RAY_TRACED_ENTITY_LIGHT;
				entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
				entity->Add_meshVertexColorU8()->AllocateBuffers(device, {255,255,255,255});
				entity->Add_lightSource(glm::vec3{0,0,0}, glm::vec3{1}, radius, lightIntensity);
			};
		}
		if (!sun2) {
			sun2 = RenderableGeometryEntity::Create(THIS_MODULE);
			sun2->SetInitialTransform(glm::translate(glm::dmat4(1), sun2Position));
			sun2->generator = [](auto* entity, Device* device){
				float lightIntensity = 5e22f;
				float radius = 400000000;
				entity->Allocate(device, "V4D_raytracing:aabb_sphere.light")->material.visibility.emission = lightIntensity;
				entity->rayTracingMask = RAY_TRACED_ENTITY_LIGHT;
				entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
				entity->Add_meshVertexColorU8()->AllocateBuffers(device, {255,255,255,255});
				entity->Add_lightSource(glm::vec3{0,0,0}, glm::vec3{1}, radius, lightIntensity);
			};
		}
		
		std::lock_guard generatorLock(TerrainGeneratorLib::mu);
		// For each planet
			if (!terrain) {
				// terrain = new PlanetTerrain {planet.atmosphereRadius, planet.solidRadius, planet.heightVariation, {0,planet.atmosphereRadius*1.5,0}};
				terrain = new PlanetTerrain {planet.atmosphereRadius, planet.solidRadius, planet.heightVariation, {0,0,0}};
				{
					std::lock_guard lock(terrain->planetMutex);
					terrain->scene = scene;
				}
			}
		//
		
		{
						// // // for each planet
						// 	int planetIndex = 0;
						// 	auto* planetBuffer = &((PlanetBuffer*)(planetsBuffer.stagingBuffer.data))[planetIndex];
						// // 	planetBuffer->viewToPlanetPosMatrix = glm::inverse(scene->camera.viewMatrix * scene->objectInstances[1]->GetWorldTransform());
						// 	planetBuffer->northDir = glm::normalize(glm::transpose(glm::inverse(glm::dmat3(terrain->matrix))) * glm::dvec3(0,1,0));
						// // //
						// auto cmdBuffer = r->BeginSingleTimeCommands(*graphicsQueue);
						// planetsBuffer.Update(renderingDevice, cmdBuffer);
						// r->EndSingleTimeCommands(*graphicsQueue, cmdBuffer);
		}
		
		{// for each planet
			std::lock_guard lock(terrain->chunksMutex);
			
			// //TODO Planet rotation
			// static v4d::Timer time(true);
			// // terrain->rotationAngle = time.GetElapsedSeconds()/1000000000;
			// // terrain->rotationAngle = time.GetElapsedSeconds()/30;
			// terrain->rotationAngle += 0.0001;
			
			terrain->RefreshMatrix();
			
			// Camera position relative to planet
			terrain->cameraPos = glm::inverse(terrain->matrix) * glm::dvec4(scene->camera.worldPosition, 1);
			auto terrainHeightAtThisPosition = terrain->GetHeightMap(glm::normalize(terrain->cameraPos), 0.5);
			terrain->cameraAltitudeAboveTerrain = glm::length(terrain->cameraPos) - terrainHeightAtThisPosition;
			
			if (automaticChunkSubdivisionDistanceFactor) {
				PlanetTerrain::chunkSubdivisionDistanceFactor = glm::mix(1.0, 4.0, glm::pow(glm::clamp(terrain->cameraAltitudeAboveTerrain / (terrain->solidRadius / 8), 0.0, 1.0), 0.25));
			}
			
			// // Player Constraints (velocity and altitude)
			// playerView->camSpeed = glm::min(20'000.0, glm::min(playerView->camSpeed, glm::round(glm::max(glm::abs(terrain->cameraAltitudeAboveTerrain)/2.0, 2.0))));
			// double playerRadius = 0.3;
			// if (terrain->cameraAltitudeAboveTerrain < playerRadius) {
			// 	playerView->worldPosition = scene->camera.worldPosition = terrain->matrix * glm::dvec4(glm::normalize(terrain->cameraPos) * (terrainHeightAtThisPosition + playerRadius), 1);
			// 	scene->camera.RefreshViewMatrix();
			// }
			
			for (auto* chunk : terrain->chunks) {
				chunk->BeforeRender();
			}
			
			
			
			// Gravity
			scene->gravityVector = glm::normalize(terrain->cameraPos) * -9.8;
			scene->camera.gravityVector = (glm::transpose(inverse(glm::mat3(scene->camera.viewMatrix))) * glm::normalize(terrain->cameraPos)) * -9.8f;
			
			
			for (auto* chunk : terrain->chunks) {
				RefreshChunk(chunk);
			}
			
			
		}
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
		// // for each planet
		// 	if (terrain) {
		// 		scene->gravityVector = glm::normalize(terrain->cameraPos) * -9.8;
		// 		std::lock_guard lock(terrain->chunksMutex);
		// 		for (auto* chunk : terrain->chunks) {
		// 			RefreshChunk(chunk);
		// 		}
		// 	}
		// //
	}
	
	V4D_MODULE_FUNC(void, DrawUi2) {
		#ifdef _ENABLE_IMGUI
			// for each planet
				ImGui::Separator();
				ImGui::Text("Planet");
				if (terrain) {
					// ImGui::Separator();
					// ImGui::Text("Atmosphere");
					// if (ImGui::SliderFloat("density", &terrain->atmosphereDensityFactor, 0.0f, 125.0f) // BAR
					// ||
					// ImGui::ColorEdit3("color", (float*)&terrain->atmosphereColor)
					// ) {
					// 	if (auto atmosphereEntity = terrain->atmosphere.lock(); atmosphereEntity) {
					// 		if (auto atmosphereColor = atmosphereEntity->meshVertexColorF32.Lock(); atmosphereColor && atmosphereColor->data) {
					// 			atmosphereColor->data->r = terrain->atmosphereColor.r;
					// 			atmosphereColor->data->g = terrain->atmosphereColor.g;
					// 			atmosphereColor->data->b = terrain->atmosphereColor.b;
					// 			atmosphereColor->data->a = terrain->atmosphereDensityFactor;
					// 			atmosphereColor->dirtyOnDevice = true;
					// 		}
					// 	}
					// }
					ImGui::Separator();
					// ImGui::Checkbox("Voxel Terrain", &PlanetTerrain::generateAabbChunks);
					// ImGui::SliderFloat("Terrain detail level", &PlanetTerrain::chunkSubdivisionDistanceFactor, 0.5f, 5.0f);
					// ImGui::Checkbox("Automatic terrain detail level", &automaticChunkSubdivisionDistanceFactor);
					ImGui::SliderFloat("Terrain near detail resolution", &PlanetTerrain::targetVertexSeparationInMeters, 0.02f, 0.5f);
					
					ImGui::Separator();
					ImGui::Text("Sun1");
					if (sun && sun->generated) {
						auto lightSource = sun->lightSource.Lock();
						if (lightSource) {
							static glm::vec3 sunPosition = glm::normalize(sun1Position);
							static double sunDistance = glm::distance(sun1Position, terrain->absolutePosition);
							static float intensity = std::log10(lightSource->intensity);
							static glm::vec3 color = lightSource->color;
							float* pos = (float*)&sunPosition;
							ImGui::SliderFloat("Intensity1", &intensity, 20, 27);
							ImGui::SliderFloat3("Position1", pos, -1, 1);
							ImGui::ColorEdit3("Color1", (float*)&color);
							lightSource->color = color;
							lightSource->intensity = std::pow(10.0f, intensity);
							sun->SetWorldTransform(glm::translate(glm::dmat4(1), terrain->absolutePosition + glm::normalize(glm::dvec3(sunPosition)) * sunDistance));
						}
					}
					
					ImGui::Separator();
					ImGui::Text("Sun2");
					if (sun2 && sun2->generated) {
						auto lightSource = sun2->lightSource.Lock();
						if (lightSource) {
							static glm::vec3 sunPosition2 = glm::normalize(sun2Position);
							static double sunDistance2 = glm::distance(sun2Position, terrain->absolutePosition);
							static float intensity2 = std::log10(lightSource->intensity);
							static glm::vec3 color2 = lightSource->color;
							float* pos2 = (float*)&sunPosition2;
							ImGui::SliderFloat("Intensity2", &intensity2, 20, 27);
							ImGui::SliderFloat3("Position2", pos2, -1, 1);
							ImGui::ColorEdit3("Color2", (float*)&color2);
							lightSource->color = color2;
							lightSource->intensity = std::pow(10.0f, intensity2);
							sun2->SetWorldTransform(glm::translate(glm::dmat4(1), terrain->absolutePosition + glm::normalize(glm::dvec3(sunPosition2)) * sunDistance2));
						}
					}
					
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
					if (terrain->totalChunkTimeNb) {
						ImGui::Text("AvgChunkTime: %d ms", (int)std::round(float(terrain->totalChunkTime)/terrain->totalChunkTimeNb));
					}
				}
			#endif
		// #endif
	}
	
	V4D_MODULE_FUNC(void, InitVulkanLayouts) {
		auto* rayTracingPipelineLayout = mainRenderModule->GetPipelineLayout("pl_rendering");
		auto* fogPipelineLayout = mainRenderModule->GetPipelineLayout("pl_fog_raster");
		
		r->descriptorSets["mapsGen"] = &mapsGenDescriptorSet;
		r->descriptorSets["mapsSampler"] = &mapsSamplerDescriptorSet;
		planetsMapGenLayout.AddDescriptorSet(r->descriptorSets["set0"]);
		planetsMapGenLayout.AddDescriptorSet(&mapsGenDescriptorSet);
		planetsMapGenLayout.AddPushConstant<MapGenPushConstant>(VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet.AddBinding_imageView_array(0, bumpMaps, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsSamplerDescriptorSet.AddBinding_combinedImageSampler_array(0, bumpMaps, 1, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		
		rayTracingPipelineLayout->AddDescriptorSet(&mapsSamplerDescriptorSet);
	}
	
	V4D_MODULE_FUNC(void, ConfigureShaders) {
		mainRenderModule->AddRayTracingHitShader(THIS_MODULE, "planet.terrain");
		mainRenderModule->AddRayTracingHitShader(THIS_MODULE, "atmosphere");
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
		planetsMapGenLayout.Create(renderingDevice);
		bumpMapsAltitudeGen.CreatePipeline(renderingDevice);
		bumpMapsNormalsGen.CreatePipeline(renderingDevice);
	}
	
	V4D_MODULE_FUNC(void, DestroyVulkanPipelines) {
		bumpMapsAltitudeGen.DestroyPipeline(renderingDevice);
		bumpMapsNormalsGen.DestroyPipeline(renderingDevice);
		planetsMapGenLayout.Destroy(renderingDevice);
	}
	
	V4D_MODULE_FUNC(void, SecondaryRenderUpdate2, VkCommandBuffer cmdBuffer) {
		if (!bumpMapsGenerated) {
			
			bumpMapsAltitudeGen.SetGroupCounts(bumpMaps[0].width, bumpMaps[0].height, bumpMaps[0].arrayLayers);
			bumpMapsAltitudeGen.Execute(renderingDevice, cmdBuffer);
			
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
				cmdBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
	
			bumpMapsNormalsGen.SetGroupCounts(bumpMaps[0].width, bumpMaps[0].height, bumpMaps[0].arrayLayers);
			bumpMapsNormalsGen.Execute(renderingDevice, cmdBuffer);
				
			bumpMapsGenerated = true;
		}
	}
	
};
