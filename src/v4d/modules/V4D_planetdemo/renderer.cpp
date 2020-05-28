#include "common.hh"

using namespace v4d::scene;
using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::vulkan::rtx;

#pragma region Graphics objects

PipelineLayout planetsMapGenLayout;

ComputeShaderPipeline 
	bumpMapsAltitudeGen{planetsMapGenLayout, "modules/V4D_planetdemo/assets/shaders/planets.bump.altitude.map.comp"},
	bumpMapsNormalsGen{planetsMapGenLayout, "modules/V4D_planetdemo/assets/shaders/planets.bump.normals.map.comp"}
;

PlanetAtmosphereShaderPipeline* planetAtmosphereShader = nullptr;
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

Renderer* r = nullptr;
Scene* scene = nullptr;
V4D_Renderer* mainRenderModule = nullptr;

V4D_MODULE_CLASS(V4D_Renderer) {
	
	V4D_MODULE_FUNC(void, ModuleUnload) {
		if (planetAtmosphereShader) delete planetAtmosphereShader;
		if (planetTerrainRasterShader) delete planetTerrainRasterShader;
	}
	
	V4D_MODULE_FUNC(void, Init, Renderer* _r, Scene* _s) {
		r = _r;
		scene = _s;
		mainRenderModule = V4D_Renderer::GetPrimaryModule();
	}
	
	V4D_MODULE_FUNC(void, InitDeviceFeatures) {
		r->deviceFeatures.shaderStorageImageWriteWithoutFormat = VK_TRUE;
	}
	
	V4D_MODULE_FUNC(void, InitLayouts) {
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
		V4D_Game::LoadModule(THIS_MODULE)->ModuleSetCustomPtr(ATMOSPHERE_SHADER, planetAtmosphereShader);
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
	V4D_MODULE_FUNC(void, CreateResources) {
		// Bump Maps
		bumpMaps[0].samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		bumpMaps[0].Create(r->renderingDevice, 4096);
		
		auto cmdBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("transfer"));
			r->TransitionImageLayout(cmdBuffer, bumpMaps[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("transfer"), cmdBuffer);
	}
	
	V4D_MODULE_FUNC(void, DestroyResources) {
		bumpMaps[0].Destroy(r->renderingDevice);
		bumpMapsGenerated = false;
	}
	
	V4D_MODULE_FUNC(void, AllocateBuffers) {
		
	}
	
	V4D_MODULE_FUNC(void, FreeBuffers) {
		
	}
	
	V4D_MODULE_FUNC(void, CreatePipelines) {
		planetsMapGenLayout.Create(r->renderingDevice);
		
		bumpMapsAltitudeGen.CreatePipeline(r->renderingDevice);
		bumpMapsNormalsGen.CreatePipeline(r->renderingDevice);
	}
	
	V4D_MODULE_FUNC(void, DestroyPipelines) {
		bumpMapsAltitudeGen.DestroyPipeline(r->renderingDevice);
		bumpMapsNormalsGen.DestroyPipeline(r->renderingDevice);
		planetsMapGenLayout.Destroy(r->renderingDevice);
	}
	
	V4D_MODULE_FUNC(void, Render2, VkCommandBuffer cmdBuffer) {
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
