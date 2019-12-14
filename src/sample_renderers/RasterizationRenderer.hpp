#pragma once
#include <v4d.h>
#include "../incubator_rendering/helpers/Geometry.hpp"

// Test Object Vertex Data Structure
struct Vertex {
	// glm::dvec3 pos;
	double posX, posY, posZ;
	glm::vec4 color;
};

struct UBO {
	glm::dmat4 proj;
	glm::dmat4 view;
	// glm::dvec4 velocity;
};

struct GalaxyUBO {
	glm::dvec4 cameraPosition;
	int galaxyFrameIndex;
};

struct ConditionalRendering {
	int fadeGalaxy;
	int genGalaxy;
};

class RasterizationRenderer : public v4d::graphics::Renderer {
	using v4d::graphics::Renderer::Renderer;
	
	Buffer uniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(UBO), true};
	Buffer galaxyUniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(GalaxyUBO), true};
	Buffer conditionalRenderingBuffer {VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT, sizeof(ConditionalRendering), true};
	std::vector<Buffer*> stagedBuffers {};
	std::vector<Geometry*> geometries {};
	
	PipelineLayout mainPipelineLayout,
				postProcessingPipelineLayout,
				galaxyGenPipelineLayout,
				galaxyBoxPipelineLayout;
	
	RasterShaderPipeline* testShader;
	RasterShaderPipeline* ppShader;
	ComputeShaderPipeline* computeTestShader;
	
	std::recursive_mutex uboMutex;
	
