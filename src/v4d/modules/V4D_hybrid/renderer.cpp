#define _V4D_MODULE
#include <v4d.h>

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::vulkan::rtx;

#pragma region Rendering Modes

	enum RenderingMode : int {
		rasterization,
		rayTracing,
	};
	const char* VISIBILITY_RENDER_MODES_STR = 
		"Rasterization\0"
		"Ray-Tracing\0"
	;
	enum ShadowType : int {
		shadows_off,
		shadows_hard,
		// shadows_soft,
	};
	const char* SHADOW_TYPES_STR = 
		"Disabled\0"
		"Hard shadows\0"
		// "Soft shadows\0"
	;
	static const int DEFAULT_VISIBILITY_RENDER_MODE = rasterization;
	static const int DEFAULT_LIGHTING_RENDER_MODE = rayTracing;
	static const int DEFAULT_SHADOW_TYPE = shadows_hard;

#pragma endregion

#pragma region Limits
	const uint32_t MAX_ACTIVE_LIGHTS = 256;
#pragma endregion

// Application
Renderer* r = nullptr;
Scene* scene = nullptr;

#pragma region Descriptor Sets
	DescriptorSet set0_base;
	DescriptorSet set1_visibility_raster;
	DescriptorSet set1_visibility_rays;
	DescriptorSet set1_lighting_and_fog;
	DescriptorSet set1_post;
	DescriptorSet set1_thumbnail;
	DescriptorSet set1_histogram;
	DescriptorSet set1_overlay;
#pragma endregion

