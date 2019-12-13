#pragma once
#include <v4d.h>
#include "V4DRenderingPipeline.hh"
#include "Camera.hpp"

using namespace v4d::graphics;

class V4DRenderer : public v4d::graphics::Renderer {
	using v4d::graphics::Renderer::Renderer;
	
	// Graphics configuration
	float uiImageScale = 1.0;
	
	// Buffers
	Buffer viewUniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(ViewUBO), true};
	Buffer galaxyUniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(GalaxyUBO), true};
	std::vector<Buffer*> stagedBuffers {};
	
	// Galaxy rendering
	CubeMapImage galaxyCubeMapImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
	CubeMapImage galaxyDepthStencilCubeMapImage { VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT , { VK_FORMAT_D32_SFLOAT_S8_UINT }};
	RenderPass galaxyGenRenderPass, galaxyFadeRenderPass;
	PipelineLayout galaxyGenPipelineLayout, galaxyBoxPipelineLayout;
	RasterShaderPipeline galaxyGenShader {galaxyGenPipelineLayout, {
		"incubator_rendering/assets/shaders/galaxy.gen.geom",
		"incubator_rendering/assets/shaders/galaxy.gen.vert",
		"incubator_rendering/assets/shaders/galaxy.gen.frag",
	}};
	RasterShaderPipeline galaxyBoxShader {galaxyBoxPipelineLayout, {
		"incubator_rendering/assets/shaders/galaxy.box.vert",
		"incubator_rendering/assets/shaders/galaxy.box.frag",
	}};
	RasterShaderPipeline galaxyFadeShader {galaxyBoxPipelineLayout, {
		"incubator_rendering/assets/shaders/galaxy.fade.vert",
		"incubator_rendering/assets/shaders/galaxy.fade.geom",
		"incubator_rendering/assets/shaders/galaxy.fade.frag",
	}};
	
	
	
	// Temporary galaxy stuff
	struct Galaxy {
		glm::vec4 posr;
		int seed;
		int numStars;
	};
	std::vector<Galaxy> galaxies {};
	uint RandomInt(uint& seed) {
		// LCG values from Numerical Recipes
		return (seed = 1664525 * seed + 1013904223);
	}
	float RandomFloat(uint& seed) {
		// Float version using bitmask from Numerical Recipes
		const uint one = 0x3f800000;
		const uint msk = 0x007fffff;
		return glm::uintBitsToFloat(one | (msk & (RandomInt(seed) >> 9))) - 1;
	}
	glm::vec3 RandomInUnitSphere(uint& seed) {
		for (;;) {
			glm::vec3 p = 2.0f * glm::vec3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed)) - 1.0f;
			if (dot(p, p) < 1) return p;
		}
	}
	
	
	
	// UI
	Image uiImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8G8B8_SNORM, VK_FORMAT_R8G8B8A8_SNORM }};
	
	// PipelineLayout testLayout;
	// RasterShaderPipeline* testShader;
	
public: // Camera
	Camera mainCamera;
	
private: // Init
	void ScorePhysicalDeviceSelection(int& score, PhysicalDevice* physicalDevice) override {}
	void Init() override {}
	void Info() override {}

private: // Resources

	void CreateResources() override {
		float screenWidth = (float)swapChain->extent.width;
		float screenHeight = (float)swapChain->extent.height;
		uint bgSize = (uint)std::max(screenWidth, screenHeight);
		uint uiWidth = (uint)(screenWidth * uiImageScale);
		uint uiHeight = (uint)(screenHeight * uiImageScale);
		
		// Create images
		galaxyCubeMapImage.Create(renderingDevice, bgSize, bgSize);
		galaxyDepthStencilCubeMapImage.viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		galaxyDepthStencilCubeMapImage.Create(renderingDevice, bgSize, bgSize);
		uiImage.Create(renderingDevice, uiWidth, uiHeight);
		mainCamera.SetRenderTarget(swapChain);
		mainCamera.CreateResources(renderingDevice);
	}
	
	void DestroyResources() override {
		// Destroy images
		galaxyCubeMapImage.Destroy(renderingDevice);
		galaxyDepthStencilCubeMapImage.Destroy(renderingDevice);
		uiImage.Destroy(renderingDevice);
		mainCamera.DestroyResources(renderingDevice);
	}
	
	void AllocateBuffers() override {}
	void FreeBuffers() override {}

private: // Graphics configuration
	void CreatePipelines() override {}
	void DestroyPipelines() override {}
	
