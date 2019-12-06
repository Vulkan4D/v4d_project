#pragma once

#include "VulkanRenderer.hpp"
#include "Geometry.hpp"

// Test Object Vertex Data Structure
struct Vertex {
	// glm::dvec3 pos;
	double posX, posY, posZ;
	glm::vec4 color;
};

struct UBO {
	glm::dmat4 proj;
	glm::dmat4 view;
	glm::dmat4 model;
	glm::dvec4 velocity;
};

struct GalaxyUBO {
	glm::dvec4 cameraPosition;
	int galaxyFrameIndex;
};

struct ConditionalRendering {
	int fadeGalaxy;
	int genGalaxy;
};

class VulkanRasterizationRenderer : public VulkanRenderer {
	using VulkanRenderer::VulkanRenderer;
	
	Buffer uniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(UBO), true};
	Buffer galaxyUniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(GalaxyUBO), true};
	Buffer conditionalRenderingBuffer {VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT, sizeof(ConditionalRendering), true};
	std::vector<Buffer*> stagedBuffers {};
	std::vector<Geometry*> geometries {};
	
	RasterShaderPipeline* testShader;
	RasterShaderPipeline* ppShader;
	CombinedImageSampler postProcessingSampler;
	
	ComputeShaderPipeline* computeTestShader;
	
private: // Rasterization Rendering
	RenderPass renderPass;
	RenderPass postProcessingRenderPass;
	RenderPass galaxyGenRenderPass;
	RenderPass galaxyFadeRenderPass;
	
	// Galaxy
	VkImage galaxyCubeImage = VK_NULL_HANDLE;
	VkDeviceMemory galaxyCubeImageMemory = VK_NULL_HANDLE;
	VkImageView galaxyCubeImageView = VK_NULL_HANDLE;
	CombinedImageSampler galaxyCubeSampler;
	struct {uint width; uint height; VkFormat format; uint layers = 6;} galaxyCubeImageFormat; 
	
	// Main Render Pass
	VkImage colorImage = VK_NULL_HANDLE;
	VkDeviceMemory colorImageMemory = VK_NULL_HANDLE;
	VkImageView colorImageView = VK_NULL_HANDLE;
	struct {uint width; uint height; VkFormat format; uint layers = 1;} colorImageFormat;
	
	// Depth Buffer
	VkImage depthImage = VK_NULL_HANDLE;
	VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE;
	struct {uint width; uint height; VkFormat format; uint layers = 1;} depthImageFormat;
	
	// Post processing
	VkImage ppImage = VK_NULL_HANDLE;
	VkDeviceMemory ppImageMemory = VK_NULL_HANDLE;
	VkImageView ppImageView = VK_NULL_HANDLE;
	struct {uint width; uint height; VkFormat format; uint layers = 1;} ppImageFormat;
	
	// Graphics settings
	float renderingResolutionScale = 1.0;
	float galaxyResolutionScale = 1.0;
	
