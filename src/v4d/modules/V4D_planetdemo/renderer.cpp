#include "common.hh"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::vulkan::rtx;

#pragma region Graphics objects

PipelineLayout planetsMapGenLayout;

ComputeShaderPipeline 
	bumpMapsAltitudeGen{planetsMapGenLayout, "modules/V4D_planetdemo/assets/shaders/planets.bump.altitude.map.comp"},
	bumpMapsNormalsGen{planetsMapGenLayout, "modules/V4D_planetdemo/assets/shaders/planets.bump.normals.map.comp"}
	// ,mantleMapGen{planetsMapGenLayout, "modules/V4D_planetdemo/assets/shaders/planets.mantle.map.comp"}
	// ,tectonicsMapGen{planetsMapGenLayout, "modules/V4D_planetdemo/assets/shaders/planets.tectonics.map.comp"}
	// ,heightMapGen{planetsMapGenLayout, "modules/V4D_planetdemo/assets/shaders/planets.height.map.comp"}
	// ,volcanoesMapGen{planetsMapGenLayout, "modules/V4D_planetdemo/assets/shaders/planets.volcanoes.map.comp"}
	// ,liquidsMapGen{planetsMapGenLayout, "modules/V4D_planetdemo/assets/shaders/planets.liquids.map.comp"}
;

PlanetAtmosphereShaderPipeline* planetAtmosphereShader = nullptr;

struct MapGenPushConstant {
	int planetIndex;
	float planetHeightVariation;
} mapGenPushConstant;

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

Renderer* r = nullptr;
Scene* scene = nullptr;
V4D_Renderer* mainRenderModule = nullptr;