private: // Commands
	void RecordComputeCommandBuffer(VkCommandBuffer, int imageIndex) override {}
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		TransitionImageLayout(commandBuffer, swapChain->images[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}
	void RecordLowPriorityComputeCommandBuffer(VkCommandBuffer) override {}
	void RecordLowPriorityGraphicsCommandBuffer(VkCommandBuffer) override {}
	void RunDynamicCompute(VkCommandBuffer) override {}
	void RunDynamicGraphics(VkCommandBuffer) override {}
	void RunDynamicLowPriorityCompute(VkCommandBuffer) override {}
	void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {}
	
public: // Scene configuration
	void LoadScene() override {
		
		// // Galaxy Gen (temporary stuff)
		// uint seed = 1;
		// for (int x = -64; x < 63; ++x) {
		// 	for (int y = -64; y < 63; ++y) {
		// 		for (int z = -8; z < 7; ++z) {
		// 			float centerFactor = 1.0f - glm::length(glm::vec3((float)x,(float)y,(float)z))/180.0f;
		// 			if (centerFactor > 0.0) {
		// 				auto offset = RandomInUnitSphere(seed)*2.84f;
		// 				galaxies.push_back({
		// 					{x*5+offset.x, y*5+offset.y, z*5+offset.z, glm::pow(centerFactor, 25)*10}, x*y*z*3+5, (int)(glm::pow(centerFactor, 8)*50.0f)
		// 				});
		// 			}
		// 		}
		// 	}
		// }
		// galaxyGenShader.AddVertexInputBinding(sizeof(Galaxy), VK_VERTEX_INPUT_RATE_VERTEX, {
		// 	{0, offsetof(Galaxy, Galaxy::posr), VK_FORMAT_R32G32B32A32_SFLOAT},
		// 	{1, offsetof(Galaxy, Galaxy::seed), VK_FORMAT_R32_UINT},
		// 	{2, offsetof(Galaxy, Galaxy::numStars), VK_FORMAT_R32_UINT},
		// });
		
		// // Galaxy Gen
		// auto* galaxyGenDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(0));
		// galaxyGenDescriptorSet->AddBinding_uniformBuffer(0, &viewUniformBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		// galaxyGenDescriptorSet->AddBinding_uniformBuffer(1, &galaxyUniformBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		// galaxyGenPipelineLayout.AddDescriptorSet(galaxyGenDescriptorSet);
		// Buffer* galaxiesBuffer = stagedBuffers.emplace_back(new Buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
		// galaxyGenShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		// galaxyGenShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
		// galaxyGenShader.depthStencilState.depthWriteEnable = VK_FALSE;
		// galaxyGenShader.depthStencilState.depthTestEnable = VK_FALSE;
		
		// // Galaxy Data
		// galaxiesBuffer->AddSrcDataPtr(&galaxies);
		// galaxyGenShader.SetData(galaxiesBuffer, galaxies.size());
		
		// // Galaxy Box
		// auto* galaxyBoxDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(0));
		// galaxyBoxDescriptorSet->AddBinding_uniformBuffer(0, &viewUniformBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		// galaxyBoxDescriptorSet->AddBinding_combinedImageSampler(1, &galaxyCubeMapImage, VK_SHADER_STAGE_FRAGMENT_BIT);
		// galaxyBoxPipelineLayout.AddDescriptorSet(galaxyBoxDescriptorSet);
		// galaxyBoxShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		// galaxyBoxShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
		// galaxyBoxShader.depthStencilState.depthWriteEnable = VK_FALSE;
		// galaxyBoxShader.depthStencilState.depthTestEnable = VK_FALSE;
		// galaxyBoxShader.SetData(4);
		
		// // Galaxy Fade
		// galaxyFadeShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		// galaxyFadeShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
		// galaxyFadeShader.depthStencilState.depthWriteEnable = VK_FALSE;
		// galaxyFadeShader.depthStencilState.depthTestEnable = VK_FALSE;
		// galaxyFadeShader.SetData(6);
		
		// /////////
		
		// // ppShader = new RasterShaderPipeline(postProcessingPipelineLayout, {
		// // 	"incubator_rendering/assets/shaders/postProcessing.vert",
		// // 	"incubator_rendering/assets/shaders/postProcessing.frag",
		// // });
		// // ppShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		// // ppShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		// // ppShader->depthStencilState.depthTestEnable = VK_FALSE;
		// // ppShader->depthStencilState.depthWriteEnable = VK_FALSE;
		// // ppShader->SetData(3);
		
		// /////////
		
		// // Load all shaders
		// galaxyGenShader.LoadShaders();
		// galaxyBoxShader.LoadShaders();
		// galaxyFadeShader.LoadShaders();
		// // ppShader->LoadShaders();
	}
	
	void UnloadScene() override {
		// // Staged buffers
		// for (auto* buffer : stagedBuffers) {
		// 	delete buffer;
		// }
		// stagedBuffers.clear();
		
		// // Descriptor sets
		// for (auto* set : descriptorSets) {
		// 	delete set;
		// }
		// descriptorSets.clear();
		
		// // Pipeline layouts
		// galaxyGenPipelineLayout.Reset();
		// galaxyBoxPipelineLayout.Reset();
	}
	
public: // Update
	void FrameUpdate(uint imageIndex) override {}
	void LowPriorityFrameUpdate() override {}
	
public: // ubo/conditional member variables
	bool continuousGalaxyGen = true;
	
};
