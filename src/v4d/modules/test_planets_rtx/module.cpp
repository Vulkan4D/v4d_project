// Metadata
#define _V4D_MODULE
#define THIS_MODULE_VENDOR_ID 16
#define THIS_MODULE_ID 1002
#define THIS_MODULE_REV 1
#define THIS_MODULE_NAME "test_planets_rtx"
#define THIS_MODULE_DESCRIPTION "test_planets_rtx"

// V4D Core Header
#include <v4d.h>

#include "PlanetsRenderer/PlanetTerrain.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan::rtx;

#pragma region Graphics

PipelineLayout planetsMapGenLayout;

ComputeShaderPipeline 
	bumpMapsAltitudeGen{planetsMapGenLayout, "modules/test_planets_rtx/assets/shaders/planets.bump.altitude.map.comp"},
	bumpMapsNormalsGen{planetsMapGenLayout, "modules/test_planets_rtx/assets/shaders/planets.bump.normals.map.comp"},
	mantleMapGen{planetsMapGenLayout, "modules/test_planets_rtx/assets/shaders/planets.mantle.map.comp"},
	tectonicsMapGen{planetsMapGenLayout, "modules/test_planets_rtx/assets/shaders/planets.tectonics.map.comp"},
	heightMapGen{planetsMapGenLayout, "modules/test_planets_rtx/assets/shaders/planets.height.map.comp"},
	volcanoesMapGen{planetsMapGenLayout, "modules/test_planets_rtx/assets/shaders/planets.volcanoes.map.comp"},
	liquidsMapGen{planetsMapGenLayout, "modules/test_planets_rtx/assets/shaders/planets.liquids.map.comp"}
;

struct MapGenPushConstant {
	int planetIndex;
	float planetHeightVariation;
} mapGenPushConstant;

struct TerrainChunkPushConstant {
	int planetIndex;
	int chunkGeometryOffset;
	float solidRadius;
	int vertexSubdivisionsPerChunk;
	glm::vec2 uvMult;
	glm::vec2 uvOffset;
	alignas(16) glm::ivec3 chunkPosition;
	alignas(4) int face;
} terrainChunkPushConstant;

DescriptorSet *mapsGenDescriptorSet, *mapsSamplerDescriptorSet, *planetsDescriptorSet;

#define MAX_PLANETS 1

Image mantleMaps[MAX_PLANETS];
Image tectonicsMaps[MAX_PLANETS];
Image heightMaps[MAX_PLANETS];
Image volcanoesMaps[MAX_PLANETS];
Image liquidsMaps[MAX_PLANETS];

Image bumpMaps[1] {// xyz=normal, a=altitude
	Image { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 1, 1, {VK_FORMAT_R32G32B32A32_SFLOAT}},
	// Image { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 1, 1, {VK_FORMAT_R32G32B32A32_SFLOAT}},
};
bool bumpMapsGenerated = false;

struct PlanetBuffer {
	// glm::dmat4 viewToPlanetPosMatrix {1};
	alignas(16) glm::vec3 northDir;
};
StagedBuffer planetsBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(PlanetBuffer)*MAX_PLANETS};

#pragma endregion

struct Planet {
	double solidRadius = 8000000;
	double atmosphereRadius = 8400000;
	double heightVariation = 40000;
	
	#pragma region cache
	
	bool mapsGenerated = false;
	
	#pragma endregion
	
	#pragma region Maps
	