private: // Renderer Configuration methods
	void Init() override {
		// Set all device features that you may want to use, then the unsupported features will be disabled, you may check via this object later.
		deviceFeatures.geometryShader = VK_TRUE;
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.sampleRateShading = VK_TRUE;
		
		// Preferences
		preferredPresentModes = {
			// VK_PRESENT_MODE_MAILBOX_KHR,	// TripleBuffering (No Tearing, low latency)
			// VK_PRESENT_MODE_FIFO_KHR,		// VSync ON (No Tearing, more latency)
			VK_PRESENT_MODE_IMMEDIATE_KHR,	// VSync OFF (With Tearing, no latency)
		};
		
		RequiredDeviceExtension(VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME);
		RequiredDeviceExtension(VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME);
	}
	
	void ScorePhysicalDeviceSelection(int& score, PhysicalDevice* physicalDevice) {
		// Build up a score here and the PhysicalDevice with the highest score will be selected.
		// Add to the score optional specs, then multiply with mandatory specs.
		
		// Optional specs  -->  score += points * CONDITION
		score += 10 * (physicalDevice->GetProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU); // Is a Dedicated PhysicalDevice
		score += 20 * physicalDevice->GetFeatures().tessellationShader; // Supports Tessellation
		score += physicalDevice->GetProperties().limits.framebufferColorSampleCounts; // Add sample counts to the score (1-64)

		// Mandatory specs  -->  score *= CONDITION
		score *= physicalDevice->GetFeatures().geometryShader; // Supports Geometry Shaders
		score *= physicalDevice->GetFeatures().samplerAnisotropy; // Supports Anisotropic filtering
		score *= physicalDevice->GetFeatures().sampleRateShading; // Supports Sample Shading
	}
	
	void Info() override {
		
	}

	void CreateResources() override {
		{// Main render pass resources
			VkImageViewCreateInfo viewInfo = {};
			
			// Main color attachment
			colorImageFormat = {
				(uint)((float)swapChain->extent.width * renderingResolutionScale),
				(uint)((float)swapChain->extent.height * renderingResolutionScale),
				renderingPhysicalDevice->FindSupportedFormat({VK_FORMAT_R32G32B32A32_SFLOAT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
			};
			renderingDevice->CreateImage(
				colorImageFormat.width,
				colorImageFormat.height,
				1, VK_SAMPLE_COUNT_1_BIT,
				colorImageFormat.format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				colorImage,
				colorImageMemory,
				colorImageFormat.layers
			);
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = colorImage;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = colorImageFormat.format;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = colorImageFormat.layers;
			if (renderingDevice->CreateImageView(&viewInfo, nullptr, &colorImageView) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create texture image view");
			}
			TransitionImageLayout(colorImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, colorImageFormat.layers);
			
			// Depth Image
			depthImageFormat = {
				colorImageFormat.width,
				colorImageFormat.height,
				renderingPhysicalDevice->FindSupportedFormat({VK_FORMAT_D32_SFLOAT_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			};
			renderingDevice->CreateImage(
				depthImageFormat.width,
				depthImageFormat.height,
				1, VK_SAMPLE_COUNT_1_BIT,
				depthImageFormat.format, 
				VK_IMAGE_TILING_OPTIMAL, 
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
				depthImage, 
				depthImageMemory,
				depthImageFormat.layers
			);
			// Depth Image View
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = depthImage;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = depthImageFormat.format;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = depthImageFormat.layers;
			if (renderingDevice->CreateImageView(&viewInfo, nullptr, &depthImageView) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create texture image view");
			}
			// Transition Layout
			TransitionImageLayout(depthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, depthImageFormat.layers);
		
			// Post Processing
			ppImageFormat = {
				(uint)((float)swapChain->extent.width),
				(uint)((float)swapChain->extent.height),
				swapChain->format.format
			};
			renderingDevice->CreateImage(
				ppImageFormat.width,
				ppImageFormat.height,
				1, VK_SAMPLE_COUNT_1_BIT,
				ppImageFormat.format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				ppImage,
				ppImageMemory,
				ppImageFormat.layers
			);
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = ppImage;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = ppImageFormat.format;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = ppImageFormat.layers;
			if (renderingDevice->CreateImageView(&viewInfo, nullptr, &ppImageView) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create texture image view");
			}
			TransitionImageLayout(ppImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, ppImageFormat.layers);
			// Create sampler
			VkSamplerCreateInfo sampler {};
			sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler.maxAnisotropy = 1.0f;
			sampler.magFilter = VK_FILTER_LINEAR;
			sampler.minFilter = VK_FILTER_LINEAR;
			sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			sampler.addressModeV = sampler.addressModeU;
			sampler.addressModeW = sampler.addressModeU;
			sampler.mipLodBias = 0.0f;
			sampler.maxAnisotropy = 1.0f;
			sampler.compareOp = VK_COMPARE_OP_NEVER;
			sampler.minLod = 0.0f;
			sampler.maxLod = 1.0f;
			sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			if (renderingDevice->CreateSampler(&sampler, nullptr, &postProcessingSampler.sampler) != VK_SUCCESS)
				throw std::runtime_error("Failed to create sampler");
			postProcessingSampler.imageView = colorImageView;
			
		}
	
		{// Galaxy Cube resources
			galaxyCubeImageFormat.width = (uint)((float)(std::max(swapChain->extent.width, swapChain->extent.height)) * galaxyResolutionScale);
			galaxyCubeImageFormat.height = galaxyCubeImageFormat.width;
			galaxyCubeImageFormat.format = swapChain->format.format;
			VkImageViewCreateInfo viewInfo = {};
			renderingDevice->CreateImage(
				galaxyCubeImageFormat.width,
				galaxyCubeImageFormat.width,
				1, VK_SAMPLE_COUNT_1_BIT,
				galaxyCubeImageFormat.format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				galaxyCubeImage,
				galaxyCubeImageMemory,
				galaxyCubeImageFormat.layers,
				VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
				{graphicsQueue.familyIndex, lowPriorityGraphicsQueue.familyIndex}
			);
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = galaxyCubeImage;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
			viewInfo.format = galaxyCubeImageFormat.format;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = galaxyCubeImageFormat.layers;
			if (renderingDevice->CreateImageView(&viewInfo, nullptr, &galaxyCubeImageView) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create texture image view for galaxy cube");
			}
			TransitionImageLayout(galaxyCubeImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1, galaxyCubeImageFormat.layers);
			// Sampler
			VkSamplerCreateInfo sampler {};
			sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler.maxAnisotropy = 1.0f;
			sampler.magFilter = VK_FILTER_LINEAR;
			sampler.minFilter = VK_FILTER_LINEAR;
			sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			sampler.addressModeV = sampler.addressModeU;
			sampler.addressModeW = sampler.addressModeU;
			sampler.mipLodBias = 0.0f;
			sampler.maxAnisotropy = 1.0f;
			sampler.compareOp = VK_COMPARE_OP_NEVER;
			sampler.minLod = 0.0f;
			sampler.maxLod = 1.0f;
			sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			if (renderingDevice->CreateSampler(&sampler, nullptr, &galaxyCubeSampler.sampler) != VK_SUCCESS)
				throw std::runtime_error("Failed to create sampler");
			galaxyCubeSampler.imageView = galaxyCubeImageView;
		}
	}
	
	void DestroyResources() override {
		// Main render pass resources
		if (colorImage != VK_NULL_HANDLE) {
			renderingDevice->DestroyImageView(colorImageView, nullptr);
			renderingDevice->DestroyImage(colorImage, nullptr);
			renderingDevice->FreeMemory(colorImageMemory, nullptr);
			colorImage = VK_NULL_HANDLE;
		}
		if (depthImage != VK_NULL_HANDLE) {
			renderingDevice->DestroyImageView(depthImageView, nullptr);
			renderingDevice->DestroyImage(depthImage, nullptr);
			renderingDevice->FreeMemory(depthImageMemory, nullptr);
			depthImage = VK_NULL_HANDLE;
		}
		
		// Post-processing
		if (ppImage != VK_NULL_HANDLE) {
			renderingDevice->DestroySampler(postProcessingSampler.sampler, nullptr);
			renderingDevice->DestroyImageView(ppImageView, nullptr);
			renderingDevice->DestroyImage(ppImage, nullptr);
			renderingDevice->FreeMemory(ppImageMemory, nullptr);
			ppImage = VK_NULL_HANDLE;
		}
		
		// Galaxy Cube resources
		if (galaxyCubeImage != VK_NULL_HANDLE) {
			renderingDevice->DestroySampler(galaxyCubeSampler.sampler, nullptr);
			renderingDevice->DestroyImageView(galaxyCubeImageView, nullptr);
			renderingDevice->DestroyImage(galaxyCubeImage, nullptr);
			renderingDevice->FreeMemory(galaxyCubeImageMemory, nullptr);
			galaxyCubeImage = VK_NULL_HANDLE;
		}
	}
	
public: // Scene configuration methods

	struct Galaxy {
		glm::vec4 posr;
		int seed;
		int numStars;
	};
	
	std::vector<Galaxy> galaxies {};
	RasterShaderPipeline* galaxyGenShader = nullptr;
	RasterShaderPipeline* galaxyBoxShader = nullptr;
	RasterShaderPipeline* galaxyFadeShader = nullptr;
	
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
	
	void LoadScene() override {
		uint seed = 1;
		
		// Galaxy Gen
		for (int x = -64; x < 63; ++x) {
			for (int y = -64; y < 63; ++y) {
				for (int z = -8; z < 7; ++z) {
					float centerFactor = 1.0f - glm::length(glm::vec3((float)x,(float)y,(float)z))/180.0f;
					if (centerFactor > 0.0) {
						auto offset = RandomInUnitSphere(seed)*2.84f;
						galaxies.push_back({
							{x*5+offset.x, y*5+offset.y, z*5+offset.z, glm::pow(centerFactor, 25)*10}, x*y*z*3+5, (int)(glm::pow(centerFactor, 8)*50.0f)
						});
					}
				}
			}
		}
		auto* galaxiesDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(0));
		galaxiesDescriptorSet->AddBinding_uniformBuffer(0, &uniformBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		galaxiesDescriptorSet->AddBinding_uniformBuffer(1, &galaxyUniformBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		PipelineLayout* galaxyGenPipelineLayout = pipelineLayouts.emplace_back(new PipelineLayout());
		galaxyGenPipelineLayout->AddDescriptorSet(galaxiesDescriptorSet);
		Buffer* galaxiesBuffer = stagedBuffers.emplace_back(new Buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
		galaxyGenShader = new RasterShaderPipeline(galaxyGenPipelineLayout, {
			"incubator_rendering/assets/shaders/galaxy.gen.geom",
			"incubator_rendering/assets/shaders/galaxy.gen.vert",
			"incubator_rendering/assets/shaders/galaxy.gen.frag",
		});
		galaxyGenShader->AddVertexInputBinding(sizeof(Galaxy), VK_VERTEX_INPUT_RATE_VERTEX /*VK_VERTEX_INPUT_RATE_INSTANCE*/, {
			{0, offsetof(Galaxy, Galaxy::posr), VK_FORMAT_R32G32B32A32_SFLOAT},
			{1, offsetof(Galaxy, Galaxy::seed), VK_FORMAT_R32_UINT},
			{2, offsetof(Galaxy, Galaxy::numStars), VK_FORMAT_R32_UINT},
		});
		galaxiesBuffer->AddSrcDataPtr(&galaxies);
		galaxyGenShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		galaxyGenShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		galaxyGenShader->depthStencilState.depthWriteEnable = VK_FALSE;
		galaxyGenShader->depthStencilState.depthTestEnable = VK_FALSE;
		galaxyGenShader->SetData(galaxiesBuffer, galaxies.size());
		galaxyGenShader->LoadShaders();

		
		// Galaxy Box
		auto* galaxyBoxDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(0));
		galaxyBoxDescriptorSet->AddBinding_uniformBuffer(0, &uniformBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		galaxyBoxDescriptorSet->AddBinding_combinedImageSampler(1, &galaxyCubeSampler, VK_SHADER_STAGE_FRAGMENT_BIT);
		PipelineLayout* galaxyBoxPipelineLayout = pipelineLayouts.emplace_back(new PipelineLayout());
		galaxyBoxPipelineLayout->AddDescriptorSet(galaxyBoxDescriptorSet);
		galaxyBoxShader = new RasterShaderPipeline(galaxyBoxPipelineLayout, {
			"incubator_rendering/assets/shaders/galaxy.box.vert",
			"incubator_rendering/assets/shaders/galaxy.box.frag",
		});
		galaxyBoxShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		galaxyBoxShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		galaxyBoxShader->depthStencilState.depthWriteEnable = VK_FALSE;
		galaxyBoxShader->depthStencilState.depthTestEnable = VK_FALSE;
		galaxyBoxShader->SetData(4);
		galaxyBoxShader->LoadShaders();
		
		
		
		// Galaxy Fade
		galaxyFadeShader = new RasterShaderPipeline(galaxyBoxPipelineLayout, {
			"incubator_rendering/assets/shaders/galaxy.fade.vert",
			"incubator_rendering/assets/shaders/galaxy.fade.geom",
			"incubator_rendering/assets/shaders/galaxy.fade.frag",
		});
		galaxyFadeShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		galaxyFadeShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		galaxyFadeShader->depthStencilState.depthWriteEnable = VK_FALSE;
		galaxyFadeShader->depthStencilState.depthTestEnable = VK_FALSE;
		galaxyFadeShader->SetData(6);
		galaxyFadeShader->LoadShaders();
		
		
		
		// Objects
		
		// Descriptor sets & Pipeline Layouts
		auto* descriptorSet = descriptorSets.emplace_back(new DescriptorSet(0));
		descriptorSet->AddBinding_uniformBuffer(0, &uniformBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		PipelineLayout* mainPipelineLayout = pipelineLayouts.emplace_back(new PipelineLayout());
		mainPipelineLayout->AddDescriptorSet(descriptorSet);
		PipelineLayout* postProcessingPipelineLayout = nullptr;
		auto* ppDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(0));
		ppDescriptorSet->AddBinding_combinedImageSampler(0, &postProcessingSampler, VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingPipelineLayout = pipelineLayouts.emplace_back(new PipelineLayout());
		postProcessingPipelineLayout->AddDescriptorSet(ppDescriptorSet);
		
		Buffer* vertexBuffer = stagedBuffers.emplace_back(new Buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
		Buffer* indexBuffer = stagedBuffers.emplace_back(new Buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
		
		// Add triangle geometries
		auto* trianglesGeometry1 = new TriangleGeometry<Vertex>({
			{/*pos*/-0.5,-0.5, 0.0, /*color*/{1.0, 0.0, 0.0, 0.5}},
			{/*pos*/ 0.5,-0.5, 0.0, /*color*/{1.0, 0.0, 0.0, 0.5}},
			{/*pos*/ 0.5, 0.5, 0.0, /*color*/{1.0, 0.0, 0.0, 0.5}},
			{/*pos*/-0.5, 0.5, 0.0, /*color*/{1.0, 0.0, 0.0, 0.5}},
			//
			{/*pos*/-0.5,-0.5,-0.2, /*color*/{0.0, 1.0, 0.0, 1.0}},
			{/*pos*/ 0.5,-0.5,-0.2, /*color*/{0.0, 1.0, 0.0, 1.0}},
			{/*pos*/ 0.5, 0.5,-0.2, /*color*/{0.0, 1.0, 0.0, 1.0}},
			{/*pos*/-0.5, 0.5,-0.2, /*color*/{0.0, 1.0, 0.0, 1.0}},
			
			// bottom white
			/*  8 */{/*pos*/-8.0,-8.0,-2.0, /*color*/{1.0,1.0,1.0, 1.0}},
			/*  9 */{/*pos*/ 8.0,-8.0,-2.0, /*color*/{1.0,1.0,1.0, 1.0}},
			/* 10 */{/*pos*/ 8.0, 8.0,-2.0, /*color*/{1.0,1.0,1.0, 1.0}},
			/* 11 */{/*pos*/-8.0, 8.0,-2.0, /*color*/{1.0,1.0,1.0, 1.0}},
			
			// top gray
			/* 12 */{/*pos*/-8.0,-8.0, 4.0, /*color*/{0.5,0.5,0.5, 1.0}},
			/* 13 */{/*pos*/ 8.0,-8.0, 4.0, /*color*/{0.5,0.5,0.5, 1.0}},
			/* 14 */{/*pos*/ 8.0, 8.0, 4.0, /*color*/{0.5,0.5,0.5, 1.0}},
			/* 15 */{/*pos*/-8.0, 8.0, 4.0, /*color*/{0.5,0.5,0.5, 1.0}},
			
			// left red
			/* 16 */{/*pos*/ 8.0, 8.0,-2.0, /*color*/{1.0,0.0,0.0, 1.0}},
			/* 17 */{/*pos*/ 8.0,-8.0,-2.0, /*color*/{1.0,0.0,0.0, 1.0}},
			/* 18 */{/*pos*/ 8.0, 8.0, 4.0, /*color*/{1.0,0.0,0.0, 1.0}},
			/* 19 */{/*pos*/ 8.0,-8.0, 4.0, /*color*/{1.0,0.0,0.0, 1.0}},
			
			// back blue
			/* 20 */{/*pos*/ 8.0,-8.0, 4.0, /*color*/{0.0,0.0,1.0, 1.0}},
			/* 21 */{/*pos*/ 8.0,-8.0,-2.0, /*color*/{0.0,0.0,1.0, 1.0}},
			/* 22 */{/*pos*/-8.0,-8.0, 4.0, /*color*/{0.0,0.0,1.0, 1.0}},
			/* 23 */{/*pos*/-8.0,-8.0,-2.0, /*color*/{0.0,0.0,1.0, 1.0}},
			
			// right green
			/* 24 */{/*pos*/-8.0, 8.0,-2.0, /*color*/{0.0,1.0,0.0, 1.0}},
			/* 25 */{/*pos*/-8.0,-8.0,-2.0, /*color*/{0.0,1.0,0.0, 1.0}},
			/* 26 */{/*pos*/-8.0, 8.0, 4.0, /*color*/{0.0,1.0,0.0, 1.0}},
			/* 27 */{/*pos*/-8.0,-8.0, 4.0, /*color*/{0.0,1.0,0.0, 1.0}},
		}, {
			0, 1, 2, 2, 3, 0,
			4, 5, 6, 6, 7, 4,
			//
			8, 9, 10, 10, 11, 8,
			13, 12, 14, 14, 12, 15,
			16, 17, 18, 18, 17, 19,
			20, 21, 22, 22, 21, 23,
			25, 24, 26, 26, 27, 25,
		}, vertexBuffer, 0, indexBuffer, 0);
		geometries.push_back(trianglesGeometry1);
		
		// Assign buffer data
		vertexBuffer->AddSrcDataPtr(&trianglesGeometry1->vertexData);
		indexBuffer->AddSrcDataPtr(&trianglesGeometry1->indexData);

		// Shader program
		testShader = new RasterShaderPipeline(mainPipelineLayout, {
			"incubator_rendering/assets/shaders/raster.vert",
			"incubator_rendering/assets/shaders/raster.frag",
		});
		// Vertex Input structure
		testShader->AddVertexInputBinding(sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX /*VK_VERTEX_INPUT_RATE_INSTANCE*/, {
			{0, offsetof(Vertex, Vertex::posX), VK_FORMAT_R64_SFLOAT},
			{1, offsetof(Vertex, Vertex::posY), VK_FORMAT_R64_SFLOAT},
			{2, offsetof(Vertex, Vertex::posZ), VK_FORMAT_R64_SFLOAT},
			{3, offsetof(Vertex, Vertex::color), VK_FORMAT_R32G32B32A32_SFLOAT},
		});
		testShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		testShader->SetData(vertexBuffer, indexBuffer);
		testShader->LoadShaders();
		
		// Post processing
		// Shader program
		ppShader = new RasterShaderPipeline(postProcessingPipelineLayout, {
			"incubator_rendering/assets/shaders/postProcessing.vert",
			"incubator_rendering/assets/shaders/postProcessing.frag",
		});
		ppShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		ppShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		ppShader->depthStencilState.depthTestEnable = VK_FALSE;
		ppShader->depthStencilState.depthWriteEnable = VK_FALSE;
		ppShader->SetData(3);
		ppShader->LoadShaders();
		
		
		// Compute shader
		auto* galaxiesComputeDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(1));
		galaxiesComputeDescriptorSet->AddBinding_imageView(0, &galaxyCubeImageView, VK_SHADER_STAGE_COMPUTE_BIT);
		galaxyGenPipelineLayout->AddDescriptorSet(galaxiesComputeDescriptorSet);
		computeTestShader = new ComputeShaderPipeline(galaxyGenPipelineLayout, "incubator_rendering/assets/shaders/compute_test.comp");
		computeTestShader->LoadShaders();
	}

	void UnloadScene() override {
		// Shaders
		delete galaxyGenShader;
		delete galaxyBoxShader;
		delete galaxyFadeShader;
		delete testShader;
		delete ppShader;
		delete computeTestShader;
		
		// Geometries
		for (auto* geometry : geometries) {
			delete geometry;
		}
		geometries.clear();
		
		// Staged buffers
		for (auto* buffer : stagedBuffers) {
			delete buffer;
		}
		stagedBuffers.clear();
		
		// Pipeline Layouts
		for (auto* layout : pipelineLayouts) {
			delete layout;
		}
		pipelineLayouts.clear();
		
		// Descriptor sets
		for (auto* set : descriptorSets) {
			delete set;
		}
		descriptorSets.clear();
	}

protected: // Graphics configuration
	void CreateSceneGraphics() override {
		// Staged Buffers
		AllocateBuffersStaged(transferQueue, stagedBuffers);
		// Other buffers
		uniformBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		uniformBuffer.MapMemory(renderingDevice);
		galaxyUniformBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		galaxyUniformBuffer.MapMemory(renderingDevice);
		conditionalRenderingBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		conditionalRenderingBuffer.MapMemory(renderingDevice);
	}

	void DestroySceneGraphics() override {
		for (auto& buffer : stagedBuffers) {
			buffer->Free(renderingDevice);
		}

		// Other buffers
		uniformBuffer.UnmapMemory(renderingDevice);
		uniformBuffer.Free(renderingDevice);
		galaxyUniformBuffer.UnmapMemory(renderingDevice);
		galaxyUniformBuffer.Free(renderingDevice);
		conditionalRenderingBuffer.UnmapMemory(renderingDevice);
		conditionalRenderingBuffer.Free(renderingDevice);
	}
	
	void CreateGraphicsPipelines() override {
		{// Main Render pass
			// Color Attachment (Fragment shader Standard Output)
			VkAttachmentDescription colorAttachment = {}; // defines the output data from the fragment shader (o_color)
				colorAttachment.format = colorImageFormat.format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // Need more with multisampling
				// Color and depth data
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // What to do with the attachment before rendering
									/*	VK_ATTACHMENT_LOAD_OP_LOAD = Preserve the existing contents of the attachment
										VK_ATTACHMENT_LOAD_OP_CLEAR = Clear the values to a constant at the start
										VK_ATTACHMENT_LOAD_OP_DONT_CARE = Existing contents are undefined, we dont care about them (faster but may cause glitches if not set properly I guess)
									*/
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // What to do with the attachment after rendering
									/*	VK_ATTACHMENT_STORE_OP_STORE = Rendered contents contents will be stored in memory and can be renad later (Need this to see something on the screen in the case of o_color)
										VK_ATTACHMENT_STORE_OP_DONT_CARE = Contents of the framebuffer will be undefined after rendering operation (Ignore this data ??)
									*/
				// Stencil data
				colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				// Textures and framebuffers in Vulkan are represented by VkImage objects with a certain pixel format, however the layout of the pixels in memory can change based on what you're trying to do with an image.
				// The InitialLayout specifies wich layout the image will have before the render pass begins. 
				// The finalLayout specifies the layout to automatically transition to when the render pass finishes. 
				// Using VK_IMAGE_LAYOUT_UNDEFINED for initialLayout means that we dont care what previous layout the image was in. 
				// The caveat of this special value is that the contents of the image are not guaranteed to be preserved, but that doesnt matter since were going to clear it anyway. 
				// We want the image to be ready for presentation using the swap chain after rendering, which is why we use VK_IMAGE_LAYOUT_PRESENT_SRC_KHR as finalLayout.
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
										/*	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = Images used as color attachment (use this for color attachment with multisampling, adapt rendering pipeline)
											VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = Images to be presented in the swap chain (use this for color attachment when no multisampling, adapt rendering pipeline)
											VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = Images to be used as destination for a memory copy operation
										*/
				// A single render pass can consist of multiple subpasses.
				// Subpasses are subsequent rendering operations that depend on the contents of framebuffers in previous passes, for example a sequence of post-processing effects that are applied one after another. 
				// If you group these rendering operations into one render pass, then vulkan is able to reorder the operations and conserve memory bandwidth for possibly better preformance. 
				// Every subpass references one or more of the attachments that we've described using the structure in the previous sections. 
				// These references are themselves VkAttachmentReference structs that look like this:
			VkAttachmentReference colorAttachmentRef = {
				renderPass.AddAttachment(colorAttachment), // layout(location = 0) for output data in the fragment shader
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			// The attachment parameter specifies which attachment to reference by its index in the attachment descriptions array.
			// Our array consists of a single VkAttachmentDescription, so its index is 0. 
			// The layout specifies which layout we would like the attachment to have during a subpass that uses this reference.
			// Vulkan will automatically transition the attachment to this layout when the subpass is started.
			// We intend to use the attachment to function as a color buffer and the VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL layout will give us the best performance, as its name implies.

			
			// Depth Attachment
			VkAttachmentDescription depthAttachment = {};
				depthAttachment.format = depthImageFormat.format;
				depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			VkAttachmentReference depthAttachmentRef = {
				renderPass.AddAttachment(depthAttachment), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			};
			
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
				subpass.pDepthStencilAttachment = &depthAttachmentRef;
			renderPass.AddSubpass(subpass);
			// The following other types of attachments can be referenced by a subpass :
				// pInputAttachments : attachments that are read from a shader
				// pResolveAttachments : attachments used for multisampling color attachments
				// pDepthStencilAttachments : attachments for depth and stencil data
				// pPreserveAttachments : attachments that are not used by this subpass, but for which the data must be preserved
			// Render pass
			// Now that the attachment and a basic subpass refencing it have been described, we can create the render pass itself.
			// The render pass object can then be created by filling in the VkRenderPassCreateInfo structure with an array of attachments and subpasses. 
			// The VkAttachmentReference objects reference attachments using the indices of this array.

			// Render Pass Dependencies
			// There are two built-in dependencies that take care of the transition at the start of the render pass and at the end of the render pass, 
			// but the former does not occur at the right time. It assumes that the transition occurs at the start of the pipeline, but we haven't aquired the image yet at that point.
			// There are two ways to deal with this problem.
			// We could change the waitStage for the imageAvailableSemaphore to VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT to ensure that the render passes dont begin until the image is available, 
			// or we can make the render pass wait for the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage, which is the option that we are using here.

			// Create the render pass
			renderPass.Create(renderingDevice);
			
			VkPipelineViewportStateCreateInfo viewportState {};
			VkViewport viewport {};
			VkRect2D scissor {};
			viewport.x = 0;
			viewport.y = 0;
			viewport.width = (float) colorImageFormat.width;
			viewport.height = (float) colorImageFormat.height;
			viewport.minDepth = 0;
			viewport.maxDepth = 1;
			scissor.offset = {0, 0};
			scissor.extent = {colorImageFormat.width, colorImageFormat.height};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.scissorCount = 1;
			viewportState.pViewports = &viewport;
			viewportState.pScissors = &scissor;
			
			// Frame Buffer
			std::vector<VkImageView> attachments {
				colorImageView,
				depthImageView,
			};
			// You can only use a framebuffer with the render passes that it is compatible with, which roughly means that they use the same number and type of attachments
			VkFramebufferCreateInfo framebufferCreateInfo = {};
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.renderPass = renderPass.handle;
			framebufferCreateInfo.attachmentCount = attachments.size();
			framebufferCreateInfo.pAttachments = attachments.data(); // Specifies the VkImageView objects that should be bound to the respective attachment descriptions in the render pass pAttachment array.
			framebufferCreateInfo.width = colorImageFormat.width;
			framebufferCreateInfo.height = colorImageFormat.height;
			framebufferCreateInfo.layers = colorImageFormat.layers;
			if (renderingDevice->CreateFramebuffer(&framebufferCreateInfo, nullptr, &renderPass.GetFrameBuffer()) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create framebuffer");
			}
			
			// Shaders
			for (auto* shaderPipeline : {galaxyBoxShader, testShader}) {
				shaderPipeline->SetRenderPass(&viewportState, renderPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState(
					VK_TRUE,
					VK_BLEND_FACTOR_ONE,
					VK_BLEND_FACTOR_ONE,
					VK_BLEND_OP_ADD,
					VK_BLEND_FACTOR_ONE,
					VK_BLEND_FACTOR_ONE,
					VK_BLEND_OP_ADD
				);
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Post Processing render pass
			// Color Attachment (Fragment shader Standard Output)
			VkAttachmentDescription colorAttachment = {}; // defines the output data from the fragment shader (o_color)
				colorAttachment.format = ppImageFormat.format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // Need more with multisampling
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // What to do with the attachment before rendering
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // What to do with the attachment after rendering
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			VkAttachmentReference colorAttachmentRef = {
				postProcessingRenderPass.AddAttachment(colorAttachment),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
			postProcessingRenderPass.AddSubpass(subpass);
			
			// // External Dependency (NOT SURE IF REALLY NEEDED)
			// VkSubpassDependency dependency = {};
			// 	dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // special value to refer to the implicit subpass before or after the render pass depending on wether it is specified in srcSubpass or dstSubpass.
			// 	dependency.dstSubpass = 0; // index 0 refers to our subpass, which is the first and only one. It must always be higher than srcSubpass to prevent cucles in the dependency graph.
			// 	// These two specify the operations to wait on and the stages in which these operations occur. 
			// 	// We need to wait for the swap chain to finish reading from the image before we can access it. 
			// 	// This can be accomplished by waiting on the color attachment output stage itself.
			// 	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			// 	dependency.srcAccessMask = 0;
			// 	// The operations that should wait on this are in the color attachment stage and involve reading and writing of the color attachment.
			// 	// These settings will prevent the transition from happening until it's actually necessary (and allowed): when we want to start writing colors to it.
			// 	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			// 	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			// // Set dependency to the render pass info
			// postProcessingRenderPass.renderPassInfo.dependencyCount = 1;
			// postProcessingRenderPass.renderPassInfo.pDependencies = &dependency;
			

			postProcessingRenderPass.Create(renderingDevice);

			// Frame Buffers
			postProcessingRenderPass.frameBuffers.resize(swapChain->imageViews.size());
			for (size_t i = 0; i < swapChain->imageViews.size(); i++) {
				// You can only use a framebuffer with the render passes that it is compatible with, which roughly means that they use the same number and type of attachments
				VkFramebufferCreateInfo framebufferCreateInfo = {};
				framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferCreateInfo.renderPass = postProcessingRenderPass.handle;
				framebufferCreateInfo.attachmentCount = 1;
				framebufferCreateInfo.pAttachments = &swapChain->imageViews[i]; // Specifies the VkImageView objects that should be bound to the respective attachment descriptions in the render pass pAttachment array.
				framebufferCreateInfo.width = ppImageFormat.width;
				framebufferCreateInfo.height = ppImageFormat.height;
				framebufferCreateInfo.layers = ppImageFormat.layers; // refers to the number of layers in image arrays
				if (renderingDevice->CreateFramebuffer(&framebufferCreateInfo, nullptr, &postProcessingRenderPass.GetFrameBuffer(i)) != VK_SUCCESS) {
					throw std::runtime_error("Failed to create framebuffer");
				}
			}
			
			// Shaders
			for (auto* shaderPipeline : {ppShader}) {
				shaderPipeline->SetRenderPass(&swapChain->viewportState, postProcessingRenderPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Galaxy Gen render pass
			// Color Attachment (Fragment shader Standard Output)
			VkAttachmentDescription colorAttachment = {}; // defines the output data from the fragment shader (o_color)
				colorAttachment.format = galaxyCubeImageFormat.format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // Need more with multisampling
				// Color and depth data
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				// Stencil data
				colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			VkAttachmentReference colorAttachmentRef = {
				galaxyGenRenderPass.AddAttachment(colorAttachment),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
			galaxyGenRenderPass.AddSubpass(subpass);
			
			// Create the render pass
			galaxyGenRenderPass.Create(renderingDevice);
			
			VkPipelineViewportStateCreateInfo viewportState {};
			VkViewport viewport {};
			VkRect2D scissor {};
			viewport.x = 0;
			viewport.y = 0;
			viewport.width = (float) galaxyCubeImageFormat.width;
			viewport.height = (float) galaxyCubeImageFormat.width;
			viewport.minDepth = 0;
			viewport.maxDepth = 1;
			scissor.offset = {0, 0};
			scissor.extent = {galaxyCubeImageFormat.width, galaxyCubeImageFormat.width};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.scissorCount = 1;
			viewportState.pViewports = &viewport;
			viewportState.pScissors = &scissor;
			
			// Frame Buffer
			VkFramebufferCreateInfo framebufferCreateInfo = {};
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.renderPass = galaxyGenRenderPass.handle;
			framebufferCreateInfo.attachmentCount = 1;
			framebufferCreateInfo.pAttachments = &galaxyCubeImageView; // Specifies the VkImageView objects that should be bound to the respective attachment descriptions in the render pass pAttachment array.
			framebufferCreateInfo.width = galaxyCubeImageFormat.width;
			framebufferCreateInfo.height = galaxyCubeImageFormat.width;
			framebufferCreateInfo.layers = galaxyCubeImageFormat.layers; // refers to the number of layers in image arrays
			if (renderingDevice->CreateFramebuffer(&framebufferCreateInfo, nullptr, &galaxyGenRenderPass.GetFrameBuffer()) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create galaxy gen framebuffer");
			}
			
			// Shader pipeline
			galaxyGenShader->SetRenderPass(&viewportState, galaxyGenRenderPass.handle, 0);
			galaxyGenShader->AddColorBlendAttachmentState(
				/*blendEnable*/				VK_TRUE,
				/*srcColorBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*dstColorBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*colorBlendOp*/			VK_BLEND_OP_MAX,
				/*srcAlphaBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*dstAlphaBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*alphaBlendOp*/			VK_BLEND_OP_MAX
			);
			galaxyGenShader->CreatePipeline(renderingDevice);
		}
		
		{// Galaxy Fade render pass
			// Color Attachment (Fragment shader Standard Output)
			VkAttachmentDescription colorAttachment = {}; // defines the output data from the fragment shader (o_color)
				colorAttachment.format = galaxyCubeImageFormat.format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // Need more with multisampling
				// Color and depth data
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				// Stencil data
				colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			VkAttachmentReference colorAttachmentRef = {
				galaxyFadeRenderPass.AddAttachment(colorAttachment),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
				subpass.pDepthStencilAttachment = nullptr;
			galaxyFadeRenderPass.AddSubpass(subpass);
			
			// Create the render pass
			galaxyFadeRenderPass.Create(renderingDevice);
			
			VkPipelineViewportStateCreateInfo viewportState {};
			VkViewport viewport {};
			VkRect2D scissor {};
			viewport.x = 0;
			viewport.y = 0;
			viewport.width = (float) galaxyCubeImageFormat.width;
			viewport.height = (float) galaxyCubeImageFormat.width;
			viewport.minDepth = 0;
			viewport.maxDepth = 1;
			scissor.offset = {0, 0};
			scissor.extent = {galaxyCubeImageFormat.width, galaxyCubeImageFormat.width};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.scissorCount = 1;
			viewportState.pViewports = &viewport;
			viewportState.pScissors = &scissor;
			
			// Frame Buffer
			VkFramebufferCreateInfo framebufferCreateInfo = {};
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.renderPass = galaxyFadeRenderPass.handle;
			framebufferCreateInfo.attachmentCount = 1;
			framebufferCreateInfo.pAttachments = &galaxyCubeImageView; // Specifies the VkImageView objects that should be bound to the respective attachment descriptions in the render pass pAttachment array.
			framebufferCreateInfo.width = galaxyCubeImageFormat.width;
			framebufferCreateInfo.height = galaxyCubeImageFormat.width;
			framebufferCreateInfo.layers = galaxyCubeImageFormat.layers; // refers to the number of layers in image arrays
			if (renderingDevice->CreateFramebuffer(&framebufferCreateInfo, nullptr, &galaxyFadeRenderPass.GetFrameBuffer()) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create galaxy gen framebuffer");
			}
			
			// Shader pipeline
			galaxyFadeShader->SetRenderPass(&viewportState, galaxyFadeRenderPass.handle, 0);
			galaxyFadeShader->AddColorBlendAttachmentState(
				/*blendEnable*/				VK_TRUE,
				/*srcColorBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*dstColorBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*colorBlendOp*/			VK_BLEND_OP_REVERSE_SUBTRACT,
				/*srcAlphaBlendFactor*/		VK_BLEND_FACTOR_ZERO,
				/*dstAlphaBlendFactor*/		VK_BLEND_FACTOR_ZERO,
				/*alphaBlendOp*/			VK_BLEND_OP_MIN
			);
			// Shader stages
			galaxyFadeShader->CreatePipeline(renderingDevice);
		}
		
		// Compute test
		computeTestShader->CreatePipeline(renderingDevice);
	}
	
	void DestroyGraphicsPipelines() override {
		
		// Main render pass
		for (auto& shaderPipeline : {galaxyBoxShader, testShader}) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		renderPass.Destroy(renderingDevice);
		renderingDevice->DestroyFramebuffer(renderPass.GetFrameBuffer(), nullptr);
		
		// Post-Processing
		ppShader->DestroyPipeline(renderingDevice);
		for (auto framebuffer : postProcessingRenderPass.frameBuffers) {
			renderingDevice->DestroyFramebuffer(framebuffer, nullptr);
		}
		postProcessingRenderPass.Destroy(renderingDevice);
		
		// Galaxy Gen
		galaxyGenShader->DestroyPipeline(renderingDevice);
		renderingDevice->DestroyFramebuffer(galaxyGenRenderPass.GetFrameBuffer(), nullptr);
		galaxyGenRenderPass.Destroy(renderingDevice);
		
		// Galaxy Fade
		galaxyFadeShader->DestroyPipeline(renderingDevice);
		renderingDevice->DestroyFramebuffer(galaxyFadeRenderPass.GetFrameBuffer(), nullptr);
		galaxyFadeRenderPass.Destroy(renderingDevice);
		
		// Compute test
		computeTestShader->DestroyPipeline(renderingDevice);
	}
	
	void LowPriorityRenderingCommandBuffer(VkCommandBuffer commandBuffer) {
		
		// Begin Render Pass
		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		// Defines the size of the render area, which defines where shader loads and stores will take place. The pixels outside this region will have undefined values. It should match the size of the attachments for best performance.
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = {galaxyCubeImageFormat.width, galaxyCubeImageFormat.height};
		
		// Conditional rendering
		VkConditionalRenderingBeginInfoEXT conditionalRenderingInfo {
			VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,// VkStructureType sType
			nullptr,// const void* pNext
			conditionalRenderingBuffer.buffer,// VkBuffer buffer
			0,// VkDeviceSize offset
			0,// VkConditionalRenderingFlagsEXT flags
		};
		
		// Galaxy Gen
		conditionalRenderingInfo.offset = offsetof(ConditionalRendering, ConditionalRendering::genGalaxy);
		renderingDevice->CmdBeginConditionalRenderingEXT(commandBuffer, &conditionalRenderingInfo);
		renderPassInfo.renderPass = galaxyGenRenderPass.handle;
		renderPassInfo.framebuffer = galaxyGenRenderPass.GetFrameBuffer();
		renderingDevice->CmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		galaxyGenShader->Execute(renderingDevice, commandBuffer);
		renderingDevice->CmdEndRenderPass(commandBuffer);
		renderingDevice->CmdEndConditionalRenderingEXT(commandBuffer);
		
		// Other galaxy elements
		//...
		
		// Galaxy Fade
		conditionalRenderingInfo.offset = offsetof(ConditionalRendering, ConditionalRendering::fadeGalaxy);
		renderingDevice->CmdBeginConditionalRenderingEXT(commandBuffer, &conditionalRenderingInfo);
		renderPassInfo.renderPass = galaxyFadeRenderPass.handle;
		renderPassInfo.framebuffer = galaxyFadeRenderPass.GetFrameBuffer();
		renderingDevice->CmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		galaxyFadeShader->Execute(renderingDevice, commandBuffer);
		renderingDevice->CmdEndRenderPass(commandBuffer);
		renderingDevice->CmdEndConditionalRenderingEXT(commandBuffer);
		
	}
	
	void RenderingCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		
		// Begin Render Pass
		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass.handle;
		renderPassInfo.framebuffer = renderPass.GetFrameBuffer(); // We create a framebuffer for each swap chain image that specifies it as color attachment
		// Defines the size of the render area, which defines where shader loads and stores will take place. The pixels outside this region will have undefined values. It should match the size of the attachments for best performance.
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = {colorImageFormat.width, colorImageFormat.height};
		// Related to VK_ATTACHMENT_LOAD_OP_CLEAR for the color attachment
		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = {.0,.0,.0,.0};
		clearValues[1].depthStencil = {1.0f/*depth*/, 0/*stencil*/}; // The range of depth is 0 to 1 in Vulkan, where 1 is far and 0 is near. We want to clear to the Far value.
		renderPassInfo.clearValueCount = clearValues.size();
		renderPassInfo.pClearValues = clearValues.data();
		//
		renderingDevice->CmdBeginRenderPass(commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE); // Returns void, so no error handling for any vkCmdXxx functions until we've finished recording.
															/*	the last parameter controls how the drawing commands within the render pass will be provided.
																VK_SUBPASS_CONTENTS_INLINE = The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed
																VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS = The render pass commands will be executed from secondary command buffers
															*/
		
		/////////////////////////////////////////
		galaxyBoxShader->Execute(renderingDevice, commandBuffer);
		testShader->Execute(renderingDevice, commandBuffer);
		/////////////////////////////////////////
		
		renderingDevice->CmdEndRenderPass(commandBuffers[imageIndex]);
		
		
		// Post Processing
		TransitionImageLayout(commandBuffer, colorImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, ppImageFormat.layers);
		
		// Begin Render Pass
		VkRenderPassBeginInfo ppRenderPassInfo = {};
		ppRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		ppRenderPassInfo.renderPass = postProcessingRenderPass.handle;
		ppRenderPassInfo.framebuffer = postProcessingRenderPass.GetFrameBuffer(imageIndex); // We create a framebuffer for each swap chain image that specifies it as color attachment
		// Defines the size of the render area, which defines where shader loads and stores will take place. The pixels outside this region will have undefined values. It should match the size of the attachments for best performance.
		ppRenderPassInfo.renderArea.offset = {0, 0};
		ppRenderPassInfo.renderArea.extent = {ppImageFormat.width, ppImageFormat.height};
		// Related to VK_ATTACHMENT_LOAD_OP_CLEAR for the color attachment
		std::array<VkClearValue, 1> ppClearValues = {};
		ppClearValues[0].color = {.0,.0,.0,.0};
		ppRenderPassInfo.clearValueCount = ppClearValues.size();
		ppRenderPassInfo.pClearValues = ppClearValues.data();
		//
		renderingDevice->CmdBeginRenderPass(commandBuffers[imageIndex], &ppRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE); // Returns void, so no error handling for any vkCmdXxx functions until we've finished recording.
		
		// post processing here
		ppShader->Execute(renderingDevice, commandBuffer);
		
		renderingDevice->CmdEndRenderPass(commandBuffers[imageIndex]);
		
	}
	
	
protected: // Methods executed on every frame
	void FrameUpdate(uint imageIndex) override {
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		double time = std::chrono::duration<double, std::chrono::seconds::period>(currentTime - startTime).count();
		UBO ubo {};
		// Slowly rotate the test object
		ubo.model = glm::rotate(glm::dmat4(1.0), time * glm::radians(10.0), glm::dvec3(0.0,0.0,1.0));
		
		// Current camera position
		ubo.view = glm::lookAt(camPosition, camPosition + camDirection, glm::dvec3(0,0,1));
		// Projection
		ubo.proj = glm::perspective(glm::radians(80.0), (double) swapChain->extent.width / (double) swapChain->extent.height, 0.01, 1.5e17); // 1cm - 1 000 000 UA  (WTF!!! seems to be working great..... 32bit z-buffer is enough???)
		ubo.proj[1][1] *= -1;
		
		ubo.velocity = glm::dvec4(velocity, speed);
		
		// Update memory
		uniformBuffer.WriteToMappedData(renderingDevice, &ubo);
	}
	
	void LowPriorityFrameUpdate() override {
		GalaxyUBO ubo {};
		// Galaxy convergence
		ConditionalRendering conditionalRendering {};
		const int convergences = 10;
		galaxyFrameIndex++;
		conditionalRendering.genGalaxy = (continuousGalaxyGen || galaxyFrameIndex <= convergences)? 1:0;
		if (galaxyFrameIndex > convergences) galaxyFrameIndex = continuousGalaxyGen? 0 : convergences;
		if (!continuousGalaxyGen && speed > 0) galaxyFrameIndex = 0;
		conditionalRendering.fadeGalaxy = (continuousGalaxyGen || speed > 0)? 1:0;
		
		ubo.cameraPosition = glm::dvec4(camPosition, 1);
		ubo.galaxyFrameIndex = galaxyFrameIndex;
		
		galaxyUniformBuffer.WriteToMappedData(renderingDevice, &ubo);
		conditionalRenderingBuffer.WriteToMappedData(renderingDevice, &conditionalRendering);
		
		// Clear galaxy upon stopping
		if (previousSpeed != speed && !continuousGalaxyGen) {
			previousSpeed = speed;
			if (speed == 0) {
				galaxyFrameIndex = 0;
				VkClearColorValue clearColor {.0,.0,.0,.0};
				VkImageSubresourceRange clearRange {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,galaxyCubeImageFormat.layers};
				auto cmdBuffer = BeginSingleTimeCommands(lowPriorityGraphicsQueue);
				TransitionImageLayout(cmdBuffer, galaxyCubeImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, galaxyCubeImageFormat.layers);
				renderingDevice->CmdClearColorImage(cmdBuffer, galaxyCubeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &clearRange);
				TransitionImageLayout(cmdBuffer, galaxyCubeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, galaxyCubeImageFormat.layers);
				EndSingleTimeCommands(lowPriorityGraphicsQueue, cmdBuffer);
			}
		}
		
		// // Compute
		// auto cmdBuffer = BeginSingleTimeCommands(lowPriorityComputeQueue);
		// computeTestShader->SetGroupCounts(galaxyCubeImageFormat.width, galaxyCubeImageFormat.width, galaxyCubeImageFormat.layers);
		// computeTestShader->Execute(renderingDevice, cmdBuffer);
		// EndSingleTimeCommands(lowPriorityComputeQueue, cmdBuffer);
	}
	
public: // user-defined state variables
	int galaxyFrameIndex = 0;
	float speed = 0;
	float previousSpeed = 0;
	bool toggleTest = false;
	bool continuousGalaxyGen = true;
	glm::dvec3 camPosition = glm::dvec3(2,2,2);
	glm::dvec3 camDirection = glm::dvec3(-2,-2,-2);
	glm::dvec3 velocity = glm::dvec3(0);
	
	
};