extern "C" {
	
	void Init(Renderer* _r, Scene* _s) {
		r = _r;
		scene = _s;
		mainRenderModule = V4D_Renderer::GetPrimaryModule();
	}
	
	void InitDeviceFeatures() {
		r->deviceFeatures.shaderStorageImageWriteWithoutFormat = VK_TRUE;
	}
	
	void InitLayouts() {
		auto* rayTracingPipelineLayout = mainRenderModule->GetPipelineLayout("rayTracing");
		auto* lightingPipelineLayout = mainRenderModule->GetPipelineLayout("lighting");
		
		r->descriptorSets["mapsGen"] = &mapsGenDescriptorSet;
		// r->descriptorSets["planets"] = &planetsDescriptorSet;
		r->descriptorSets["mapsSampler"] = &mapsSamplerDescriptorSet;
		planetsMapGenLayout.AddDescriptorSet(r->descriptorSets["0_base"]);
		planetsMapGenLayout.AddDescriptorSet(&mapsGenDescriptorSet);
		planetsMapGenLayout.AddPushConstant<MapGenPushConstant>(VK_SHADER_STAGE_COMPUTE_BIT);
		rayTracingPipelineLayout->AddDescriptorSet(&mapsSamplerDescriptorSet);
		// rayTracingPipelineLayout->AddDescriptorSet(&planetsDescriptorSet);
		
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
		
		// terrainVertexComputeLayout.AddDescriptorSet(r->descriptorSets["0_base"]);
		// terrainVertexComputeLayout.AddDescriptorSet(r->descriptorSets["1_rayTracing"]);
		// terrainVertexComputeLayout.AddDescriptorSet(&mapsSamplerDescriptorSet);
		// // terrainVertexComputeLayout.AddDescriptorSet(planetsDescriptorSet);
		// terrainVertexComputeLayout.AddPushConstant<TerrainChunkPushConstant>(VK_SHADER_STAGE_COMPUTE_BIT);
		
		planetAtmosphereShader = new PlanetAtmosphereShaderPipeline {*lightingPipelineLayout, {
			"modules/V4D_planetdemo/assets/shaders/planetAtmosphere.vert",
			"modules/V4D_planetdemo/assets/shaders/planetAtmosphere.frag",
		}};
		planetAtmosphereShader->camera = &scene->camera;
		planetAtmosphereShader->atmospherePushConstantIndex = lightingPipelineLayout->AddPushConstant<PlanetAtmosphereShaderPipeline::PlanetAtmospherePushConstant>(VK_SHADER_STAGE_ALL_GRAPHICS);
		V4D_Game::LoadModule(THIS_MODULE)->ModuleSetCustomPtr(ATMOSPHERE_SHADER, planetAtmosphereShader);
		mainRenderModule->GetShaderGroup("lighting_1").push_back(planetAtmosphereShader);
	}
	
	void ConfigureShaders() {
		auto* shaderBindingTable = mainRenderModule->GetShaderBindingTable();
		
		// Geometry::rayTracingShaderOffsets["planet_raymarching"] = shaderBindingTable->AddHitShader("modules/V4D_planetdemo/assets/shaders/planets.raymarching.rchit", "", "modules/V4D_planetdemo/assets/shaders/planets.raymarching.rint");
		Geometry::rayTracingShaderOffsets["planet_terrain"] = shaderBindingTable->AddHitShader("modules/V4D_planetdemo/assets/shaders/planets.terrain.rchit");
		
		// Atmosphere
		planetAtmosphereShader->AddVertexInputBinding(sizeof(PlanetAtmosphere::Vertex), VK_VERTEX_INPUT_RATE_VERTEX, PlanetAtmosphere::Vertex::GetInputAttributes());
		// planetAtmosphereShader->depthStencilState.depthTestEnable = VK_FALSE;
		// planetAtmosphereShader->depthStencilState.depthWriteEnable = VK_FALSE;
		// planetAtmosphereShader->rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
		#ifdef PLANETARY_ATMOSPHERE_MESH_USE_TRIANGLE_STRIPS
			planetAtmosphereShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			planetAtmosphereShader->inputAssembly.primitiveRestartEnable = VK_TRUE;
		#else
			planetAtmosphereShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		#endif
	}
	
	void ReadShaders() {
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
	void CreateResources() {
		// Bump Maps
		bumpMaps[0].samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].Create(r->renderingDevice, 4096);
		
		auto cmdBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("transfer"));
			r->TransitionImageLayout(cmdBuffer, bumpMaps[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("transfer"), cmdBuffer);
		
		// bumpMaps[1].Create(r->renderingDevice, 4096);
		// r->TransitionImageLayout(bumpMaps[1], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		
		// planet.CreateMaps(r->renderingDevice);
		
		// r->TransitionImageLayout(planet.mantleMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		// r->TransitionImageLayout(planet.tectonicsMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		// r->TransitionImageLayout(planet.heightMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		// r->TransitionImageLayout(planet.volcanoesMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		// r->TransitionImageLayout(planet.liquidsMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		
		// // Set Maps
		// mantleMaps[0] = planet.mantleMap;
		// tectonicsMaps[0] = planet.tectonicsMap;
		// heightMaps[0] = planet.heightMap;
		// volcanoesMaps[0] = planet.volcanoesMap;
		// liquidsMaps[0] = planet.liquidsMap;
	}
	
	void DestroyResources() {
		// planet.DestroyMaps(r->renderingDevice);
		bumpMaps[0].Destroy(r->renderingDevice);
		// bumpMaps[1].Destroy(r->renderingDevice);
		bumpMapsGenerated = false;
	}
	
	void AllocateBuffers() {
		// planetsBuffer.Allocate(r->renderingDevice);
	}
	
	void FreeBuffers() {
		// planetsBuffer.Free(r->renderingDevice);
	}
	
	void CreatePipelines() {
		planetsMapGenLayout.Create(r->renderingDevice);
		// terrainVertexComputeLayout.Create(r->renderingDevice);
		
		bumpMapsAltitudeGen.CreatePipeline(r->renderingDevice);
		bumpMapsNormalsGen.CreatePipeline(r->renderingDevice);
		
		// mantleMapGen.CreatePipeline(r->renderingDevice);
		// tectonicsMapGen.CreatePipeline(r->renderingDevice);
		// heightMapGen.CreatePipeline(r->renderingDevice);
		// volcanoesMapGen.CreatePipeline(r->renderingDevice);
		// liquidsMapGen.CreatePipeline(r->renderingDevice);
		
		// terrainVertexPosCompute.CreatePipeline(r->renderingDevice);
		// terrainVertexNormalCompute.CreatePipeline(r->renderingDevice);
	}
	
	void DestroyPipelines() {
		bumpMapsAltitudeGen.DestroyPipeline(r->renderingDevice);
		bumpMapsNormalsGen.DestroyPipeline(r->renderingDevice);
		
		// mantleMapGen.DestroyPipeline(r->renderingDevice);
		// tectonicsMapGen.DestroyPipeline(r->renderingDevice);
		// heightMapGen.DestroyPipeline(r->renderingDevice);
		// volcanoesMapGen.DestroyPipeline(r->renderingDevice);
		// liquidsMapGen.DestroyPipeline(r->renderingDevice);
		
		// terrainVertexPosCompute.DestroyPipeline(r->renderingDevice);
		// terrainVertexNormalCompute.DestroyPipeline(r->renderingDevice);

		// terrainVertexComputeLayout.Destroy(r->renderingDevice);
		planetsMapGenLayout.Destroy(r->renderingDevice);
	}
	
	void Render2() {
		if (!bumpMapsGenerated) {
			auto cmdBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("secondary"));
				
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
				
			r->EndSingleTimeCommands(r->renderingDevice->GetQueue("secondary"), cmdBuffer);
			bumpMapsGenerated = true;
		}
	}
	
	
	
}