	CubeMapImage mantleMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}}; // platesDir, mantleHeightFactor, surfaceHeightFactor, hotSpots
	CubeMapImage tectonicsMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}}; // divergent, convergent, transform, density
	CubeMapImage heightMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32_SFLOAT}}; // variation from surface radius, in meters, rounded (range -32k, +32k)
	CubeMapImage volcanoesMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R8_UNORM}}; // volcanoesMap
	CubeMapImage liquidsMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R8_UNORM}}; // liquidMap
	
	// temperature k, radiation rad, moisture norm, wind m/s
	CubeMapImage weatherMapAvg { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	CubeMapImage weatherMapMinimum { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	CubeMapImage weatherMapMaximum { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	CubeMapImage weatherMapCurrent { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	
	void CreateMaps(Device* device, double scale = 1.0) {
		int mapSize = std::max(64, int(scale * std::min(8000000.0, solidRadius) / 2000.0 * 3.141592654)); // 1 km / pixel (considering maximum radius of 8000km)
		// max image width : 12566
		mantleMap.Create(device, mapSize/16); // max 785 px = 57 MB
		tectonicsMap.Create(device, mapSize/8); // max 1570 px = 226 MB
		heightMap.Create(device, mapSize/2); // max 6283 px = 3614 MB
		volcanoesMap.Create(device, mapSize/4); // max 3141 px = 57 MB
		liquidsMap.Create(device, mapSize/4); // max 3141 px = 57 MB
		int weatherMapSize = std::max(8, int(mapSize / 100)); // max 125 px = 1.5 MB x4
		weatherMapAvg.Create(device, weatherMapSize);
		weatherMapMinimum.Create(device, weatherMapSize);
		weatherMapMaximum.Create(device, weatherMapSize);
		weatherMapCurrent.Create(device, weatherMapSize);
	}
	
	void DestroyMaps(Device* device) {
		mantleMap.Destroy(device);
		tectonicsMap.Destroy(device);
		heightMap.Destroy(device);
		volcanoesMap.Destroy(device);
		liquidsMap.Destroy(device);
		weatherMapAvg.Destroy(device);
		weatherMapMinimum.Destroy(device);
		weatherMapMaximum.Destroy(device);
		weatherMapCurrent.Destroy(device);
		
		mapsGenerated = false;
	}
	
	void GenerateMaps(Device* device, VkCommandBuffer commandBuffer) {
		if (!mapsGenerated) {
			mapGenPushConstant.planetIndex = 0;
			mapGenPushConstant.planetHeightVariation = heightVariation;
			
			/*First Pass*/
		
			mantleMapGen.SetGroupCounts(mantleMap.width, mantleMap.height, mantleMap.arrayLayers);
			mantleMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
			
			tectonicsMapGen.SetGroupCounts(tectonicsMap.width, tectonicsMap.height, tectonicsMap.arrayLayers);
			tectonicsMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
			
			heightMapGen.SetGroupCounts(heightMap.width, heightMap.height, heightMap.arrayLayers);
			heightMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
			
			volcanoesMapGen.SetGroupCounts(volcanoesMap.width, volcanoesMap.height, volcanoesMap.arrayLayers);
			volcanoesMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
			
			liquidsMapGen.SetGroupCounts(liquidsMap.width, liquidsMap.height, liquidsMap.arrayLayers);
			liquidsMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
			
			// Need pipeline barrier before other passes
			
			
			mapsGenerated = true;
		
			VkMemoryBarrier barrier {};
				barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
				barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			device->CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
		}
	}
	
	#pragma endregion
	
} planet;

PlanetTerrain terrain {planet.atmosphereRadius, planet.solidRadius, planet.heightVariation, {0,planet.solidRadius*2,0}};

PipelineLayout terrainVertexComputeLayout;
ComputeShaderPipeline terrainVertexPosCompute {terrainVertexComputeLayout, "modules/test_planets_rtx/assets/shaders/planets.terrain.vertexpos.comp"};
ComputeShaderPipeline terrainVertexNormalCompute {terrainVertexComputeLayout, "modules/test_planets_rtx/assets/shaders/planets.terrain.vertexnormal.comp"};

void ComputeChunkVertices(Device* device, VkCommandBuffer commandBuffer, PlanetTerrain::Chunk* chunk) {
	std::scoped_lock lock(chunk->stateMutex);
	if (chunk->active) {
		
		// subChunks
		{
			std::scoped_lock lock(chunk->subChunksMutex);
			for (auto* subChunk : chunk->subChunks) {
				ComputeChunkVertices(device, commandBuffer, subChunk);
			}
		}
		
		if (chunk->render && chunk->obj && chunk->obj->IsGenerated()) {
			if (chunk->computedLevel == 0) {
				terrainChunkPushConstant.chunkGeometryOffset = chunk->obj->GetFirstGeometryOffset();
				terrainChunkPushConstant.chunkPosition = chunk->centerPos;
				terrainChunkPushConstant.face = chunk->face;
				terrainChunkPushConstant.uvMult = {chunk->uvMultX, chunk->uvMultY};
				terrainChunkPushConstant.uvOffset = {chunk->uvOffsetX, chunk->uvOffsetY};
				
				// Compute positions
				terrainVertexPosCompute.SetGroupCounts(PlanetTerrain::vertexSubdivisionsPerChunk+1, PlanetTerrain::vertexSubdivisionsPerChunk+1, 1);
				terrainVertexPosCompute.Execute(device, commandBuffer, 1, &terrainChunkPushConstant);
				
				VkBufferMemoryBarrier barrier {};
					barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
					barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
					barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_TRANSFER_READ_BIT;
					barrier.offset = chunk->obj->GetFirstGeometryVertexOffset() * sizeof(Geometry::VertexBuffer_T);
					barrier.size = PlanetTerrain::nbVerticesPerChunk * sizeof(Geometry::VertexBuffer_T);
					barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.buffer = Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer.buffer;
				device->CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
				
				// Compute normals
				terrainVertexNormalCompute.SetGroupCounts(PlanetTerrain::vertexSubdivisionsPerChunk+1, PlanetTerrain::vertexSubdivisionsPerChunk+1, 1);
				terrainVertexNormalCompute.Execute(device, commandBuffer, 1, &terrainChunkPushConstant);
				
				chunk->computedLevel++;
				chunk->obj->SetGeometriesDirty();
				
				// Pull some vertices
				chunk->obj->GetGeometries()[0]->Pull(device, commandBuffer, Geometry::GlobalGeometryBuffers::BUFFER_VERTEX);
			
			} else {
				chunk->RefreshVertices();
			}
		} else {
			chunk->RefreshVertices();
		}
	}
}

class Rendering : public v4d::modules::Rendering {
public:
	Rendering() {}
	int OrderIndex() const override {return 0;}
	
	// Init/Shaders
	void Init() override {
		
	}
	void InitLayouts(std::vector<DescriptorSet*>& descriptorSets, std::unordered_map<std::string, Image*>&, PipelineLayout* rayTracingPipelineLayout) override {
		
		mapsGenDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(1));
		planetsDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(2));
		mapsSamplerDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(3));
		planetsMapGenLayout.AddDescriptorSet(descriptorSets[0]);
		planetsMapGenLayout.AddDescriptorSet(mapsGenDescriptorSet);
		planetsMapGenLayout.AddPushConstant<MapGenPushConstant>(VK_SHADER_STAGE_COMPUTE_BIT);
		rayTracingPipelineLayout->AddDescriptorSet(mapsSamplerDescriptorSet);
		rayTracingPipelineLayout->AddDescriptorSet(planetsDescriptorSet);
		
		mapsGenDescriptorSet->AddBinding_imageView_array(0, bumpMaps, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet->AddBinding_imageView_array(1, mantleMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet->AddBinding_imageView_array(2, tectonicsMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet->AddBinding_imageView_array(3, heightMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet->AddBinding_imageView_array(4, volcanoesMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet->AddBinding_imageView_array(5, liquidsMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(0, bumpMaps, 1, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(1, mantleMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(2, tectonicsMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(3, heightMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(4, volcanoesMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(5, liquidsMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
		
		planetsDescriptorSet->AddBinding_storageBuffer(0, &planetsBuffer.deviceLocalBuffer, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
		
		terrainVertexComputeLayout.AddDescriptorSet(descriptorSets[0]);
		terrainVertexComputeLayout.AddDescriptorSet(descriptorSets[1]);
		terrainVertexComputeLayout.AddDescriptorSet(mapsSamplerDescriptorSet);
		// terrainVertexComputeLayout.AddDescriptorSet(planetsDescriptorSet);
		terrainVertexComputeLayout.AddPushConstant<TerrainChunkPushConstant>(VK_SHADER_STAGE_COMPUTE_BIT);
	}
	void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders, ShaderBindingTable* shaderBindingTable) override {
		// Geometry::rayTracingShaderOffsets["planet_raymarching"] = shaderBindingTable->AddHitShader("modules/test_planets_rtx/assets/shaders/planets.raymarching.rchit", "", "modules/test_planets_rtx/assets/shaders/planets.raymarching.rint");
		Geometry::rayTracingShaderOffsets["planet_terrain"] = shaderBindingTable->AddHitShader("modules/test_planets_rtx/assets/shaders/planets.terrain.rchit");
		
	}
	void ReadShaders() override {
		bumpMapsAltitudeGen.ReadShaders();
		bumpMapsNormalsGen.ReadShaders();
		mantleMapGen.ReadShaders();
		tectonicsMapGen.ReadShaders();
		heightMapGen.ReadShaders();
		volcanoesMapGen.ReadShaders();
		liquidsMapGen.ReadShaders();
		terrainVertexPosCompute.ReadShaders();
		terrainVertexNormalCompute.ReadShaders();
	}
	
	// Images / Buffers / Pipelines
	void CreateResources() override {
		// Bump Maps
		bumpMaps[0].samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].Create(renderingDevice, 2048);
		// bumpMaps[1].Create(renderingDevice, 2048);
		renderer->TransitionImageLayout(bumpMaps[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		// renderer->TransitionImageLayout(bumpMaps[1], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		
		planet.CreateMaps(renderingDevice);
		
		renderer->TransitionImageLayout(planet.mantleMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		renderer->TransitionImageLayout(planet.tectonicsMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		renderer->TransitionImageLayout(planet.heightMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		renderer->TransitionImageLayout(planet.volcanoesMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		renderer->TransitionImageLayout(planet.liquidsMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		
		// Set Maps
		mantleMaps[0] = planet.mantleMap;
		tectonicsMaps[0] = planet.tectonicsMap;
		heightMaps[0] = planet.heightMap;
		volcanoesMaps[0] = planet.volcanoesMap;
		liquidsMaps[0] = planet.liquidsMap;
		
	}
	void DestroyResources() override {
		planet.DestroyMaps(renderingDevice);
		bumpMaps[0].Destroy(renderingDevice);
		// bumpMaps[1].Destroy(renderingDevice);
		bumpMapsGenerated = false;
	}
	void AllocateBuffers() override {
		planetsBuffer.Allocate(renderingDevice);
	}
	void FreeBuffers() override {
		planetsBuffer.Free(renderingDevice);
	}
	void CreatePipelines(std::unordered_map<std::string, Image*>&) override {
		planetsMapGenLayout.Create(renderingDevice);
		terrainVertexComputeLayout.Create(renderingDevice);
		
		bumpMapsAltitudeGen.CreatePipeline(renderingDevice);
		bumpMapsNormalsGen.CreatePipeline(renderingDevice);
		mantleMapGen.CreatePipeline(renderingDevice);
		tectonicsMapGen.CreatePipeline(renderingDevice);
		heightMapGen.CreatePipeline(renderingDevice);
		volcanoesMapGen.CreatePipeline(renderingDevice);
		liquidsMapGen.CreatePipeline(renderingDevice);
		terrainVertexPosCompute.CreatePipeline(renderingDevice);
		terrainVertexNormalCompute.CreatePipeline(renderingDevice);
	}
	void DestroyPipelines() override {
		bumpMapsAltitudeGen.DestroyPipeline(renderingDevice);
		bumpMapsNormalsGen.DestroyPipeline(renderingDevice);
		mantleMapGen.DestroyPipeline(renderingDevice);
		tectonicsMapGen.DestroyPipeline(renderingDevice);
		heightMapGen.DestroyPipeline(renderingDevice);
		volcanoesMapGen.DestroyPipeline(renderingDevice);
		liquidsMapGen.DestroyPipeline(renderingDevice);
		terrainVertexPosCompute.DestroyPipeline(renderingDevice);
		terrainVertexNormalCompute.DestroyPipeline(renderingDevice);

		terrainVertexComputeLayout.Destroy(renderingDevice);
		planetsMapGenLayout.Destroy(renderingDevice);
	}
	
	// Scene
	void LoadScene(Scene& scene) override {
		
		// Light source
		scene.AddObjectInstance("light")->Configure([](ObjectInstance* obj){
			obj->SetSphereLightSource(700000000, 1e24f);
		}, {10,-1.496e+11,300});
				
						// // Planet
						// scene.objectInstances.emplace_back(new ObjectInstance("planet_raymarching"))->Configure([](ObjectInstance* obj){
						// 	obj->SetSphereGeometry((float)planet.solidRadius, {1,0,0, 1}, 0/*planet index*/);
						// }, {0,planet.solidRadius*2,0});
				
		terrain.scene = &scene;
		
	}
	
	void UnloadScene(Scene&) override {
		terrain.scene = nullptr;
	}

	// Frame Update
	void FrameUpdate(Scene& scene) override {
		
						// // // for each planet
						// 	int planetIndex = 0;
						// 	auto* planetBuffer = &((PlanetBuffer*)(planetsBuffer.stagingBuffer.data))[planetIndex];
						// // 	planetBuffer->viewToPlanetPosMatrix = glm::inverse(scene.camera.viewMatrix * scene.objectInstances[1]->GetWorldTransform());
						// 	planetBuffer->northDir = glm::normalize(glm::transpose(glm::inverse(glm::dmat3(terrain.matrix))) * glm::dvec3(0,1,0));
						// // //
						// auto cmdBuffer = renderer->BeginSingleTimeCommands(*graphicsQueue);
						// planetsBuffer.Update(renderingDevice, cmdBuffer);
						// renderer->EndSingleTimeCommands(*graphicsQueue, cmdBuffer);
				
		
		
		// for each planet
			std::lock_guard lock(terrain.chunksMutex);
			
			// //TODO Planet rotation
			// static v4d::Timer time(true);
			// // terrain.rotationAngle = time.GetElapsedSeconds()/1000000000;
			// // terrain.rotationAngle = time.GetElapsedSeconds()/30;
			// terrain.rotationAngle += 0.0001;
			
			terrain.RefreshMatrix();
			
			// Camera position relative to planet
			terrain.cameraPos = glm::inverse(terrain.matrix) * glm::dvec4(scene.camera.worldPosition, 1);
			terrain.cameraAltitudeAboveTerrain = glm::length(terrain.cameraPos) - terrain.GetHeightMap(glm::normalize(terrain.cameraPos), 0.5);
			
			for (auto* chunk : terrain.chunks) {
				chunk->BeforeRender();
			}
			
		//
	}
	void LowPriorityFrameUpdate() override {
		// for each planet
			// terrain.Optimize();
		//
		// PlanetTerrain::CollectGarbage(renderingDevice);
	}
	
	// Render frame
	void RunDynamicGraphicsTop(VkCommandBuffer commandBuffer, std::unordered_map<std::string, Image*>&) {
		
	}
	void RunDynamicLowPriorityCompute(VkCommandBuffer commandBuffer) override {
		if (!bumpMapsGenerated) {
			bumpMapsAltitudeGen.SetGroupCounts(bumpMaps[0].width, bumpMaps[0].height, bumpMaps[0].arrayLayers);
			bumpMapsAltitudeGen.Execute(renderingDevice, commandBuffer);
			
			// VkImageMemoryBarrier barrier = {};
			// 	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			// 	barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			// 	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			// 	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			// 	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			// 	barrier.image = bumpMaps[0].image;
			// 	barrier.subresourceRange.baseMipLevel = 0;
			// 	barrier.subresourceRange.levelCount = 1;
			// 	barrier.subresourceRange.baseArrayLayer = 0;
			// 	barrier.subresourceRange.layerCount = 1;
			// 	barrier.srcAccessMask = 0;
			// 	barrier.dstAccessMask = 0;
			// 	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			// 	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			// 	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			// renderingDevice->CmdPipelineBarrier(
			// 	commandBuffer,
			// 	VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			// 	0,
			// 	0, nullptr,
			// 	0, nullptr,
			// 	1, &barrier
			// );
	
			bumpMapsNormalsGen.SetGroupCounts(bumpMaps[0].width, bumpMaps[0].height, bumpMaps[0].arrayLayers);
			bumpMapsNormalsGen.Execute(renderingDevice, commandBuffer);
			bumpMapsGenerated = true;
		}
		// for each planet
			planet.GenerateMaps(renderingDevice, commandBuffer);
			terrainChunkPushConstant.planetIndex = 0;
			terrainChunkPushConstant.solidRadius = float(terrain.solidRadius);
			terrainChunkPushConstant.vertexSubdivisionsPerChunk = PlanetTerrain::vertexSubdivisionsPerChunk;
			std::lock_guard lock(terrain.chunksMutex);
			for (auto* chunk : terrain.chunks) {
				ComputeChunkVertices(renderingDevice, commandBuffer, chunk);
				chunk->Process(); // need to process after compute, because we will compute on next lowPriority frame, because otherwise the computed geometries get overridden by the ones in the staging buffer
			}
		//
	}
	void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {
		
	}

	#ifdef _ENABLE_IMGUI
		void RunImGui() override {
			
		}
	#endif
};

V4DMODULE void V4D_ModuleCreate() {
	V4D_LOAD_SUBMODULE(Rendering)
}

V4DMODULE void V4D_ModuleDestroy() {
	V4D_UNLOAD_SUBMODULES(Rendering)
}