#pragma region Images
	DepthImage img_rasterDepth { VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
	Image img_tmpBuffer { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT }};
	Image img_depth { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32_SFLOAT } };
	Image img_gBuffer_0 { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R8G8_SNORM }};
	Image img_gBuffer_1 { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT }};
	Image img_gBuffer_2 { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT }};
	Image img_gBuffer_3 { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R8G8B8A8_SNORM }};
	Image img_lit { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_pp { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_history { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_thumbnail { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_overlay { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8G8B8A8_SNORM }};
	
#pragma endregion

#pragma region Pipeline Layouts
	PipelineLayout pl_visibility_raster;
	PipelineLayout pl_visibility_rays;
	PipelineLayout pl_lighting_raster;
	PipelineLayout pl_lighting_rays;
	PipelineLayout pl_fog_raster;
	PipelineLayout pl_overlay;
	PipelineLayout pl_post;
	PipelineLayout pl_thumbnail;
	PipelineLayout pl_histogram;
#pragma endregion

#pragma region Shaders

	// Visibility
	RasterShaderPipeline shader_visibility {pl_visibility_raster, {
		"modules/V4D_hybrid/assets/shaders/raster_visibility.vert",
		"modules/V4D_hybrid/assets/shaders/raster_visibility.frag",
	}};
	// #ifdef _DEBUG
		RasterShaderPipeline shader_debug_wireframe {pl_visibility_raster, {
			"modules/V4D_hybrid/assets/shaders/raster_visibility.vert",
			"modules/V4D_hybrid/assets/shaders/raster_visibility.frag",
		}};
	// #endif
	RasterShaderPipeline shader_glass {pl_visibility_raster, {
		"modules/V4D_hybrid/assets/shaders/raster_glass.vert",
		"modules/V4D_hybrid/assets/shaders/raster_glass.frag",
	}};
	
	// Main Rendering
	RasterShaderPipeline shader_lighting {pl_lighting_raster, {
		"modules/V4D_hybrid/assets/shaders/raster_lighting.vert",
		"modules/V4D_hybrid/assets/shaders/raster_lighting.frag",
	}};
	
	// Ray Tracing
	ShaderBindingTable sbt_visibility {pl_visibility_rays, "modules/V4D_hybrid/assets/shaders/rtx_visibility.rgen"};
	ShaderBindingTable sbt_lighting {pl_lighting_rays, "modules/V4D_hybrid/assets/shaders/rtx_lighting.rgen"};
	
	// Post Processing
	RasterShaderPipeline shader_thumbnail {pl_thumbnail, {
		"modules/V4D_hybrid/assets/shaders/v4d_thumbnail.vert",
		"modules/V4D_hybrid/assets/shaders/v4d_thumbnail.frag",
	}};
	RasterShaderPipeline shader_txaa {pl_post, {
		"modules/V4D_hybrid/assets/shaders/v4d_post.vert",
		"modules/V4D_hybrid/assets/shaders/v4d_post.txaa.frag",
	}, 100};
	RasterShaderPipeline shader_history_write {pl_post, {
		"modules/V4D_hybrid/assets/shaders/v4d_post.vert",
		"modules/V4D_hybrid/assets/shaders/v4d_post.history_write.frag",
	}};
	RasterShaderPipeline shader_hdr {pl_post, {
		"modules/V4D_hybrid/assets/shaders/v4d_post.vert",
		"modules/V4D_hybrid/assets/shaders/v4d_post.hdr.frag",
	}, -100};
	RasterShaderPipeline shader_overlay_apply {pl_post, {
		"modules/V4D_hybrid/assets/shaders/v4d_post.vert",
		"modules/V4D_hybrid/assets/shaders/v4d_post.overlay_apply.frag",
	}, 100};
	ComputeShaderPipeline shader_histogram_compute {pl_histogram, 
		"modules/V4D_hybrid/assets/shaders/v4d_histogram.comp"
	};
	 
#pragma endregion

#pragma region Containers
	// Pipelines
	std::unordered_map<std::string, Image*> images {
		{"img_rasterDepth", &img_rasterDepth},
		{"img_tmpBuffer", &img_tmpBuffer},
		{"img_depth", &img_depth},
		{"img_gBuffer_0", &img_gBuffer_0},
		{"img_gBuffer_1", &img_gBuffer_1},
		{"img_gBuffer_2", &img_gBuffer_2},
		{"img_gBuffer_3", &img_gBuffer_3},
		{"img_lit", &img_lit},
		{"img_pp", &img_pp},
		{"img_history", &img_history},
		{"img_thumbnail", &img_thumbnail},
		{"img_overlay", &img_overlay},
	};
	std::unordered_map<std::string, PipelineLayout*> pipelineLayouts {
		{"pl_visibility_raster", &pl_visibility_raster},
		{"pl_visibility_rays", &pl_visibility_rays},
		{"pl_lighting_raster", &pl_lighting_raster},
		{"pl_lighting_rays", &pl_lighting_rays},
		{"pl_fog_raster", &pl_fog_raster},
		{"pl_overlay", &pl_overlay},
		{"pl_post", &pl_post},
		{"pl_thumbnail", &pl_thumbnail},
		{"pl_histogram", &pl_histogram},
	};
	std::unordered_map<std::string, std::vector<RasterShaderPipeline*>> shaderGroups {
		{"sg_visibility", {&shader_visibility,
			// #ifdef _DEBUG
				&shader_debug_wireframe,
			// #endif
		}},
		{"sg_lighting", {&shader_lighting}},
		{"sg_fog", {}},
		{"sg_glass", {&shader_glass}},
		{"sg_thumbnail", {&shader_thumbnail}},
		{"sg_postfx", {&shader_txaa}},
		{"sg_history_write", {&shader_history_write}},
		{"sg_present", {&shader_hdr, &shader_overlay_apply}},
		{"sg_overlay", {}},
	};
	std::unordered_map<std::string, ShaderBindingTable*> shaderBindingTables {
		{"sbt_visibility", &sbt_visibility},
		{"sbt_lighting", &sbt_lighting},
	};
	// G-Buffers
	static const int NB_G_BUFFERS = 4;
	std::array<Image*, NB_G_BUFFERS> gBuffers {
		&img_gBuffer_0, // R=snorm8(metallic), G=snorm8(roughness)
		&img_gBuffer_1, // RGB=sfloat32(normal.xyz), A=sfloat32(packed(uv))
		&img_gBuffer_2, // RGB=sfloat32(viewPosition.xyz), A=sfloat32(realDistanceFromCamera)
		&img_gBuffer_3, // RGB=snorm8(albedo.rgb), A=snorm8(emit?)
	};
	// FrameBuffers
	std::unordered_map<std::string, std::vector<VkSemaphore>> semaphores {};
	std::unordered_map<std::string, std::vector<VkFence>> fences {};
	std::unordered_map<std::string, std::vector<VkCommandBuffer>> commandBuffers {};
#pragma endregion

#pragma region Render Passes (Rasterization)
	RenderPass lightingRenderPass;
	RenderPass visibilityRasterPass;
	RenderPass postProcessingRenderPass;
	RenderPass thumbnailRenderPass;
	RenderPass uiRenderPass;
#pragma endregion

#pragma region Buffers
	StagedBuffer cameraUniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(Camera)};
	StagedBuffer activeLightsUniformBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
		uint32_t nbActiveLights = 0;
		uint32_t activeLights[MAX_ACTIVE_LIGHTS];
	Buffer totalLuminance {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(glm::vec4)};
#pragma endregion

#pragma region Rasterization Pipelines & Resources

	struct GeometryPushConstant {
		uint objectIndex;
		uint geometryIndex;
	};

	void CreateRasterVisibilityResources() {
		uint rasterWidth = (uint)((float)r->swapChain->extent.width);
		uint rasterHeight = (uint)((float)r->swapChain->extent.height);
		
		auto commandBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
		
			for (auto* img : gBuffers)
				img->Create(r->renderingDevice, rasterWidth, rasterHeight);
			for (auto* img : gBuffers)
				r->TransitionImageLayout(commandBuffer, *img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
				
			img_rasterDepth.viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			img_rasterDepth.Create(r->renderingDevice, rasterWidth, rasterHeight);
			r->TransitionImageLayout(commandBuffer, img_rasterDepth, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			
			img_tmpBuffer.Create(r->renderingDevice, rasterWidth, rasterHeight);
			r->TransitionImageLayout(commandBuffer, img_tmpBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
	}
	void DestroyRasterVisibilityResources() {
		for (auto* img : gBuffers)
			img->Destroy(r->renderingDevice);
		img_rasterDepth.Destroy(r->renderingDevice);
		img_tmpBuffer.Destroy(r->renderingDevice);
	}
	
	void CreateRenderingResources() {
		uint rasterWidth = (uint)((float)r->swapChain->extent.width);
		uint rasterHeight = (uint)((float)r->swapChain->extent.height);
		
		img_lit.Create(r->renderingDevice, rasterWidth, rasterHeight);
		img_depth.Create(r->renderingDevice, rasterWidth, rasterHeight);
		
		auto commandBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
			r->TransitionImageLayout(commandBuffer, img_lit, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_depth, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
	}
	void DestroyRenderingResources() {
		img_lit.Destroy(r->renderingDevice);
		img_depth.Destroy(r->renderingDevice);
	}
	
	// Separate visibily and material passes
	void CreateRasterVisibilityPipeline() {
		std::array<Image*, NB_G_BUFFERS+1> attachmentImages {};
		std::array<VkAttachmentDescription, NB_G_BUFFERS+1> attachments {};
		std::array<VkAttachmentReference, NB_G_BUFFERS> colorAttachmentRefs {};
		VkAttachmentReference depthAttachmentRef;
		int i = 0;
		// G-Buffers
		for (auto* img : gBuffers) {
			attachmentImages[i] = img;
			attachments[i].format = img->format;
			attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			colorAttachmentRefs[i] = {
				visibilityRasterPass.AddAttachment(attachments[i]),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			++i;
		}
		{ // Depth buffer
			attachmentImages[i] = &img_rasterDepth;
			attachments[i].format = img_rasterDepth.format;
			attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			depthAttachmentRef = {
				visibilityRasterPass.AddAttachment(attachments[i]),
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			};
			++i;
		}
		
		// SubPass
		VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = colorAttachmentRefs.size();
			subpass.pColorAttachments = colorAttachmentRefs.data();
			subpass.pDepthStencilAttachment = &depthAttachmentRef;
		visibilityRasterPass.AddSubpass(subpass);
		
		// Create the render pass
		visibilityRasterPass.Create(r->renderingDevice);
		visibilityRasterPass.CreateFrameBuffers(r->renderingDevice, attachmentImages.data(), attachmentImages.size());
		
		// Shaders
		for (auto* s : shaderGroups["sg_visibility"]) {
			s->SetRenderPass(*attachmentImages.data(), visibilityRasterPass.handle, 0);
			for (int i = 0; i < colorAttachmentRefs.size(); ++i)
				s->AddColorBlendAttachmentState(VK_FALSE);
			s->CreatePipeline(r->renderingDevice);
		}
	}
	
	void DestroyRasterVisibilityPipeline() {
		for (auto* s : shaderGroups["sg_visibility"]) {
			s->DestroyPipeline(r->renderingDevice);
		}
		visibilityRasterPass.DestroyFrameBuffers(r->renderingDevice);
		visibilityRasterPass.Destroy(r->renderingDevice);
	}
	
	void CreateLightingPipeline() {
		const int nbColorAttachments = 1;
		const int nbInputAttachments = NB_G_BUFFERS;
		std::array<VkAttachmentDescription, nbColorAttachments + nbInputAttachments> attachments {};
		std::vector<Image*> attachmentImages(nbColorAttachments + nbInputAttachments);
		std::array<VkAttachmentReference, nbColorAttachments> colorAttachmentRefs0 {};
		std::array<VkAttachmentReference, nbInputAttachments> inputAttachmentRefs0 {};
		
		int i = 0;
		
		// Color attachment
		attachmentImages[i] = &img_lit;
		attachments[i].format = img_lit.format;
		attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[i].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
		attachments[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		colorAttachmentRefs0[0] = {
			lightingRenderPass.AddAttachment(attachments[i]),
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};
		++i;
		
		// Input attachments
		for (auto* img : gBuffers) {
			attachmentImages[i] = img;
			attachments[i].format = img->format;
			attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[i].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
			attachments[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			inputAttachmentRefs0[i-nbColorAttachments] = {
				lightingRenderPass.AddAttachment(attachments[i]),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
			++i;
		}
		
		std::array<VkAttachmentReference, nbColorAttachments> colorAttachmentRefs1 = colorAttachmentRefs0;
		std::array<VkAttachmentReference, nbInputAttachments> inputAttachmentRefs1 = inputAttachmentRefs0;
	
		// SubPasses
		std::array<VkSubpassDependency, 2> subpassDependencies {
			VkSubpassDependency{
				VK_SUBPASS_EXTERNAL,// srcSubpass;
				0,// dstSubpass;
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// srcStageMask;
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// dstStageMask;
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// srcAccessMask;
				VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// dstAccessMask;
				0// dependencyFlags;
			},
			VkSubpassDependency{
				0,// srcSubpass;
				1,// dstSubpass;
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// srcStageMask;
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// dstStageMask;
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// srcAccessMask;
				VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// dstAccessMask;
				0// dependencyFlags;
			},
		};
		std::array<VkSubpassDescription, 2> subpasses {};
			subpasses[0] = {};
			subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpasses[0].colorAttachmentCount = colorAttachmentRefs0.size();
			subpasses[0].pColorAttachments = colorAttachmentRefs0.data();
			subpasses[0].inputAttachmentCount = inputAttachmentRefs0.size();
			subpasses[0].pInputAttachments = inputAttachmentRefs0.data();
			subpasses[1] = {};
			subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpasses[1].colorAttachmentCount = colorAttachmentRefs1.size();
			subpasses[1].pColorAttachments = colorAttachmentRefs1.data();
			subpasses[1].inputAttachmentCount = inputAttachmentRefs1.size();
			subpasses[1].pInputAttachments = inputAttachmentRefs1.data();
		lightingRenderPass.AddSubpass(subpasses[0]);
		lightingRenderPass.AddSubpass(subpasses[1]);
		lightingRenderPass.renderPassInfo.dependencyCount = subpassDependencies.size();
		lightingRenderPass.renderPassInfo.pDependencies = subpassDependencies.data();
		
		// Create the render pass
		lightingRenderPass.Create(r->renderingDevice);
		lightingRenderPass.CreateFrameBuffers(r->renderingDevice, attachmentImages);
		
		// Shaders
		for (auto* s : shaderGroups["sg_lighting"]) {
			s->SetRenderPass(&img_lit, lightingRenderPass.handle, 0);
			s->AddColorBlendAttachmentState();
			s->CreatePipeline(r->renderingDevice);
		}
		for (auto* s : shaderGroups["sg_fog"]) {
			s->SetRenderPass(&img_lit, lightingRenderPass.handle, 1);
			s->AddColorBlendAttachmentState();
			s->CreatePipeline(r->renderingDevice);
		}
	}
	void DestroyLightingPipeline() {
		for (auto* s : shaderGroups["sg_lighting"]) {
			s->DestroyPipeline(r->renderingDevice);
		}
		for (auto* s : shaderGroups["sg_fog"]) {
			s->DestroyPipeline(r->renderingDevice);
		}
		lightingRenderPass.DestroyFrameBuffers(r->renderingDevice);
		lightingRenderPass.Destroy(r->renderingDevice);
	}
	
	void ConfigureRasterVisibilityShaders() {
		pl_visibility_raster.AddPushConstant<GeometryPushConstant>(VK_SHADER_STAGE_ALL_GRAPHICS);
		
		shader_visibility.AddVertexInputBinding(sizeof(Geometry::VertexBuffer_T), VK_VERTEX_INPUT_RATE_VERTEX, Geometry::VertexBuffer_T::GetInputAttributes());
		#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
			shader_visibility.SetData(&Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer);
		#else
			shader_visibility.SetData(&Geometry::globalBuffers.vertexBuffer, &Geometry::globalBuffers.indexBuffer);
		#endif
		
		// #ifdef _DEBUG
			shader_debug_wireframe.AddVertexInputBinding(sizeof(Geometry::VertexBuffer_T), VK_VERTEX_INPUT_RATE_VERTEX, Geometry::VertexBuffer_T::GetInputAttributes());
			shader_debug_wireframe.rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
			shader_debug_wireframe.rasterizer.lineWidth = 1;
			shader_debug_wireframe.rasterizer.cullMode = VK_CULL_MODE_NONE;
			#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
				shader_debug_wireframe.SetData(&Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer);
			#else
				shader_debug_wireframe.SetData(&Geometry::globalBuffers.vertexBuffer, &Geometry::globalBuffers.indexBuffer);
			#endif
		// #endif
		
		for (auto* s : shaderGroups["sg_material"]) {
			s->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			s->depthStencilState.depthTestEnable = VK_FALSE;
			s->depthStencilState.depthWriteEnable = VK_FALSE;
			s->rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
			s->SetData(3);
		}
	}
	
	void ConfigureLightingShaders() {
		for (auto[rp, ss] : shaderGroups) if (rp == "sg_lighting" || rp == "sg_fog") {
			for (auto* s : ss) {
				s->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				s->depthStencilState.depthTestEnable = VK_FALSE;
				s->depthStencilState.depthWriteEnable = VK_FALSE;
				s->rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
				s->SetData(3);
			}
		}
	}
	
	void ClearLitImage(VkCommandBuffer commandBuffer) {
		r->TransitionImageLayout(commandBuffer, img_lit, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			const VkClearColorValue clearValues = {0,0,0,0};
			VkImageSubresourceRange range {VK_IMAGE_ASPECT_COLOR_BIT, 0,1,0,1};
			r->renderingDevice->CmdClearColorImage(commandBuffer, img_lit.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValues, 1, &range);
		r->TransitionImageLayout(commandBuffer, img_lit, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	}
	
	void RunRasterVisibility(Device* device, VkCommandBuffer commandBuffer) {
		std::vector<VkClearValue> clearValues(NB_G_BUFFERS+1);
		int i = 0;
		for (; i < NB_G_BUFFERS; ++i)
			clearValues[i] = {.0,.0,.0,.0};
		clearValues[i] = {0.0f,0}; // depth
		
		// Visibility Raster pass
		visibilityRasterPass.Begin(r->renderingDevice, commandBuffer, **gBuffers.data(), clearValues);
			for (auto* s : shaderGroups["sg_visibility"]) {
				if (
					s == &shader_visibility
					// #ifdef _DEBUG
						|| s == &shader_debug_wireframe
					// #endif
				) {
					// #ifdef _DEBUG
						if (scene->camera.debug != (s == &shader_debug_wireframe)) {
							continue;
						}
					// #endif
					scene->Lock();
						for (auto* obj : scene->objectInstances) {
							if (obj) {
								// obj->Lock();
								if (obj->IsActive()) {
									// Geometries
									for (auto& geom : obj->GetGeometries()) {
										if (geom.geometry->active) {
											// Frustum culling
											if (scene->camera.IsVisibleInScreen((obj->GetWorldTransform() * glm::dmat4(geom.transform))[3], geom.geometry->boundingDistance)) {
												if (geom.geometry->isProcedural) {
													//TODO procedural objects
												} else {
													s->vertexOffset = geom.geometry->vertexOffset * sizeof(Geometry::VertexBuffer_T);
													s->indexOffset = geom.geometry->indexOffset * sizeof(Geometry::IndexBuffer_T);
													s->indexCount = geom.geometry->indexCount;
													GeometryPushConstant geometryPushConstant {};
														geometryPushConstant.objectIndex = obj->GetObjectOffset();
														geometryPushConstant.geometryIndex = geom.geometry->geometryOffset;
													s->Execute(device, commandBuffer, 1, &geometryPushConstant);
												}
											}
										}
									}
								}
								// obj->Unlock();
							}
						}
					scene->Unlock();
				} else {
					s->Execute(device, commandBuffer);
				}
			}
		visibilityRasterPass.End(device, commandBuffer);
		
	}
	
	void RunLighting(Device* device, VkCommandBuffer commandBuffer, bool runSubpass0, bool runSubpass1) {
		if (!runSubpass0 && !runSubpass1) return;
		lightingRenderPass.Begin(device, commandBuffer, img_lit);
			if (runSubpass0) {
				for (auto* s : shaderGroups["sg_lighting"]) {
					s->Execute(device, commandBuffer);
				}
			}
			r->renderingDevice->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
			if (runSubpass1) {
				for (auto* s : shaderGroups["sg_fog"]) {
					s->Execute(device, commandBuffer);
				}
			}
		lightingRenderPass.End(device, commandBuffer);
	}
	
#pragma endregion

#pragma region Ray-Tracing Pipeliens & Resouces
	VkPhysicalDeviceRayTracingPropertiesKHR rayTracingProperties{};
	AccelerationStructure topLevelAccelerationStructure {};
	Buffer rayTracingShaderBindingTableBuffer_visibility {VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR};
	Buffer rayTracingShaderBindingTableBuffer_lighting {VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR};
	std::mutex rayTracingInstanceMutex, blasBuildQueueMutex;
	std::vector<VkAccelerationStructureBuildGeometryInfoKHR> blasQueueBuildGeometryInfos {};
	std::vector<VkAccelerationStructureBuildOffsetInfoKHR*> blasQueueBuildOffsetInfos {};
	Buffer rayTracingInstanceBuffer {VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, sizeof(RayTracingBLASInstance)*RAY_TRACING_TLAS_MAX_INSTANCES};
	RayTracingBLASInstance* rayTracingInstances = nullptr;
	uint32_t nbRayTracingInstances = 0;
	Buffer globalScratchBuffer {VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 16*1024*1024/* 16 MB */};
	static const bool globalScratchDynamicSize = false;
	static const int maxBlasBuildsPerFrame = 8;
	std::map<int/*instance index*/, std::shared_ptr<Geometry>> activeRayTracedGeometries {};
	std::vector<std::shared_ptr<AccelerationStructure>> inactiveBlas {};
	std::vector<std::shared_ptr<AccelerationStructure>> blasBuildsForGlobalScratchBufferReallocation {};
	
	void ResetRayTracingBlasBuilds() {
		std::lock_guard lock(blasBuildQueueMutex);
		blasQueueBuildGeometryInfos.clear();
		blasQueueBuildOffsetInfos.clear();
	}
	
	void CreateRayTracingResources() {
		topLevelAccelerationStructure.AssignTopLevel();
		topLevelAccelerationStructure.Create(r->renderingDevice, true);
		topLevelAccelerationStructure.Allocate(r->renderingDevice);
		topLevelAccelerationStructure.SetInstanceBuffer(r->renderingDevice, rayTracingInstanceBuffer.buffer);
	}
	void DestroyRayTracingResources() {
		topLevelAccelerationStructure.Free(r->renderingDevice);
		topLevelAccelerationStructure.Destroy(r->renderingDevice);
		
		activeRayTracedGeometries.clear();
		inactiveBlas.clear();
		blasBuildsForGlobalScratchBufferReallocation.clear();
		ResetRayTracingBlasBuilds();
	}
	
	void AllocateRayTracingBuffers() {
		rayTracingInstanceBuffer.Allocate(r->renderingDevice, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		rayTracingInstanceBuffer.MapMemory(r->renderingDevice);
		rayTracingInstances = (RayTracingBLASInstance*)rayTracingInstanceBuffer.data;
		
		// LOG_VERBOSE("Allocated instance buffer " << std::hex << r->renderingDevice->GetBufferDeviceAddress(rayTracingInstanceBuffer.buffer))
		
		if (AccelerationStructure::useGlobalScratchBuffer) {
			globalScratchBuffer.Allocate(r->renderingDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);
			topLevelAccelerationStructure.SetGlobalScratchBuffer(r->renderingDevice, globalScratchBuffer.buffer);
		}
		
		// LOG_VERBOSE("Allocated global scratch buffer " << std::hex << r->renderingDevice->GetBufferDeviceAddress(globalScratchBuffer.buffer))
	}
	void FreeRayTracingBuffers() {
		rayTracingInstances = nullptr;
		nbRayTracingInstances = 0;
		rayTracingInstanceBuffer.UnmapMemory(r->renderingDevice);
		rayTracingInstanceBuffer.Free(r->renderingDevice);
		
		if (AccelerationStructure::useGlobalScratchBuffer) {
			globalScratchBuffer.Free(r->renderingDevice);
		}
	}
	
	void CreateRayTracingPipeline() {
		{
			auto* sbt = shaderBindingTables["sbt_visibility"];
			sbt->CreateRayTracingPipeline(r->renderingDevice);
			rayTracingShaderBindingTableBuffer_visibility.size = sbt->GetSbtBufferSize(rayTracingProperties);
			rayTracingShaderBindingTableBuffer_visibility.Allocate(r->renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			sbt->WriteShaderBindingTableToBuffer(r->renderingDevice, &rayTracingShaderBindingTableBuffer_visibility, 0, rayTracingProperties);
		}
		{
			auto* sbt = shaderBindingTables["sbt_lighting"];
			sbt->CreateRayTracingPipeline(r->renderingDevice);
			rayTracingShaderBindingTableBuffer_lighting.size = sbt->GetSbtBufferSize(rayTracingProperties);
			rayTracingShaderBindingTableBuffer_lighting.Allocate(r->renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			sbt->WriteShaderBindingTableToBuffer(r->renderingDevice, &rayTracingShaderBindingTableBuffer_lighting, 0, rayTracingProperties);
		}
	}
	void DestroyRayTracingPipeline() {
		rayTracingShaderBindingTableBuffer_visibility.Free(r->renderingDevice);
		rayTracingShaderBindingTableBuffer_lighting.Free(r->renderingDevice);
		shaderBindingTables["sbt_visibility"]->DestroyRayTracingPipeline(r->renderingDevice);
		shaderBindingTables["sbt_lighting"]->DestroyRayTracingPipeline(r->renderingDevice);
	}
	
	void ConfigureRayTracingShaders() {
		shaderBindingTables["sbt_visibility"]->AddMissShader("modules/V4D_hybrid/assets/shaders/rtx_visibility.rmiss");
		shaderBindingTables["sbt_visibility"]->AddMissShader("modules/V4D_hybrid/assets/shaders/rtx_visibility.shadow.rmiss");
		Geometry::rayTracingShaderOffsets["standard"] = shaderBindingTables["sbt_visibility"]->AddHitShader("modules/V4D_hybrid/assets/shaders/rtx_visibility.rchit" /*, "modules/V4D_hybrid/assets/shaders/rtx_visibility.rahit"*/ );
		Geometry::rayTracingShaderOffsets["sphere"] = shaderBindingTables["sbt_visibility"]->AddHitShader("modules/V4D_hybrid/assets/shaders/rtx_visibility.sphere.rchit", "", "modules/V4D_hybrid/assets/shaders/rtx_visibility.sphere.rint");
		Geometry::rayTracingShaderOffsets["light"] = shaderBindingTables["sbt_visibility"]->AddHitShader("modules/V4D_hybrid/assets/shaders/rtx_visibility.light.rchit", "", "modules/V4D_hybrid/assets/shaders/rtx_visibility.sphere.rint");
		//TODO sbt_lighting
	}
	
	void RunRayTracingVisibilityCommands(VkCommandBuffer commandBuffer) {
		auto* sbt = shaderBindingTables["sbt_visibility"];
		
		int width = (int)((float)r->swapChain->extent.width);
		int height = (int)((float)r->swapChain->extent.height);
		
		r->renderingDevice->CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, sbt->GetPipeline());
		sbt->GetPipelineLayout()->Bind(r->renderingDevice, commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
		r->renderingDevice->CmdTraceRaysKHR(
			commandBuffer, 
			sbt->GetRayGenBufferRegion(),
			sbt->GetRayMissBufferRegion(),
			sbt->GetRayHitBufferRegion(),
			sbt->GetRayCallableBufferRegion(),
			width, height, 1
		);
	}
	
	void RunRayTracingLightingCommands(VkCommandBuffer commandBuffer) {
		auto* sbt = shaderBindingTables["sbt_lighting"];
		
		int width = (int)((float)r->swapChain->extent.width);
		int height = (int)((float)r->swapChain->extent.height);
		
		r->renderingDevice->CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, sbt->GetPipeline());
		sbt->GetPipelineLayout()->Bind(r->renderingDevice, commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
		r->renderingDevice->CmdTraceRaysKHR(
			commandBuffer, 
			sbt->GetRayGenBufferRegion(),
			sbt->GetRayMissBufferRegion(),
			sbt->GetRayHitBufferRegion(),
			sbt->GetRayCallableBufferRegion(),
			width, height, 1
		);
	}
	
	void BuildBottomLevelRayTracingAccelerationStructures(Device* device, VkCommandBuffer commandBuffer) {
		// Build all new/updated bottom levels
		std::lock_guard lock(blasBuildQueueMutex);
		if (blasQueueBuildGeometryInfos.size() > 0) {
			// #ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
			// 	VkBufferMemoryBarrier bufferBarriers[2];
			// 	bufferBarriers[0] = {};
			// 		bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			// 		bufferBarriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
			// 		bufferBarriers[0].dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			// 		bufferBarriers[0].offset = 0;
			// 		bufferBarriers[0].size = Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer.size;
			// 		bufferBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			// 		bufferBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			// 		bufferBarriers[0].buffer = Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer.buffer;
			// 	bufferBarriers[1] = {};
			// 		bufferBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			// 		bufferBarriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
			// 		bufferBarriers[1].dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			// 		bufferBarriers[1].offset = 0;
			// 		bufferBarriers[1].size = Geometry::globalBuffers.indexBuffer.deviceLocalBuffer.size;
			// 		bufferBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			// 		bufferBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			// 		bufferBarriers[1].buffer = Geometry::globalBuffers.indexBuffer.deviceLocalBuffer.buffer;
			// 	device->CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 0, nullptr, 2, bufferBarriers, 0, nullptr);
			// #endif
			
			device->CmdBuildAccelerationStructureKHR(commandBuffer, blasQueueBuildGeometryInfos.size(), blasQueueBuildGeometryInfos.data(), blasQueueBuildOffsetInfos.data());
			
			VkMemoryBarrier memoryBarrier {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				nullptr,// pNext
				VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,// VkAccessFlags srcAccessMask
				VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,// VkAccessFlags dstAccessMask
			};
			device->CmdPipelineBarrier(
				commandBuffer, 
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
				0, 
				1, &memoryBarrier, 
				0, 0, 
				0, 0
			);
		}
	}
	
	void BuildTopLevelRayTracingAccelerationStructure(Device* device, VkCommandBuffer commandBuffer) {
		std::lock_guard lock(rayTracingInstanceMutex);
		static const VkAccelerationStructureBuildOffsetInfoKHR* topLevelAccelerationStructureGeometriesOffsets = &topLevelAccelerationStructure.buildOffsetInfo;
		topLevelAccelerationStructure.SetInstanceCount(nbRayTracingInstances);
		device->CmdBuildAccelerationStructureKHR(commandBuffer, 1, &topLevelAccelerationStructure.buildGeometryInfo, &topLevelAccelerationStructureGeometriesOffsets);
	
		VkMemoryBarrier memoryBarrier {
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			nullptr,// pNext
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,// VkAccessFlags srcAccessMask
			VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT,// VkAccessFlags dstAccessMask
		};
		device->CmdPipelineBarrier(
			commandBuffer, 
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 
			0, 
			1, &memoryBarrier, 
			0, 0, 
			0, 0
		);
	}
	
	void AddRayTracingBlasBuild(std::shared_ptr<AccelerationStructure> blas) {
		std::lock_guard lock(blasBuildQueueMutex);
		if (blasQueueBuildGeometryInfos.size() < maxBlasBuildsPerFrame) {
			blasQueueBuildGeometryInfos.push_back(blas->buildGeometryInfo);
			blasQueueBuildOffsetInfos.push_back(&blas->buildOffsetInfo);
			blas->built = true;
		}
	}
	
	void MakeRayTracingBlas(GeometryInstance* geometryInstance) {
		std::lock_guard lock(blasBuildQueueMutex);
		geometryInstance->geometry->blas = std::make_shared<AccelerationStructure>();
		geometryInstance->geometry->blas->AssignBottomLevel(r->renderingDevice, geometryInstance->geometry);
		geometryInstance->geometry->blas->Create(r->renderingDevice);
		geometryInstance->geometry->blas->Allocate(r->renderingDevice);
	}
	
	void SetRayTracingInstanceTransform(GeometryInstance* geometryInstance, const glm::dmat4& objectViewTransform) {
		std::lock_guard lock(rayTracingInstanceMutex);
		if (geometryInstance->rayTracingInstanceIndex == -1) return;
		rayTracingInstances[geometryInstance->rayTracingInstanceIndex].transform = glm::transpose(glm::mat4(objectViewTransform * geometryInstance->transform));
	}
	
	void AddRayTracingInstance(GeometryInstance* geometryInstance) {
		std::lock_guard lock(rayTracingInstanceMutex);
		if (geometryInstance->rayTracingInstanceIndex != -1) return;
		if (!geometryInstance->geometry->blas || !geometryInstance->geometry->blas->handle) return;
		int index = nbRayTracingInstances++;
		rayTracingInstances[index].accelerationStructureHandle = geometryInstance->geometry->blas->handle;
		rayTracingInstances[index].customInstanceId = geometryInstance->geometry->geometryOffset;
		rayTracingInstances[index].mask = geometryInstance->geometry->rayTracingMask;
		rayTracingInstances[index].shaderInstanceOffset = Geometry::rayTracingShaderOffsets[geometryInstance->type];
		rayTracingInstances[index].flags = geometryInstance->geometry->flags;
		rayTracingInstances[index].transform = glm::mat3x4{0};
		geometryInstance->rayTracingInstanceIndex = index;
		activeRayTracedGeometries[geometryInstance->rayTracingInstanceIndex] = geometryInstance->geometry;
	}
	
	void RemoveRayTracingInstance(GeometryInstance* geometryInstance) {
		std::lock_guard lock(rayTracingInstanceMutex);
		if (geometryInstance->rayTracingInstanceIndex == -1) return;
		int lastIndex = --nbRayTracingInstances;
		int index = geometryInstance->rayTracingInstanceIndex;
		geometryInstance->rayTracingInstanceIndex = -1;
		rayTracingInstances[index] = rayTracingInstances[lastIndex];
		rayTracingInstances[lastIndex].accelerationStructureHandle = 0;
	
		// inactiveBlas.push_back(activeRayTracedGeometries[index]->blas);
		activeRayTracedGeometries[index] = activeRayTracedGeometries[lastIndex];
		activeRayTracedGeometries[lastIndex] = nullptr;
		
		if (rayTracingInstances[index].accelerationStructureHandle != 0) {
			for (auto* obj : scene->objectInstances) {
				for (auto& geom : obj->GetGeometries()) {
					if (geom.rayTracingInstanceIndex == lastIndex) {
						geom.rayTracingInstanceIndex = index;
						return;
					}
				}
			}
			LOG_ERROR("Object Instance to move to deleted instance index : Not Found")
		}
	}
	
#pragma endregion

#pragma region Post Processing
	
	float thumbnailScale = 1.0/16;
	float exposureFactor = 1.0;
	
	void CreatePostProcessingResources() {
		uint rasterWidth = (uint)((float)r->swapChain->extent.width);
		uint rasterHeight = (uint)((float)r->swapChain->extent.height);
		uint thumbnailWidth = (uint)((float)rasterWidth * thumbnailScale);
		uint thumbnailHeight = (uint)((float)rasterHeight * thumbnailScale);
		
		img_pp.Create(r->renderingDevice, rasterWidth, rasterHeight);
		img_history.Create(r->renderingDevice, rasterWidth, rasterHeight);
		img_thumbnail.Create(r->renderingDevice, thumbnailWidth, thumbnailHeight, {img_lit.format});
		
		auto commandBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
			r->TransitionImageLayout(commandBuffer, img_history, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_thumbnail, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
	}
	void DestroyPostProcessingResources() {
		img_pp.Destroy(r->renderingDevice);
		img_history.Destroy(r->renderingDevice);
		img_thumbnail.Destroy(r->renderingDevice);
	}
	
	void CreatePostProcessingPipeline() {
		{// Thumbnail Gen render pass
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = img_thumbnail.format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			VkAttachmentReference colorAttachmentRef = {
				thumbnailRenderPass.AddAttachment(colorAttachment),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
			thumbnailRenderPass.AddSubpass(subpass);
			
			// Create the render pass
			thumbnailRenderPass.Create(r->renderingDevice);
			thumbnailRenderPass.CreateFrameBuffers(r->renderingDevice, img_thumbnail);
			
			// Shaders
			for (auto* shaderPipeline : shaderGroups["sg_thumbnail"]) {
				shaderPipeline->SetRenderPass(&img_thumbnail, thumbnailRenderPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(r->renderingDevice);
			}
		}
		
		{// Post Processing render pass
			std::array<VkAttachmentDescription, 3> attachments {};
			// std::array<VkAttachmentReference, 0> inputAttachmentRefs {};
			
			// PP image
			attachments[0].format = img_pp.format;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			uint32_t ppAttachmentIndex = postProcessingRenderPass.AddAttachment(attachments[0]);
			
			// History image
			attachments[1].format = img_history.format;
			attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[1].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
			attachments[1].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			uint32_t historyAttachmentIndex = postProcessingRenderPass.AddAttachment(attachments[1]);
			
			// SwapChain image
			attachments[2].format = r->swapChain->format.format;
			attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			uint32_t presentAttachmentIndex = postProcessingRenderPass.AddAttachment(attachments[2]);
			
			// SubPasses
			VkAttachmentReference colorAttRef_0 { ppAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			{
				VkSubpassDescription subpass {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttRef_0;
				postProcessingRenderPass.AddSubpass(subpass);
			}
			VkAttachmentReference colorAttRef_1 { historyAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			VkAttachmentReference inputAtt_1 { ppAttachmentIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			{
				VkSubpassDescription subpass {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttRef_1;
				subpass.inputAttachmentCount = 1;
				subpass.pInputAttachments = &inputAtt_1;
				postProcessingRenderPass.AddSubpass(subpass);
			}
			VkAttachmentReference colorAttRef_2 { presentAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			VkAttachmentReference inputAtt_2 { ppAttachmentIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			uint32_t preserverAtt_2 = historyAttachmentIndex;
			{
				VkSubpassDescription subpass {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttRef_2;
				subpass.preserveAttachmentCount = 1;
				subpass.pPreserveAttachments = &preserverAtt_2;
				subpass.inputAttachmentCount = 1;
				subpass.pInputAttachments = &inputAtt_2;
				postProcessingRenderPass.AddSubpass(subpass);
			}
			
			std::array<VkSubpassDependency, 3> subPassDependencies {
				VkSubpassDependency{
					VK_SUBPASS_EXTERNAL,// srcSubpass;
					0,// dstSubpass;
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// srcStageMask;
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// dstStageMask;
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// srcAccessMask;
					VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// dstAccessMask;
					0// dependencyFlags;
				},
				VkSubpassDependency{
					0,// srcSubpass;
					1,// dstSubpass;
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// srcStageMask;
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// dstStageMask;
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// srcAccessMask;
					VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// dstAccessMask;
					0// dependencyFlags;
				},
				VkSubpassDependency{
					0,// srcSubpass;
					2,// dstSubpass;
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// srcStageMask;
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// dstStageMask;
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// srcAccessMask;
					VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// dstAccessMask;
					0// dependencyFlags;
				}
			};
			postProcessingRenderPass.renderPassInfo.dependencyCount = subPassDependencies.size();
			postProcessingRenderPass.renderPassInfo.pDependencies = subPassDependencies.data();
			
			// Create the render pass
			postProcessingRenderPass.Create(r->renderingDevice);
			postProcessingRenderPass.CreateFrameBuffers(r->renderingDevice, r->swapChain, {
				img_pp.view, 
				img_history.view, 
				VK_NULL_HANDLE
			});
			
			// Shaders
			for (auto* shader : shaderGroups["sg_postfx"]) {
				shader->SetRenderPass(r->swapChain, postProcessingRenderPass.handle, 0);
				shader->AddColorBlendAttachmentState();
				shader->CreatePipeline(r->renderingDevice);
			}
			for (auto* shader : shaderGroups["sg_history_write"]) {
				shader->SetRenderPass(r->swapChain, postProcessingRenderPass.handle, 1);
				shader->AddColorBlendAttachmentState(VK_FALSE);
				shader->CreatePipeline(r->renderingDevice);
			}
			for (auto* shader : shaderGroups["sg_present"]) {
				shader->SetRenderPass(r->swapChain, postProcessingRenderPass.handle, 2);
				shader->AddColorBlendAttachmentState();
				shader->CreatePipeline(r->renderingDevice);
			}
		}
		
		// Compute
		shader_histogram_compute.CreatePipeline(r->renderingDevice);
	}
	void DestroyPostProcessingPipeline() {
		// Thumbnail Gen
		for (auto* shaderPipeline : shaderGroups["sg_thumbnail"]) {
			shaderPipeline->DestroyPipeline(r->renderingDevice);
		}
		thumbnailRenderPass.DestroyFrameBuffers(r->renderingDevice);
		thumbnailRenderPass.Destroy(r->renderingDevice);
		
		// Post-processing
		for (auto* s : shaderGroups["sg_postfx"]) {
			s->DestroyPipeline(r->renderingDevice);
		}
		for (auto* s : shaderGroups["sg_history_write"]) {
			s->DestroyPipeline(r->renderingDevice);
		}
		for (auto* s : shaderGroups["sg_present"]) {
			s->DestroyPipeline(r->renderingDevice);
		}
		postProcessingRenderPass.DestroyFrameBuffers(r->renderingDevice);
		postProcessingRenderPass.Destroy(r->renderingDevice);
		
		// Compute
		shader_histogram_compute.DestroyPipeline(r->renderingDevice);
	}
	
	void ConfigurePostProcessingShaders() {
		// Thumbnail Gen
		shader_thumbnail.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		shader_thumbnail.depthStencilState.depthTestEnable = VK_FALSE;
		shader_thumbnail.depthStencilState.depthWriteEnable = VK_FALSE;
		shader_thumbnail.rasterizer.cullMode = VK_CULL_MODE_NONE;
		shader_thumbnail.SetData(3);
		
		// Post-Processing
		for (auto[rp, ss] : shaderGroups) if (rp == "sg_postfx" || rp == "sg_history_write" || rp == "sg_present") {
			for (auto* s : ss) {
				s->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				s->depthStencilState.depthTestEnable = VK_FALSE;
				s->depthStencilState.depthWriteEnable = VK_FALSE;
				s->rasterizer.cullMode = VK_CULL_MODE_NONE;
				s->SetData(3);
			}
		}
	}
	
	void AllocatePostProcessingBuffers() {
		// Compute histogram
		totalLuminance.Allocate(r->renderingDevice, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		totalLuminance.MapMemory(r->renderingDevice);
		glm::vec4 luminance {0,0,0,1};
		totalLuminance.WriteToMappedData(&luminance);
	}
	void FreePostProcessingBuffers() {
		// Compute histogram
		totalLuminance.UnmapMemory(r->renderingDevice);
		totalLuminance.Free(r->renderingDevice);
	}
	
	void RecordPostProcessingCommands(VkCommandBuffer commandBuffer, int imageIndex) {
		// Gen Thumbnail
		thumbnailRenderPass.Begin(r->renderingDevice, commandBuffer, img_thumbnail, {{.0,.0,.0,.0}});
			for (auto* shader : shaderGroups["sg_thumbnail"]) {
				shader->Execute(r->renderingDevice, commandBuffer);
			}
		thumbnailRenderPass.End(r->renderingDevice, commandBuffer);
		
		// Post Processing
		postProcessingRenderPass.Begin(r->renderingDevice, commandBuffer, r->swapChain, {{.0,.0,.0,.0}, {.0,.0,.0,.0}, {.0,.0,.0,.0}}, imageIndex);
			for (auto* shader : shaderGroups["sg_postfx"]) {
				shader->Execute(r->renderingDevice, commandBuffer);
			}
			r->renderingDevice->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
			for (auto* shader : shaderGroups["sg_history_write"]) {
				shader->Execute(r->renderingDevice, commandBuffer);
			}
			r->renderingDevice->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
			for (auto* shader : shaderGroups["sg_present"]) {
				shader->Execute(r->renderingDevice, commandBuffer);
			}
		postProcessingRenderPass.End(r->renderingDevice, commandBuffer);
	}
	void RunDynamicPostProcessingCompute(VkCommandBuffer commandBuffer) {
		// Compute
		shader_histogram_compute.SetGroupCounts(1, 1, 1);
		shader_histogram_compute.Execute(r->renderingDevice, commandBuffer);
	}
	void PostProcessingUpdate2() {
		// Histogram
		glm::vec4 luminance;
		totalLuminance.ReadFromMappedData(&luminance);
		if (luminance.a > 0) {
			scene->camera.luminance = glm::vec4(glm::vec3(luminance) / luminance.a, exposureFactor);
		}
	}
	
#pragma endregion

#pragma region UI
	float img_overlayScale = 1.0;
	
	void CreateUiResources() {
		img_overlay.Create(r->renderingDevice, 
			(uint)((float)r->swapChain->extent.width * img_overlayScale), 
			(uint)((float)r->swapChain->extent.height * img_overlayScale)
		);
		auto commandBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
			r->TransitionImageLayout(commandBuffer, img_overlay, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
	}
	void DestroyUiResources() {
		img_overlay.Destroy(r->renderingDevice);
	}
	
	void CreateUiPipeline() {
		// Color Attachment (Fragment shader Standard Output)
		VkAttachmentDescription colorAttachment = {};
			colorAttachment.format = img_overlay.format;
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			// Color and depth data
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			// Stencil data
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		VkAttachmentReference colorAttachmentRef = {
			uiRenderPass.AddAttachment(colorAttachment),
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};
		// SubPass
		VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
		uiRenderPass.AddSubpass(subpass);
		
		// Create the render pass
		uiRenderPass.Create(r->renderingDevice);
		uiRenderPass.CreateFrameBuffers(r->renderingDevice, img_overlay);
		
		// Shader pipeline
		for (auto* shader : shaderGroups["sg_overlay"]) {
			shader->SetRenderPass(&img_overlay, uiRenderPass.handle, 0);
			shader->AddColorBlendAttachmentState();
			shader->CreatePipeline(r->renderingDevice);
		}
	}
	void DestroyUiPipeline() {
		for (auto* shader : shaderGroups["sg_overlay"]) {
			shader->DestroyPipeline(r->renderingDevice);
		}
		uiRenderPass.DestroyFrameBuffers(r->renderingDevice);
		uiRenderPass.Destroy(r->renderingDevice);
	}
	
	void ConfigureUiShaders() {
		//...
	}
	
	#ifdef _ENABLE_IMGUI
		void LoadImGui() {
			ImGui_ImplVulkan_InitInfo init_info {};
				init_info.Instance = r->GetHandle();
				init_info.PhysicalDevice = r->renderingDevice->GetPhysicalDevice()->GetHandle();
				init_info.Device = r->renderingDevice->GetHandle();
				init_info.QueueFamily = r->renderingDevice->GetQueue("graphics").familyIndex;
				init_info.Queue = r->renderingDevice->GetQueue("graphics").handle;
				init_info.DescriptorPool = r->descriptorPool;
				init_info.MinImageCount = r->swapChain->images.size();
				init_info.ImageCount = r->swapChain->images.size();
			ImGui_ImplVulkan_Init(&init_info, uiRenderPass.handle);
			// Font Upload
			auto cmdBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
				ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer);
			r->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), cmdBuffer);
			ImGui_ImplVulkan_DestroyFontUploadObjects();
		}
		void UnloadImGui() {
			ImGui_ImplVulkan_Shutdown();
		}
		void DrawImGui(VkCommandBuffer commandBuffer) {
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
		}
	#endif


#pragma endregion

#pragma region Frame Update

	void FrameUpdate(uint imageIndex) {
		
		// Reset camera information
		scene->camera.width = r->swapChain->extent.width;
		scene->camera.height = r->swapChain->extent.height;
		scene->camera.RefreshProjectionMatrix();
		scene->camera.time = float(v4d::Timer::GetCurrentTimestamp() - 1587838909.0);
		
		// Modules
		V4D_Game::ForEachSortedModule([](auto* mod){
			if (mod->RendererFrameUpdate) mod->RendererFrameUpdate();
		});
		
		// Ray Tracing
		size_t globalScratchBufferSize = 0;
		if (r->rayTracingFeatures.rayTracing) {
			topLevelAccelerationStructure.GetMemoryRequirementSizeForScratchBuffer(r->renderingDevice);
		}
		if (r->rayTracingFeatures.rayTracing) {
			ResetRayTracingBlasBuilds();
			inactiveBlas.clear();
		}
		
		scene->CollectGarbage();
		
		// Update object transforms and light sources (Use all lights for now)
		scene->Lock();
			nbActiveLights = 0;
			for (auto* obj : scene->objectInstances) {
				if (obj) {
					// obj->Lock();
					if (obj->IsActive()) {
						if (!obj->IsGenerated()) obj->GenerateGeometries();
						// Matrices
						obj->WriteMatrices(scene->camera.viewMatrix);
						// Light sources
						for (auto* lightSource : obj->GetLightSources()) {
							activeLights[nbActiveLights++] = lightSource->lightOffset;
						}
						// Geometries
						if (r->rayTracingFeatures.rayTracing) {
							for (auto& geom : obj->GetGeometries()) {
								if (geom.geometry->active) {
									if (!geom.geometry->blas) {
										MakeRayTracingBlas(&geom);
									}
									if (geom.geometry->blas && !geom.geometry->blas->built) {
										
										// Global Scratch Buffer
										if (AccelerationStructure::useGlobalScratchBuffer) {
											VkDeviceSize scratchSize = geom.geometry->blas->GetMemoryRequirementSizeForScratchBuffer(r->renderingDevice);
											if (!globalScratchDynamicSize && globalScratchBufferSize + scratchSize > globalScratchBuffer.size) {
												continue;
											}
											geom.geometry->blas->globalScratchBufferOffset = globalScratchBufferSize;
											geom.geometry->blas->SetGlobalScratchBuffer(r->renderingDevice, globalScratchBuffer.buffer);
											globalScratchBufferSize += scratchSize;
											if (globalScratchDynamicSize) blasBuildsForGlobalScratchBufferReallocation.push_back(geom.geometry->blas);
										}
										
										AddRayTracingBlasBuild(geom.geometry->blas);
									}
									if (geom.geometry->blas && geom.geometry->blas->built) {
										if (geom.rayTracingInstanceIndex == -1) {
											AddRayTracingInstance(&geom);
										}
										SetRayTracingInstanceTransform(&geom, scene->camera.viewMatrix * obj->GetWorldTransform());
									}
								} else if (geom.rayTracingInstanceIndex != -1) {
									RemoveRayTracingInstance(&geom);
								}
							}
						}
					} else if (r->rayTracingFeatures.rayTracing) {
						for (auto& geom : obj->GetGeometries()) if (geom.rayTracingInstanceIndex != -1) {
							RemoveRayTracingInstance(&geom);
						}
					}
					// obj->Unlock();
				}
			}
		scene->Unlock();
		
		// Global Scratch Buffer
		if (r->rayTracingFeatures.rayTracing && AccelerationStructure::useGlobalScratchBuffer && globalScratchDynamicSize) {
			// If current scratch buffer size is too small or more than 4x the necessary size, reallocate it
			if (globalScratchBuffer.size < globalScratchBufferSize || globalScratchBuffer.size > globalScratchBufferSize*4) {
				globalScratchBuffer.Free(r->renderingDevice);
				globalScratchBuffer.size = globalScratchBufferSize;
				globalScratchBuffer.Allocate(r->renderingDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);
				LOG("Reallocated global scratch buffer size " << globalScratchBufferSize)
				topLevelAccelerationStructure.SetGlobalScratchBuffer(r->renderingDevice, globalScratchBuffer.buffer);
				ResetRayTracingBlasBuilds();
				for (auto blas : blasBuildsForGlobalScratchBufferReallocation) {
					blas->SetGlobalScratchBuffer(r->renderingDevice, globalScratchBuffer.buffer);
					AddRayTracingBlasBuild(blas);
				}
			}
			blasBuildsForGlobalScratchBufferReallocation.clear();
		}
		
	}

	void FrameUpdate2() {
		PostProcessingUpdate2();
		
		// Modules
		V4D_Game::ForEachSortedModule([](auto* mod){
			if (mod->RendererFrameUpdate2) mod->RendererFrameUpdate2();
		});
	}

#pragma endregion

#pragma region Graphics commands

	void RunDynamicGraphics(VkCommandBuffer commandBuffer) {
		ClearLitImage(commandBuffer);
		
		// Transfer data to rendering device
		cameraUniformBuffer.Update(r->renderingDevice, commandBuffer);
		activeLightsUniformBuffer.Update(r->renderingDevice, commandBuffer, sizeof(uint32_t)/*lightCount*/ + sizeof(uint32_t)*nbActiveLights/*lightIndices*/);
		Geometry::globalBuffers.PushObjects(r->renderingDevice, commandBuffer);
		Geometry::globalBuffers.PushLights(r->renderingDevice, commandBuffer);
		if (r->rayTracingFeatures.rayTracing) {
			for (auto[i,geometry] : activeRayTracedGeometries) {
				if (geometry) geometry->AutoPush(r->renderingDevice, commandBuffer, true);
			}
		} else {
			// Geometry::globalBuffers.PushGeometriesInfo(r->renderingDevice, commandBuffer);
			scene->Lock();
				for (auto* obj : scene->objectInstances) if (obj && obj->IsActive()) {
					for (auto& geom : obj->GetGeometries()) if (geom.geometry->active) {
						if (geom.geometry) geom.geometry->AutoPush(r->renderingDevice, commandBuffer, true);
					}
				}
			scene->Unlock();
		}
		
		// Build Acceleration Structures
		if (r->rayTracingFeatures.rayTracing) {
			BuildBottomLevelRayTracingAccelerationStructures(r->renderingDevice, commandBuffer);
			BuildTopLevelRayTracingAccelerationStructure(r->renderingDevice, commandBuffer);
		}
		
		V4D_Renderer::ForEachSortedModule([&commandBuffer](auto* mod){
			if (mod->Render) mod->Render(commandBuffer);
		}, "render");
	}
	
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) {
		RecordPostProcessingCommands(commandBuffer, imageIndex);
	}

#pragma endregion

///////////////////////////////////////////////////////////

extern "C" {
	
	bool ModuleIsPrimary() {return true;}
	int OrderIndex() {return -1000;}
	
	#pragma region Containers Access
		
		Image* GetImage (const std::string& name) {
			if (images.find(name) == images.end()) {
				throw std::runtime_error(std::string("Image '") + name + "' does not exist");
				return nullptr;
			}
			return images[name];
		}
		
		PipelineLayout* GetPipelineLayout (const std::string& name) {
			if (pipelineLayouts.find(name) == pipelineLayouts.end()) {
				throw std::runtime_error(std::string("Pipeline layout '") + name + "' does not exist");
				return nullptr;
			}
			return pipelineLayouts[name];
		}
		
		void AddShader (const std::string& groupName, RasterShaderPipeline* shader) {
			if (shaderGroups.find(groupName) == shaderGroups.end()) {
				throw std::runtime_error(std::string("Shader group '") + groupName + "' does not exist");
				return;
			}
			shaderGroups[groupName].push_back(shader);
		}
		
		ShaderBindingTable* GetShaderBindingTable (const std::string& sbtName) {
			if (shaderBindingTables.find(sbtName) == shaderBindingTables.end()) {
				throw std::runtime_error(std::string("Shader binding table '") + sbtName + "' does not exist");
				return nullptr;
			}
			return shaderBindingTables[sbtName];
		}
		
	#pragma endregion
	
	#pragma region Graphics configuration
		
		void ScorePhysicalDeviceSelection(int& score, PhysicalDevice* physicalDevice) {
			// Higher score for Ray Tracing support
			if (physicalDevice->GetRayTracingFeatures().rayTracing) {
				score += 2;
			}
			if (physicalDevice->GetRayTracingFeatures().rayQuery) {
				score += 1;
			}
		}
		
		void Init(Renderer* _r, Scene* _s) {
			r = _r;
			scene = _s;
			
			V4D_Renderer::SortModules([](auto* a, auto* b){
				return (a->RenderOrderIndex? a->RenderOrderIndex():0) < (b->RenderOrderIndex? b->RenderOrderIndex():0);
			}, "render");
			
			scene->camera.renderMode = DEFAULT_VISIBILITY_RENDER_MODE;
			scene->camera.shadows = DEFAULT_SHADOW_TYPE;
			
			r->queuesInfo.emplace_back("secondary", VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
			
			// Device Extensions
			if (Loader::VULKAN_API_VERSION >= VK_API_VERSION_1_2) {
				r->OptionalDeviceExtension(VK_KHR_RAY_TRACING_EXTENSION_NAME); // RayTracing extension
				r->OptionalDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME); // Needed for RayTracing extension
				r->OptionalDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME); // Needed for RayTracing extension
				r->OptionalDeviceExtension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME); // Needed for RayTracing extension
			}
			
			// UBOs
			cameraUniformBuffer.AddSrcDataPtr(&scene->camera, sizeof(Camera));
			activeLightsUniformBuffer.AddSrcDataPtr(&nbActiveLights, sizeof(uint32_t));
			activeLightsUniformBuffer.AddSrcDataPtr(&activeLights, sizeof(uint32_t)*MAX_ACTIVE_LIGHTS);
			
			AccelerationStructure::useGlobalScratchBuffer = true;
		}
		
		void InitDeviceFeatures() {
			r->deviceFeatures.shaderFloat64 = VK_TRUE;
			r->deviceFeatures.depthClamp = VK_TRUE;
			r->deviceFeatures.fillModeNonSolid = VK_TRUE;
			
			// Vulkan 1.2
			if (Loader::VULKAN_API_VERSION >= VK_API_VERSION_1_2) {
				r->EnableVulkan12DeviceFeatures()->bufferDeviceAddress = VK_TRUE;
				r->EnableRayTracingFeatures()->rayTracing = VK_TRUE;
				r->EnableRayTracingFeatures()->rayQuery = VK_TRUE;
			}
		}
		
		void ConfigureRenderer() {
			if (r->rayTracingFeatures.rayTracing) {
				LOG_SUCCESS("Ray-Tracing Supported")
				// Query the ray tracing properties of the current implementation
				rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_KHR;
				VkPhysicalDeviceProperties2 deviceProps2 {};
					deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
					deviceProps2.pNext = &rayTracingProperties;
				r->GetPhysicalDeviceProperties2(r->renderingDevice->GetPhysicalDevice()->GetHandle(), &deviceProps2);
			} else {
				// No Ray-Tracing support
				LOG_WARN("Ray-Tracing unavailable, using rasterization")
				scene->camera.renderMode = rasterization;
				scene->camera.shadows = shadows_off;
			}
		}
	
	#pragma endregion
	
	void InitLayouts() {
		{r->descriptorSets["set0_base"] = &set0_base;
			set0_base.AddBinding_uniformBuffer(0, &cameraUniformBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR);
			set0_base.AddBinding_storageBuffer(1, &Geometry::globalBuffers.objectBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
			set0_base.AddBinding_storageBuffer(2, &Geometry::globalBuffers.lightBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
			set0_base.AddBinding_storageBuffer(3, &activeLightsUniformBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
			set0_base.AddBinding_storageBuffer(4, &Geometry::globalBuffers.geometryBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
			#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
				set0_base.AddBinding_storageBuffer(5, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
				set0_base.AddBinding_storageBuffer(6, &Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
			#else
				set0_base.AddBinding_storageBuffer(5, &Geometry::globalBuffers.indexBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
				set0_base.AddBinding_storageBuffer(6, &Geometry::globalBuffers.vertexBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
			#endif
		}
			
		{r->descriptorSets["set1_visibility_raster"] = &set1_visibility_raster;
			//
		}
		
		{r->descriptorSets["set1_visibility_rays"] = &set1_visibility_rays;
			int i = 0;
			set1_visibility_rays.AddBinding_accelerationStructure(i++, &topLevelAccelerationStructure.accelerationStructure, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			set1_visibility_rays.AddBinding_imageView(i++, &img_lit, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
			set1_visibility_rays.AddBinding_imageView(i++, &img_depth, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
			for (auto* img : gBuffers) set1_visibility_rays.AddBinding_imageView(i++, img, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		}
		
		{r->descriptorSets["set1_lighting_and_fog"] = &set1_lighting_and_fog;
			int i = 0;
			for (auto* img : gBuffers) set1_lighting_and_fog.AddBinding_inputAttachment(i++, img, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_lighting_and_fog.AddBinding_combinedImageSampler(i++, &img_depth, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_lighting_and_fog.AddBinding_combinedImageSampler(i++, &img_rasterDepth, VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_post"] = &set1_post;
			int i = 0;
			set1_post.AddBinding_combinedImageSampler(i++, &img_lit, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_overlay, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_inputAttachment(i++, &img_pp, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_history, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_depth, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_rasterDepth, VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_overlay"] = &set1_overlay;
			//...
		}
		
		{r->descriptorSets["set1_thumbnail"] = &set1_thumbnail;
			set1_thumbnail.AddBinding_combinedImageSampler(0, &img_lit, VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_histogram"] = &set1_histogram;
			set1_histogram.AddBinding_imageView(0, &img_thumbnail, VK_SHADER_STAGE_COMPUTE_BIT);
			set1_histogram.AddBinding_storageBuffer(1, &totalLuminance, VK_SHADER_STAGE_COMPUTE_BIT);
		}
		
		{ // Assign descriptor sets to layouts
			// Add the same set 0 to all pipeline layouts
			for (auto[name, layout] : pipelineLayouts) {
				layout->AddDescriptorSet(&set0_base);
			}
			// Add specific set 1 to specific layout lists
			pipelineLayouts["pl_visibility_raster"]->AddDescriptorSet(&set1_visibility_raster);
			pipelineLayouts["pl_visibility_rays"]->AddDescriptorSet(&set1_visibility_rays);
			pipelineLayouts["pl_lighting_raster"]->AddDescriptorSet(&set1_lighting_and_fog);
			pipelineLayouts["pl_lighting_rays"]->AddDescriptorSet(&set1_lighting_and_fog);
			pipelineLayouts["pl_fog_raster"]->AddDescriptorSet(&set1_lighting_and_fog);
			pipelineLayouts["pl_thumbnail"]->AddDescriptorSet(&set1_thumbnail);
			pipelineLayouts["pl_overlay"]->AddDescriptorSet(&set1_overlay);
			pipelineLayouts["pl_post"]->AddDescriptorSet(&set1_post);
			pipelineLayouts["pl_histogram"]->AddDescriptorSet(&set1_histogram);
		}
	}
	
	#pragma region Load/Upload Renderer
		
		void ConfigureShaders() {
			ConfigureRasterVisibilityShaders();
			ConfigureRayTracingShaders();
			ConfigureLightingShaders();
			ConfigurePostProcessingShaders();
			ConfigureUiShaders();
		}

		void ReadShaders() {
			for (auto&[grpName, shaderList] : shaderGroups) {
				for (auto* shader : shaderList) {
					shader->ReadShaders();
				}
			}
			
			for (auto&[name, sbt] : shaderBindingTables) {
				sbt->ReadShaders();
			}
			
			shader_histogram_compute.ReadShaders();
		}
		
		void CreateSyncObjects() {
			semaphores["imageAvailable"].resize(r->NB_FRAMES_IN_FLIGHT);
			semaphores["staticRenderFinished"].resize(r->NB_FRAMES_IN_FLIGHT);
			semaphores["dynamicRenderFinished"].resize(r->NB_FRAMES_IN_FLIGHT);
			fences["graphics"].resize(r->NB_FRAMES_IN_FLIGHT);
			fences["compute"].resize(r->NB_FRAMES_IN_FLIGHT);

			VkSemaphoreCreateInfo semaphoreInfo = {};
				semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkFenceCreateInfo fenceInfo = {};
				fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Initialize in the signaled state so that we dont get stuck on the first frame

			for (int i = 0; i < r->NB_FRAMES_IN_FLIGHT; i++) {
				for (auto&[name, s] : semaphores) {
					if (r->renderingDevice->CreateSemaphore(&semaphoreInfo, nullptr, &s[i]) != VK_SUCCESS) {
						throw std::runtime_error("Failed to create semaphores");
					}
				}
				for (auto&[name, f] : fences) {
					if (r->renderingDevice->CreateFence(&fenceInfo, nullptr, &f[i]) != VK_SUCCESS) {
						throw std::runtime_error("Failed to create fences");
					}
				}
			}
		}

		void DestroySyncObjects() {
			for (int i = 0; i < r->NB_FRAMES_IN_FLIGHT; i++) {
				for (auto&[name, s] : semaphores) r->renderingDevice->DestroySemaphore(s[i], nullptr);
				for (auto&[name, f] : fences) r->renderingDevice->DestroyFence(f[i], nullptr);
			}
		}

		void CreateResources() {
			CreateUiResources();
			CreateRenderingResources();
			CreatePostProcessingResources();
			if (r->rayTracingFeatures.rayTracing) CreateRayTracingResources();
			CreateRasterVisibilityResources();
			
			// Modules
			V4D_Game::ForEachSortedModule([](auto* mod){
				if (mod->RendererCreateResources) mod->RendererCreateResources(r->renderingDevice);
			});
		}
		
		void DestroyResources() {
			DestroyUiResources();
			DestroyRenderingResources();
			DestroyPostProcessingResources();
			if (r->rayTracingFeatures.rayTracing) DestroyRayTracingResources();
			DestroyRasterVisibilityResources();
			
			// Modules
			V4D_Game::ForEachSortedModule([](auto* mod){
				if (mod->RendererDestroyResources) mod->RendererDestroyResources(r->renderingDevice);
			});
		}
		
		void AllocateBuffers() {
			// Uniform Buffers
			cameraUniformBuffer.Allocate(r->renderingDevice);
			activeLightsUniformBuffer.Allocate(r->renderingDevice);

			AllocatePostProcessingBuffers();
			if (r->rayTracingFeatures.rayTracing) AllocateRayTracingBuffers();
			
			Geometry::globalBuffers.DefragmentMemory();
			Geometry::globalBuffers.Allocate(r->renderingDevice, {r->renderingDevice->GetQueue("compute").familyIndex, r->renderingDevice->GetQueue("graphics").familyIndex});
		}
		
		void FreeBuffers() {
			scene->ClenupObjectInstancesGeometries();
			activeRayTracedGeometries.clear();
			scene->CollectGarbage();
			
			// Uniform Buffers
			cameraUniformBuffer.Free(r->renderingDevice);
			activeLightsUniformBuffer.Free(r->renderingDevice);
			
			FreePostProcessingBuffers();
			if (r->rayTracingFeatures.rayTracing) FreeRayTracingBuffers();
			
			Geometry::globalBuffers.Free(r->renderingDevice);
		}

		void CreatePipelines() {
			// Sort shaders
			for (auto&[rs, ss] : shaderGroups) {
				std::sort(ss.begin(), ss.end(), [](auto* a, auto* b){
					return a->sortIndex < b->sortIndex;
				});
			}
			
			// Pipeline layouts
			for (auto[name, layout] : pipelineLayouts) {
				layout->Create(r->renderingDevice);
			}
			
			// UI
			CreateUiPipeline();
			#ifdef _ENABLE_IMGUI
				LoadImGui();
			#endif
			
			// Visibility
			CreateRasterVisibilityPipeline();
			
			// Ray Tracing
			if (r->rayTracingFeatures.rayTracing) CreateRayTracingPipeline();
			
			// Lighting
			CreateLightingPipeline();
			
			// Post Processing
			CreatePostProcessingPipeline();
		}
		
		void DestroyPipelines() {
			// UI
			#ifdef _ENABLE_IMGUI
				UnloadImGui();
			#endif
			DestroyUiPipeline();
			
			// Visibility
			DestroyRasterVisibilityPipeline();
			
			// Ray Tracing
			if (r->rayTracingFeatures.rayTracing) DestroyRayTracingPipeline();
			
			// Lighting
			DestroyLightingPipeline();
			
			// Post Processing
			DestroyPostProcessingPipeline();
			
			// Pipeline layouts
			for (auto[name,layout] : pipelineLayouts) {
				layout->Destroy(r->renderingDevice);
			}
		}
		
		void CreateCommandBuffers() {
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
							/*	VK_COMMAND_BUFFER_LEVEL_PRIMARY = Can be submitted to a queue for execution, but cannot be called from other command buffers
								VK_COMMAND_BUFFER_LEVEL_SECONDARY = Cannot be submitted directly, but can be called from primary command buffers
							*/
			
			{// Graphics Commands Recorder
				// Because one of the drawing commands involves binding the right VkFramebuffer, we'll actually have to record a command buffer for every image in the swap chain once again.
				// Command buffers will be automatically freed when their command pool is destroyed, so we don't need an explicit cleanup
				commandBuffers["graphics"].resize(r->swapChain->imageViews.size());
				
				allocInfo.commandPool = r->renderingDevice->GetQueue("graphics").commandPool;
				allocInfo.commandBufferCount = (uint) commandBuffers["graphics"].size();
				if (r->renderingDevice->AllocateCommandBuffers(&allocInfo, commandBuffers["graphics"].data()) != VK_SUCCESS) {
					throw std::runtime_error("Failed to allocate command buffers");
				}

				// Starting command buffer recording...
				for (size_t i = 0; i < commandBuffers["graphics"].size(); i++) {
					VkCommandBufferBeginInfo beginInfo = {};
					beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
					beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // We have used this flag because we may already be scheduling the drawing commands for the next frame while the last frame is not finished yet.
									/*	VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = The command buffer will be rerecorded right after executing it once
										VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT = This is a secondary command buffer that will be entirely within a single render pass.
										VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT = The command buffer can be resubmited while it is also already pending execution
									*/
					beginInfo.pInheritanceInfo = nullptr; // only relevant for secondary command buffers. It specifies which state to inherit from the calling primary command buffers.
					// If a command buffer was already recorded once, then a call to vkBeginCommandBuffer will implicitly reset it.
					// It's not possible to append commands to a buffer at a later time.
					if (r->renderingDevice->BeginCommandBuffer(commandBuffers["graphics"][i], &beginInfo) != VK_SUCCESS) {
						throw std::runtime_error("Faild to begin recording command buffer");
					}
					
					// Record commands
					RecordGraphicsCommandBuffer(commandBuffers["graphics"][i], i);
					
					if (r->renderingDevice->EndCommandBuffer(commandBuffers["graphics"][i]) != VK_SUCCESS) {
						throw std::runtime_error("Failed to record command buffer");
					}
				}
			}
			commandBuffers["graphicsDynamic"].resize(commandBuffers["graphics"].size());
			if (r->renderingDevice->AllocateCommandBuffers(&allocInfo, commandBuffers["graphicsDynamic"].data()) != VK_SUCCESS) {
				throw std::runtime_error("Failed to allocate command buffers");
			}
		}

		void DestroyCommandBuffers() {
			r->renderingDevice->FreeCommandBuffers(r->renderingDevice->GetQueue("graphics").commandPool, static_cast<uint32_t>(commandBuffers["graphics"].size()), commandBuffers["graphics"].data());
			r->renderingDevice->FreeCommandBuffers(r->renderingDevice->GetQueue("graphics").commandPool, static_cast<uint32_t>(commandBuffers["graphicsDynamic"].size()), commandBuffers["graphicsDynamic"].data());
		}
	
	#pragma endregion

	void Update() {
		
		uint64_t timeout = 1000UL * 1000 * 1000 * 10; // 10 seconds

		// Get an image from the swapchain
		uint imageIndex;
		{
			VkResult result = r->renderingDevice->AcquireNextImageKHR(
				r->swapChain->GetHandle(), // swapChain
				timeout, // timeout in nanoseconds (using max disables the timeout)
				semaphores["imageAvailable"][r->currentFrameInFlight], // semaphore
				VK_NULL_HANDLE, // fence
				&imageIndex // output the index of the swapchain image in there
			);

			// Check for errors
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				// SwapChain is out of date, for instance if the window was resized, stop here and ReCreate the swapchain.
				r->RecreateSwapChains();
				return;
			} else if (result == VK_SUBOPTIMAL_KHR) {
				LOG_VERBOSE("Swapchain is suboptimal...")
			} else if (result != VK_SUCCESS) {
				throw std::runtime_error("Failed to acquire swap chain images");
			}
		}

		// Update data every frame
		FrameUpdate(imageIndex);

		std::array<VkSubmitInfo, 2> computeSubmitInfo {};
			computeSubmitInfo[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			computeSubmitInfo[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		std::array<VkSubmitInfo, 2> graphicsSubmitInfo {};
			graphicsSubmitInfo[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			graphicsSubmitInfo[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		
		std::array<VkSemaphore, 2> staticGraphicsWaitSemaphores {
			semaphores["imageAvailable"][r->currentFrameInFlight],
			semaphores["dynamicRenderFinished"][r->currentFrameInFlight],
		};
		VkPipelineStageFlags staticGraphicsWaitStages[] {
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		};
		
		{// Configure Graphics
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			
			r->renderingDevice->WaitForFences(1, &fences["graphics"][r->currentFrameInFlight], VK_TRUE, timeout);
			
			//TODO find a better fix...
			r->renderingDevice->QueueWaitIdle(r->renderingDevice->GetQueue("graphics").handle); // Temporary fix for occasional crash with acceleration structures
		
			r->renderingDevice->ResetFences(1, &fences["graphics"][r->currentFrameInFlight]);
			
			r->renderingDevice->ResetCommandBuffer(commandBuffers["graphicsDynamic"][imageIndex], 0);
			if (r->renderingDevice->BeginCommandBuffer(commandBuffers["graphicsDynamic"][imageIndex], &beginInfo) != VK_SUCCESS) {
				throw std::runtime_error("Faild to begin recording command buffer");
			}
			RunDynamicGraphics(commandBuffers["graphicsDynamic"][imageIndex]);
			if (r->renderingDevice->EndCommandBuffer(commandBuffers["graphicsDynamic"][imageIndex]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to record command buffer");
			}
			// dynamic commands
			graphicsSubmitInfo[0].waitSemaphoreCount = 0;
			graphicsSubmitInfo[0].pWaitSemaphores = nullptr;
			graphicsSubmitInfo[0].pWaitDstStageMask = nullptr;
			graphicsSubmitInfo[0].commandBufferCount = 1;
			graphicsSubmitInfo[0].pCommandBuffers = &commandBuffers["graphicsDynamic"][imageIndex];
			graphicsSubmitInfo[0].signalSemaphoreCount = 1;
			graphicsSubmitInfo[0].pSignalSemaphores = &semaphores["dynamicRenderFinished"][r->currentFrameInFlight];
			// static commands
			graphicsSubmitInfo[1].waitSemaphoreCount = staticGraphicsWaitSemaphores.size();
			graphicsSubmitInfo[1].pWaitSemaphores = staticGraphicsWaitSemaphores.data();
			graphicsSubmitInfo[1].pWaitDstStageMask = staticGraphicsWaitStages;
			graphicsSubmitInfo[1].commandBufferCount = 1;
			graphicsSubmitInfo[1].pCommandBuffers = &commandBuffers["graphics"][imageIndex];
			graphicsSubmitInfo[1].signalSemaphoreCount = 1;
			graphicsSubmitInfo[1].pSignalSemaphores = &semaphores["staticRenderFinished"][r->currentFrameInFlight];
		}
		// Submit Graphics
		VkResult result = r->renderingDevice->QueueSubmit(r->renderingDevice->GetQueue("graphics").handle, graphicsSubmitInfo.size(), graphicsSubmitInfo.data(), fences["graphics"][r->currentFrameInFlight]);
		if (result != VK_SUCCESS) {
			if (result == VK_ERROR_DEVICE_LOST) {
				LOG_ERROR("Render() Failed to submit graphics command buffer : VK_ERROR_DEVICE_LOST")
				// SLEEP(500ms)
				// ReloadRenderer();
				return;
			}
			LOG_ERROR((int)result)
			throw std::runtime_error("Render() Failed to submit graphics command buffer");
		}

		// Present
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		// Specify which semaphore to wait on before presentation can happen
		presentInfo.waitSemaphoreCount = 1;
		VkSemaphore presentWaitSemaphores[] = {
			semaphores["staticRenderFinished"][r->currentFrameInFlight],
		};
		presentInfo.pWaitSemaphores = presentWaitSemaphores;
		// Specify the swap chains to present images to and the index for each swap chain. (almost always a single one)
		VkSwapchainKHR swapChains[] = {r->swapChain->GetHandle()};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		// The next param allows to specify an array of VkResult values to check for every individual swap chain if presentation was successful.
		// its not necessary if only using a single swap chain, because we can simply use the return value of the present function.
		presentInfo.pResults = nullptr;
		// Send the present info to the presentation queue !
		// This submits the request to present an image to the swap chain.
		result = r->renderingDevice->QueuePresentKHR(r->renderingDevice->GetQueue("present").handle, &presentInfo);

		// Check for errors
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			// SwapChain is out of date, for instance if the window was resized, stop here and ReCreate the swapchain.
			r->RecreateSwapChains();
			return;
		} else if (result == VK_SUBOPTIMAL_KHR) {
			LOG_VERBOSE("Swapchain is suboptimal...")
		} else if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to present swap chain images");
		}

		// Increment r->currentFrameInFlight
		r->currentFrameInFlight = (r->currentFrameInFlight + 1) % r->NB_FRAMES_IN_FLIGHT;
	}
	
	void Update2() {
		FrameUpdate2();
	
		// Dynamic compute
		auto cmdBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("secondary"));
		
			RunDynamicPostProcessingCompute(cmdBuffer);
			
			// Modules
			V4D_Game::ForEachSortedModule([cmdBuffer](auto* mod){
				if (mod->RendererFrameCompute) mod->RendererFrameCompute(cmdBuffer);
			});
			V4D_Renderer::ForEachSortedModule([cmdBuffer](auto* mod){
				if (mod->Render2) mod->Render2(cmdBuffer);
			}, "render");
			
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("secondary"), cmdBuffer);
	}
	
	void RunUi() {
		#ifdef _ENABLE_IMGUI
			ImGui::SetNextWindowSize({340, 160});
			ImGui::Begin("Settings and Modules");
			// #ifdef _DEBUG
				ImGui::Checkbox("Debug Geometry", &scene->camera.debug);
			// #endif
			if (scene->camera.debug) {
				// ...
			} else {
				if (r->rayTracingFeatures.rayTracing) {
					ImGui::Combo("Rendering Mode", &scene->camera.renderMode, VISIBILITY_RENDER_MODES_STR);
				} else {
					ImGui::Text("Ray-Tracing unavailable, using rasterization");
				}
				if (scene->camera.renderMode > 0) {
					ImGui::Combo("Shadows", &scene->camera.shadows, SHADOW_TYPES_STR);
				} else {
					ImGui::Text("Shadows disabled");
				}
				ImGui::Checkbox("TXAA", &scene->camera.txaa);
				ImGui::Checkbox("Gamma correction", &scene->camera.gammaCorrection);
				ImGui::Checkbox("HDR Tone Mapping", &scene->camera.hdr);
				ImGui::SliderFloat("HDR Exposure", &exposureFactor, 0, 10);
				ImGui::SliderFloat("brightness", &scene->camera.brightness, 0, 2);
				ImGui::SliderFloat("contrast", &scene->camera.contrast, 0, 2);
				ImGui::SliderFloat("gamma", &scene->camera.gamma, 0, 5);
			}
		#endif
			// Modules
			V4D_Game::ForEachSortedModule([](auto* mod){
				if (mod->RendererRunUi) {
					#ifdef _ENABLE_IMGUI
						ImGui::Separator();
					#endif
					mod->RendererRunUi();
				}
			});
		#ifdef _ENABLE_IMGUI
			ImGui::End();
		#endif
		// #ifdef _DEBUG
			#ifdef _ENABLE_IMGUI
				ImGui::SetNextWindowPos({425+345,0});
				ImGui::SetNextWindowSize({250, 100});
				ImGui::Begin("Debug");
			#endif
				// Modules
				V4D_Game::ForEachSortedModule([](auto* mod){
					#ifdef _ENABLE_IMGUI
						ImGui::Separator();
					#endif
					if (mod->RendererRunUiDebug) mod->RendererRunUiDebug();
				});
			#ifdef _ENABLE_IMGUI
				ImGui::End();
			#endif
		// #endif
	}
	
	// Render pipelines
	
	void Render2(VkCommandBuffer commandBuffer) {
		// UI
		uiRenderPass.Begin(r->renderingDevice, commandBuffer, img_overlay, {{.0,.0,.0,.0}});
			for (auto* shader : shaderGroups["sg_overlay"]) {
				shader->Execute(r->renderingDevice, commandBuffer);
			}
			#ifdef _ENABLE_IMGUI
				DrawImGui(commandBuffer);
			#endif
		uiRenderPass.End(r->renderingDevice, commandBuffer);
	}
	
	void Render(VkCommandBuffer commandBuffer) {
		if (scene->camera.renderMode == rasterization || scene->camera.debug) {
			// Raster Visibility and lighting
			RunRasterVisibility(r->renderingDevice, commandBuffer);
			RunLighting(r->renderingDevice, commandBuffer, true, !scene->camera.debug);
		} else if (r->rayTracingFeatures.rayTracing) {
			// Ray Tracing lighting
			RunRayTracingVisibilityCommands(commandBuffer);
			RunLighting(r->renderingDevice, commandBuffer, false, true);
		}
	}

}