private: // Rasterization Rendering
	RenderPass renderPass;
	RenderPass postProcessingRenderPass;
	RenderPass galaxyGenRenderPass;
	RenderPass galaxyFadeRenderPass;
	
	// Images
	DepthStencilImage depthImage;
	Image colorImage {
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
	};
	Image ppImage {
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
	};
	CubeMapImage galaxyCubeImage {
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT
	};
	
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
			// VK_PRESENT_MODE_FIFO_KHR,	// VSync ON (No Tearing, more latency)
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

	void InitLayouts() override {}
	void ConfigureShaders() override {}
	
	void CreateResources() override {
		// Main color attachment
		colorImage.Create(
			renderingDevice,
			(uint)((float)swapChain->extent.width * renderingResolutionScale),
			(uint)((float)swapChain->extent.height * renderingResolutionScale),
			{VK_FORMAT_R32G32B32A32_SFLOAT}
		);
		TransitionImageLayout(colorImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		// Depth Image
		depthImage.Create(
			renderingDevice,
			colorImage.width,
			colorImage.height,
			{VK_FORMAT_D32_SFLOAT_S8_UINT}
		);
		TransitionImageLayout(depthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		// Post Processing
		ppImage.Create(
			renderingDevice,
			(uint)((float)swapChain->extent.width),
			(uint)((float)swapChain->extent.height),
			{swapChain->format.format}
		);
		TransitionImageLayout(ppImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		// Galaxy CubeMap
		uint size = (uint)((float)(std::max(swapChain->extent.width, swapChain->extent.height)) * galaxyResolutionScale);
		galaxyCubeImage.SetAccessQueues({graphicsQueue.familyIndex, lowPriorityGraphicsQueue.familyIndex});
		galaxyCubeImage.Create(
			renderingDevice, 
			size, size,
			{VK_FORMAT_R32G32B32A32_SFLOAT}
		);
		TransitionImageLayout(galaxyCubeImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	}
	
	void DestroyResources() override {
		colorImage.Destroy(renderingDevice);
		depthImage.Destroy(renderingDevice);
		ppImage.Destroy(renderingDevice);
		galaxyCubeImage.Destroy(renderingDevice);
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
		galaxyGenPipelineLayout.AddDescriptorSet(galaxiesDescriptorSet);
		Buffer* galaxiesBuffer = stagedBuffers.emplace_back(new Buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
		galaxyGenShader = new RasterShaderPipeline(galaxyGenPipelineLayout, {
			"incubator_rendering/assets/shaders/v4d_galaxy.gen.geom",
			"incubator_rendering/assets/shaders/v4d_galaxy.gen.vert",
			"incubator_rendering/assets/shaders/v4d_galaxy.gen.frag",
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

		
		// Galaxy Box
		auto* galaxyBoxDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(0));
		galaxyBoxDescriptorSet->AddBinding_uniformBuffer(0, &uniformBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		galaxyBoxDescriptorSet->AddBinding_combinedImageSampler(1, &galaxyCubeImage, VK_SHADER_STAGE_FRAGMENT_BIT);
		galaxyBoxPipelineLayout.AddDescriptorSet(galaxyBoxDescriptorSet);
		galaxyBoxShader = new RasterShaderPipeline(galaxyBoxPipelineLayout, {
			"incubator_rendering/assets/shaders/v4d_galaxy.box.vert",
			"incubator_rendering/assets/shaders/v4d_galaxy.box.frag",
		});
		galaxyBoxShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		galaxyBoxShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		galaxyBoxShader->depthStencilState.depthWriteEnable = VK_FALSE;
		galaxyBoxShader->depthStencilState.depthTestEnable = VK_FALSE;
		galaxyBoxShader->SetData(4);
		
		
		
		// Galaxy Fade
		galaxyFadeShader = new RasterShaderPipeline(galaxyBoxPipelineLayout, {
			"incubator_rendering/assets/shaders/v4d_galaxy.fade.vert",
			"incubator_rendering/assets/shaders/v4d_galaxy.fade.geom",
			"incubator_rendering/assets/shaders/v4d_galaxy.fade.frag",
		});
		galaxyFadeShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		galaxyFadeShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		galaxyFadeShader->depthStencilState.depthWriteEnable = VK_FALSE;
		galaxyFadeShader->depthStencilState.depthTestEnable = VK_FALSE;
		galaxyFadeShader->SetData(6);
		
		
		
		// Objects
		
		// Descriptor sets & Pipeline Layouts
		auto* descriptorSet = descriptorSets.emplace_back(new DescriptorSet(0));
		descriptorSet->AddBinding_uniformBuffer(0, &uniformBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		mainPipelineLayout.AddDescriptorSet(descriptorSet);
		auto* ppDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(0));
		ppDescriptorSet->AddBinding_combinedImageSampler(0, &colorImage, VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingPipelineLayout.AddDescriptorSet(ppDescriptorSet);
		
		Buffer* vertexBuffer = stagedBuffers.emplace_back(new Buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
		Buffer* indexBuffer = stagedBuffers.emplace_back(new Buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
		
		// Add triangle geometries
		auto* trianglesGeometry1 = new TriangleGeometry<Vertex>({
			{/*pos*/-0.5,-0.5, 0.0, /*color*/{1.0, 0.0, 0.0, 1.0}},
			{/*pos*/ 0.5,-0.5, 0.0, /*color*/{1.0, 0.0, 0.0, 1.0}},
			{/*pos*/ 0.5, 0.5, 0.0, /*color*/{1.0, 0.0, 0.0, 1.0}},
			{/*pos*/-0.5, 0.5, 0.0, /*color*/{1.0, 0.0, 0.0, 1.0}},
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
		testShader->SetData(vertexBuffer, indexBuffer);
		
		// Post processing
		// Shader program
		ppShader = new RasterShaderPipeline(postProcessingPipelineLayout, {
			"incubator_rendering/assets/shaders/v4d_postProcessing.vert",
			"incubator_rendering/assets/shaders/v4d_postProcessing.frag",
		});
		ppShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		ppShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		ppShader->depthStencilState.depthTestEnable = VK_FALSE;
		ppShader->depthStencilState.depthWriteEnable = VK_FALSE;
		ppShader->SetData(3);
		
		
		// Compute shader
		auto* galaxiesComputeDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(1));
		galaxiesComputeDescriptorSet->AddBinding_imageView(0, &galaxyCubeImage.view, VK_SHADER_STAGE_COMPUTE_BIT);
		galaxyGenPipelineLayout.AddDescriptorSet(galaxiesComputeDescriptorSet);
		computeTestShader = new ComputeShaderPipeline(galaxyGenPipelineLayout, "incubator_rendering/assets/shaders/compute_test.comp");
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
		
		// Descriptor sets
		for (auto* set : descriptorSets) {
			delete set;
		}
		descriptorSets.clear();
		
		// Pipeline layouts
		mainPipelineLayout.Reset();
		postProcessingPipelineLayout.Reset();
		galaxyGenPipelineLayout.Reset();
		galaxyBoxPipelineLayout.Reset();
	}

	void ReadShaders() override {
		galaxyGenShader->ReadShaders();
		galaxyBoxShader->ReadShaders();
		galaxyFadeShader->ReadShaders();
		testShader->ReadShaders();
		ppShader->ReadShaders();
		computeTestShader->ReadShaders();
	}

protected: // Graphics configuration
	void AllocateBuffers() override {
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

	void FreeBuffers() override {
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
	
	void CreatePipelines() override {
		// Pipeline layouts
		mainPipelineLayout.Create(renderingDevice);
		postProcessingPipelineLayout.Create(renderingDevice);
		galaxyGenPipelineLayout.Create(renderingDevice);
		galaxyBoxPipelineLayout.Create(renderingDevice);
		
		{// Main Render pass
			// Color Attachment (Fragment shader Standard Output)
			VkAttachmentDescription colorAttachment = {}; // defines the output data from the fragment shader (o_color)
				colorAttachment.format = colorImage.format;
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
				depthAttachment.format = depthImage.format;
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
			renderPass.CreateFrameBuffers(renderingDevice, {
				&colorImage,
				&depthImage,
			});
			
			// Shaders
			for (auto* shaderPipeline : {galaxyBoxShader, testShader}) {
				shaderPipeline->SetRenderPass(&colorImage, renderPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Post Processing render pass
			// Color Attachment (Fragment shader Standard Output)
			VkAttachmentDescription colorAttachment = {}; // defines the output data from the fragment shader (o_color)
				colorAttachment.format = ppImage.format;
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
			postProcessingRenderPass.CreateFrameBuffers(renderingDevice, swapChain);
			
			// Shaders
			for (auto* shaderPipeline : {ppShader}) {
				shaderPipeline->SetRenderPass(swapChain, postProcessingRenderPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Galaxy Gen render pass
			// Color Attachment (Fragment shader Standard Output)
			VkAttachmentDescription colorAttachment = {}; // defines the output data from the fragment shader (o_color)
				colorAttachment.format = galaxyCubeImage.format;
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
			galaxyGenRenderPass.CreateFrameBuffers(renderingDevice, galaxyCubeImage);
			
			// Shader pipeline
			galaxyGenShader->SetRenderPass(&galaxyCubeImage, galaxyGenRenderPass.handle, 0);
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
				colorAttachment.format = galaxyCubeImage.format;
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
			galaxyFadeRenderPass.CreateFrameBuffers(renderingDevice, galaxyCubeImage);
			
			// Shader pipeline
			galaxyFadeShader->SetRenderPass(&galaxyCubeImage, galaxyFadeRenderPass.handle, 0);
			galaxyFadeShader->AddColorBlendAttachmentState(
				/*blendEnable*/				VK_TRUE,
				/*srcColorBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*dstColorBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*colorBlendOp*/			VK_BLEND_OP_REVERSE_SUBTRACT,
				/*srcAlphaBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*dstAlphaBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*alphaBlendOp*/			VK_BLEND_OP_MAX
			);
			// Shader stages
			galaxyFadeShader->CreatePipeline(renderingDevice);
		}
		
		// Compute test
		computeTestShader->CreatePipeline(renderingDevice);
	}
	
	void DestroyPipelines() override {
		
		// Main render pass
		for (auto& shaderPipeline : {galaxyBoxShader, testShader}) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		renderPass.DestroyFrameBuffers(renderingDevice);
		renderPass.Destroy(renderingDevice);
		
		// Post-Processing
		ppShader->DestroyPipeline(renderingDevice);
		postProcessingRenderPass.DestroyFrameBuffers(renderingDevice);
		postProcessingRenderPass.Destroy(renderingDevice);
		
		// Galaxy Gen
		galaxyGenShader->DestroyPipeline(renderingDevice);
		galaxyGenRenderPass.DestroyFrameBuffers(renderingDevice);
		galaxyGenRenderPass.Destroy(renderingDevice);
		
		// Galaxy Fade
		galaxyFadeShader->DestroyPipeline(renderingDevice);
		galaxyFadeRenderPass.DestroyFrameBuffers(renderingDevice);
		galaxyFadeRenderPass.Destroy(renderingDevice);
		
		// Compute test
		computeTestShader->DestroyPipeline(renderingDevice);
		
		// Pipeline layouts
		mainPipelineLayout.Destroy(renderingDevice);
		postProcessingPipelineLayout.Destroy(renderingDevice);
		galaxyGenPipelineLayout.Destroy(renderingDevice);
		galaxyBoxPipelineLayout.Destroy(renderingDevice);
	}
	
protected: // Commands
	
	void RecordLowPriorityGraphicsCommandBuffer(VkCommandBuffer commandBuffer) {
		
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
		galaxyGenRenderPass.Begin(renderingDevice, commandBuffer, galaxyCubeImage);
		galaxyGenShader->Execute(renderingDevice, commandBuffer);
		galaxyGenRenderPass.End(renderingDevice, commandBuffer);
		renderingDevice->CmdEndConditionalRenderingEXT(commandBuffer);
		
		// Other galaxy elements
		//...
		
		// Galaxy Fade
		conditionalRenderingInfo.offset = offsetof(ConditionalRendering, ConditionalRendering::fadeGalaxy);
		renderingDevice->CmdBeginConditionalRenderingEXT(commandBuffer, &conditionalRenderingInfo);
		galaxyFadeRenderPass.Begin(renderingDevice, commandBuffer, galaxyCubeImage);
		galaxyFadeShader->Execute(renderingDevice, commandBuffer);
		galaxyFadeRenderPass.End(renderingDevice, commandBuffer);
		renderingDevice->CmdEndConditionalRenderingEXT(commandBuffer);
		
	}
	
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		renderPass.Begin(renderingDevice, commandBuffer, colorImage, {{.0,.0,.0,.0}, {1.0,.0}});
		galaxyBoxShader->Execute(renderingDevice, commandBuffer);
		testShader->Execute(renderingDevice, commandBuffer);
		renderPass.End(renderingDevice, commandBuffer);
		
		// Post Processing
		TransitionImageLayout(commandBuffer, colorImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		postProcessingRenderPass.Begin(renderingDevice, commandBuffer, ppImage, {{.0,.0,.0,.0}}, imageIndex);
		ppShader->Execute(renderingDevice, commandBuffer);
		postProcessingRenderPass.End(renderingDevice, commandBuffer);
	}
	
	void RecordComputeCommandBuffer(VkCommandBuffer, int imageIndex) override {}
	void RecordLowPriorityComputeCommandBuffer(VkCommandBuffer) override {}
	void RunDynamicCompute(VkCommandBuffer) override {}
	void RunDynamicGraphics(VkCommandBuffer) override {}
	void RunDynamicLowPriorityCompute(VkCommandBuffer) override {}
	void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {}
	
protected: // Methods executed on every frame
	void FrameUpdate(uint imageIndex) override {
		LockUBO();
		UBO ubo {};
		
		// Current camera position
		ubo.view = glm::lookAt(camPosition, camPosition + camDirection, glm::dvec3(0,0,1));
		// Projection
		ubo.proj = glm::perspective(glm::radians(80.0), (double) swapChain->extent.width / (double) swapChain->extent.height, 0.01, 1.5e17); // 1cm - 1 000 000 UA  (WTF!!! seems to be working great..... 32bit z-buffer is enough???)
		ubo.proj[1][1] *= -1;
		
		// ubo.velocity = glm::dvec4(velocity, speed);
		
		// Update memory
		uniformBuffer.WriteToMappedData(renderingDevice, &ubo);
		UnlockUBO();
	}
	
	void LowPriorityFrameUpdate() override {
		LockUBO();
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
				VkImageSubresourceRange clearRange {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,galaxyCubeImage.arrayLayers};
				auto cmdBuffer = BeginSingleTimeCommands(lowPriorityGraphicsQueue);
				TransitionImageLayout(cmdBuffer, galaxyCubeImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
				renderingDevice->CmdClearColorImage(cmdBuffer, galaxyCubeImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &clearRange);
				TransitionImageLayout(cmdBuffer, galaxyCubeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
				EndSingleTimeCommands(lowPriorityGraphicsQueue, cmdBuffer);
			}
		}
		
		// // Compute
		// auto cmdBuffer = BeginSingleTimeCommands(lowPriorityComputeQueue);
		// computeTestShader->SetGroupCounts(galaxyCubeImage.width, galaxyCubeImage.width, galaxyCubeImage.arrayLayers);
		// computeTestShader->Execute(renderingDevice, cmdBuffer);
		// EndSingleTimeCommands(lowPriorityComputeQueue, cmdBuffer);
		
		UnlockUBO();
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
	
	
	inline void LockUBO() {
		uboMutex.lock();
	}

	inline void UnlockUBO() {
		uboMutex.unlock();
	}

};
