#define _V4D_MODULE
#define V4D_HYBRID_RENDERER_MODULE

#include <v4d.h>
#include "Texture2D.hpp"
#include "RayCast.hh"
#include "camera_options.hh"

using namespace v4d::scene;
using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::vulkan::rtx;

#pragma region Limits
	const uint32_t MAX_ACTIVE_LIGHTS = 256;
#pragma endregion

// Application
Renderer* r = nullptr;
Scene* scene = nullptr;

RayCast currentRayCast {};

// Textures
Texture2D tex_img_font_atlas { V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/monospace_font_atlas.png"), STBI_grey_alpha};

#pragma region Descriptor Sets
	DescriptorSet set0_base;
	DescriptorSet set1_visibility_raster;
	DescriptorSet set1_visibility_rays;
	DescriptorSet set1_lighting_raster;
	DescriptorSet set1_lighting_rays;
	DescriptorSet set1_transparent;
	DescriptorSet set1_fog_raster;
	DescriptorSet set1_post;
	DescriptorSet set1_thumbnail;
	DescriptorSet set1_histogram;
	DescriptorSet set1_raycast;
	DescriptorSet set1_overlay;
#pragma endregion

#pragma region Images
	DepthImage img_rasterDepth { VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
	Image img_depth { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32_SFLOAT } };
	Image img_gBuffer_0 { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R8G8_SNORM }};
	Image img_gBuffer_1 { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT }};
	Image img_gBuffer_2 { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT }};
	Image img_gBuffer_3 { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_gBuffer_4 { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32G32B32A32_UINT }};
	Image img_lit { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_pp { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_history { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_thumbnail { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_overlay { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8G8B8A8_UNORM }};
#pragma endregion

#pragma region Pipeline Layouts
	PipelineLayout pl_visibility_raster;
	PipelineLayout pl_visibility_rays;
	PipelineLayout pl_lighting_raster;
	PipelineLayout pl_lighting_rays;
	PipelineLayout pl_transparent;
	PipelineLayout pl_fog_raster;
	PipelineLayout pl_overlay;
	PipelineLayout pl_post;
	PipelineLayout pl_thumbnail;
	PipelineLayout pl_histogram;
	PipelineLayout pl_raycast;
#pragma endregion

#pragma region Shaders

	const std::string rasterTrianglesDefaultVertexShader = V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_visibility.vert");
	const std::string rasterAabbDefaultVertexShader = V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_visibility.aabb.vert");
	const std::string rasterAabbDefaultGeometryShader = V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_visibility.aabb.geom");
	const std::string rasterAabbSphereDefaultGeometryShader = V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_visibility.sphere.geom");

	// Visibility
	RasterShaderPipeline shader_visibility_basic {pl_visibility_raster, {
		rasterTrianglesDefaultVertexShader,
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_visibility.basic.frag"),
	}};
	RasterShaderPipeline shader_visibility_standard {pl_visibility_raster, {
		rasterTrianglesDefaultVertexShader,
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_visibility.standard.frag"),
	}};
	RasterShaderPipeline shader_visibility_terrain {pl_visibility_raster, {
		rasterTrianglesDefaultVertexShader,
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_visibility.terrain.frag"),
	}};
	RasterShaderPipeline shader_visibility_aabb {pl_visibility_raster, {
		rasterAabbDefaultVertexShader,
		rasterAabbDefaultGeometryShader,
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_visibility.aabb.frag"),
	}};
	RasterShaderPipeline shader_visibility_sphere {pl_visibility_raster, {
		rasterAabbDefaultVertexShader,
		rasterAabbSphereDefaultGeometryShader,
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_visibility.sphere.frag"),
	}};
	RasterShaderPipeline shader_visibility_light {pl_visibility_raster, {
		rasterAabbDefaultVertexShader,
		rasterAabbSphereDefaultGeometryShader,
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_visibility.light.frag"),
	}};
	RasterShaderPipeline shader_visibility_sun {pl_visibility_raster, {
		rasterAabbDefaultVertexShader,
		rasterAabbSphereDefaultGeometryShader,
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_visibility.sun.frag"),
	}};
	// #ifdef _DEBUG
		RasterShaderPipeline shader_debug_wireframe {pl_visibility_raster, {
			rasterTrianglesDefaultVertexShader,
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_visibility.basic.frag"),
		}};
	// #endif
	
	// Transparent/Fog
	RasterShaderPipeline shader_transparent {pl_transparent, {
		rasterTrianglesDefaultVertexShader,
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/transparent.frag"),
	}};
	RasterShaderPipeline shader_transparent_wireframe {pl_transparent, {
		rasterTrianglesDefaultVertexShader,
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/transparent.wireframe.frag"),
	}};
	
	// Overlay
	RasterShaderPipeline shader_overlay_circles {pl_overlay, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_shapes.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_shapes.circle.frag"),
	}, -4};
	RasterShaderPipeline shader_overlay_squares {pl_overlay, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_shapes.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_shapes.square.frag"),
	}, -3};
	RasterShaderPipeline shader_overlay_lines {pl_overlay, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_lines.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_lines.frag"),
	}, -2};
	RasterShaderPipeline shader_overlay_text {pl_overlay, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_text.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_text.frag"),
	}, -1};
	
	// Main Rendering
	RasterShaderPipeline shader_lighting {pl_lighting_raster, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_lighting.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_lighting.frag"),
	}};
	RasterShaderPipeline shader_lighting_rtx {pl_lighting_raster, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_lighting.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster_lighting.rtx.frag"),
	}};
	
	// Ray Tracing
	ShaderBindingTable sbt_visibility {pl_visibility_rays, V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.rgen")};
	ShaderBindingTable sbt_lighting {pl_lighting_rays, V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_lighting.rgen")};
	
	// Post Processing
	RasterShaderPipeline shader_thumbnail {pl_thumbnail, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_thumbnail.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_thumbnail.frag"),
	}};
	RasterShaderPipeline shader_fx_txaa {pl_post, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.txaa.frag"),
	}, 100};
	RasterShaderPipeline shader_history_write {pl_post, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.history_write.frag"),
	}};
	RasterShaderPipeline shader_present_hdr {pl_post, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.hdr.frag"),
	}, -100};
	RasterShaderPipeline shader_present_overlay_apply {pl_post, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.overlay_apply.frag"),
	}, 100};
	ComputeShaderPipeline shader_histogram_compute {pl_histogram, 
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_histogram.comp")
	};
	ComputeShaderPipeline shader_raycast_compute {pl_raycast, 
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_raycast.comp")
	};
	
#pragma endregion

#pragma region Containers
	// Pipelines
	std::unordered_map<std::string, Image*> images {
		{"img_rasterDepth", &img_rasterDepth},
		{"img_depth", &img_depth},
		{"img_gBuffer_0", &img_gBuffer_0},
		{"img_gBuffer_1", &img_gBuffer_1},
		{"img_gBuffer_2", &img_gBuffer_2},
		{"img_gBuffer_3", &img_gBuffer_3},
		{"img_gBuffer_4", &img_gBuffer_4},
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
		{"pl_transparent", &pl_transparent},
		{"pl_fog_raster", &pl_fog_raster},
		{"pl_overlay", &pl_overlay},
		{"pl_post", &pl_post},
		{"pl_thumbnail", &pl_thumbnail},
		{"pl_histogram", &pl_histogram},
		{"pl_raycast", &pl_raycast},
	};
	std::unordered_map<std::string, std::vector<RasterShaderPipeline*>> shaderGroups {
		{"sg_visibility", {
			&shader_visibility_basic,
			&shader_visibility_standard,
			&shader_visibility_terrain,
			&shader_visibility_aabb,
			&shader_visibility_sphere,
			&shader_visibility_light,
			&shader_visibility_sun,
			// #ifdef _DEBUG
				&shader_debug_wireframe,
			// #endif
		}},
		{"sg_lighting", {&shader_lighting}},
		{"sg_lighting_rtx", {&shader_lighting_rtx}},
		{"sg_transparent", {&shader_transparent}},
		{"sg_wireframe", {&shader_transparent_wireframe}},
		{"sg_fog", {}},
		{"sg_thumbnail", {&shader_thumbnail}},
		{"sg_postfx", {&shader_fx_txaa}},
		{"sg_history_write", {&shader_history_write}},
		{"sg_present", {&shader_present_hdr, &shader_present_overlay_apply}},
		{"sg_overlay", {&shader_overlay_lines, &shader_overlay_text, &shader_overlay_squares, &shader_overlay_circles}},
	};
	std::unordered_map<std::string, ShaderBindingTable*> shaderBindingTables {
		{"sbt_visibility", &sbt_visibility},
		{"sbt_lighting", &sbt_lighting},
	};
	// G-Buffers
	static const int NB_G_BUFFERS = 5;
	std::array<Image*, NB_G_BUFFERS> gBuffers {
		&img_gBuffer_0, // R=snorm8(metallic), G=snorm8(roughness)
		&img_gBuffer_1, // RGB=sfloat32(normal.xyz), A=sfloat32(packed(uv))
		&img_gBuffer_2, // RGB=sfloat32(viewPosition.xyz), A=sfloat32(distance)
		&img_gBuffer_3, // RGB=snorm8(albedo.rgb), A=snorm8(emit?)
		&img_gBuffer_4, // R=objectIndex24+customType8, G=flags32, B=customData32, A=customData32
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
	Buffer raycastBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(RayCast)};
#pragma endregion

#pragma region Rasterization Pipelines & Resources

	struct GeometryPushConstant {
		glm::vec4 wireframeColor = {1,1,1,1};
		uint objectIndex; // limited to 23 bits (8 million objects)
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
			
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
	}
	void DestroyRasterVisibilityResources() {
		for (auto* img : gBuffers)
			img->Destroy(r->renderingDevice);
		img_rasterDepth.Destroy(r->renderingDevice);
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
		if (r->rayTracingFeatures.rayQuery) {
			for (auto* s : shaderGroups["sg_lighting_rtx"]) {
				s->SetRenderPass(&img_lit, lightingRenderPass.handle, 0);
				s->AddColorBlendAttachmentState();
				s->CreatePipeline(r->renderingDevice);
			}
		} else {
			for (auto* s : shaderGroups["sg_lighting"]) {
				s->SetRenderPass(&img_lit, lightingRenderPass.handle, 0);
				s->AddColorBlendAttachmentState();
				s->CreatePipeline(r->renderingDevice);
			}
		}
		for (auto& sg : {shaderGroups["sg_fog"], shaderGroups["sg_transparent"], shaderGroups["sg_wireframe"]}) {
			for (auto* s : sg) {
				s->SetRenderPass(&img_lit, lightingRenderPass.handle, 1);
				s->AddColorBlendAttachmentState(
					VK_TRUE,
					VK_BLEND_FACTOR_SRC_ALPHA,
					VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
					VK_BLEND_OP_ADD,
					VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
					VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
					VK_BLEND_OP_ADD
				);
				s->CreatePipeline(r->renderingDevice);
			}
		}
	}
	void DestroyLightingPipeline() {
		if (r->rayTracingFeatures.rayQuery) {
			for (auto* s : shaderGroups["sg_lighting_rtx"]) {
				s->DestroyPipeline(r->renderingDevice);
			}
		} else {
			for (auto* s : shaderGroups["sg_lighting"]) {
				s->DestroyPipeline(r->renderingDevice);
			}
		}
		for (auto& sg : {shaderGroups["sg_fog"], shaderGroups["sg_transparent"], shaderGroups["sg_wireframe"]}) {
			for (auto* s : sg) {
				s->DestroyPipeline(r->renderingDevice);
			}
		}
		lightingRenderPass.DestroyFrameBuffers(r->renderingDevice);
		lightingRenderPass.Destroy(r->renderingDevice);
	}
	
	void ConfigureRasterVisibilityShaders() {
		pl_visibility_raster.AddPushConstant<GeometryPushConstant>(VK_SHADER_STAGE_ALL_GRAPHICS);
	
		for (auto* s : shaderGroups["sg_visibility"]) {
			// #ifdef _DEBUG
				if (s == &shader_debug_wireframe) {
					s->AddVertexInputBinding(sizeof(Geometry::VertexBuffer_T), VK_VERTEX_INPUT_RATE_VERTEX, Geometry::VertexBuffer_T::GetInputAttributes());
					s->rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
					s->rasterizer.lineWidth = 1;
					s->rasterizer.cullMode = VK_CULL_MODE_NONE;
					s->dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
					#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
						s->SetData(&Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer, 0);
					#else
						s->SetData(&Geometry::globalBuffers.vertexBuffer, &Geometry::globalBuffers.indexBuffer, 0);
					#endif
				} else 
			// #endif
			if (s->GetShaderPath("vert") == rasterTrianglesDefaultVertexShader) {
				s->AddVertexInputBinding(sizeof(Geometry::VertexBuffer_T), VK_VERTEX_INPUT_RATE_VERTEX, Geometry::VertexBuffer_T::GetInputAttributes());
				s->rasterizer.cullMode = VK_CULL_MODE_NONE;
				#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
					s->SetData(&Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer, 0);
				#else
					s->SetData(&Geometry::globalBuffers.vertexBuffer, &Geometry::globalBuffers.indexBuffer, 0);
				#endif
			} else if (s->GetShaderPath("vert") == rasterAabbDefaultVertexShader) {
				s->AddVertexInputBinding(sizeof(Geometry::ProceduralVertexBuffer_T), VK_VERTEX_INPUT_RATE_VERTEX, Geometry::ProceduralVertexBuffer_T::GetInputAttributes());
				s->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
				s->rasterizer.cullMode = VK_CULL_MODE_NONE;
				s->depthStencilState.depthTestEnable = true;
				s->depthStencilState.depthWriteEnable = true;
				#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
					s->SetData(&Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, 1);
				#else
					s->SetData(&Geometry::globalBuffers.vertexBuffer, 1);
				#endif
			}
		}
	}
	
	void ConfigureTransparentShaders() {
		pl_transparent.AddPushConstant<GeometryPushConstant>(VK_SHADER_STAGE_ALL_GRAPHICS);
		
		for (auto* s : shaderGroups["sg_transparent"]) {
			s->AddVertexInputBinding(sizeof(Geometry::VertexBuffer_T), VK_VERTEX_INPUT_RATE_VERTEX, Geometry::VertexBuffer_T::GetInputAttributes());
			s->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			s->rasterizer.cullMode = VK_CULL_MODE_NONE;
			s->depthStencilState.depthTestEnable = VK_FALSE;
			s->depthStencilState.depthWriteEnable = VK_FALSE;
			#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
				s->SetData(&Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer, 0);
			#else
				s->SetData(&Geometry::globalBuffers.vertexBuffer, &Geometry::globalBuffers.indexBuffer, 0);
			#endif
		}
		
		for (auto* s : shaderGroups["sg_wireframe"]) {
			s->AddVertexInputBinding(sizeof(Geometry::VertexBuffer_T), VK_VERTEX_INPUT_RATE_VERTEX, Geometry::VertexBuffer_T::GetInputAttributes());
			s->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			s->rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
			s->rasterizer.cullMode = VK_CULL_MODE_NONE;
			s->depthStencilState.depthTestEnable = VK_FALSE;
			s->depthStencilState.depthWriteEnable = VK_FALSE;
			s->dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
			#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
				s->SetData(&Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer, 0);
			#else
				s->SetData(&Geometry::globalBuffers.vertexBuffer, &Geometry::globalBuffers.indexBuffer, 0);
			#endif
		}
	}
	
	void ConfigureLightingShaders() {
		for (auto[rp, ss] : shaderGroups) if (rp == "sg_lighting" || rp == "sg_lighting_rtx" || rp == "sg_fog") {
			for (auto* s : ss) {
				s->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				s->depthStencilState.depthTestEnable = VK_FALSE;
				s->depthStencilState.depthWriteEnable = VK_FALSE;
				s->rasterizer.cullMode = VK_CULL_MODE_NONE;
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
			scene->Lock();
			for (auto* s : shaderGroups["sg_visibility"]) {
				if (
					// #ifdef _DEBUG
						!DEBUG_OPTIONS::WIREFRAME || s == &shader_debug_wireframe
					// #else
						// true
					// #endif
				) {
					for (auto obj : scene->objectInstances) {
						if (obj) {
							// obj->Lock();
							if (obj->IsActive() && (obj->rayTracingMaskRemoved & GEOMETRY_ATTR_PRIMARY_VISIBLE)==0) {
								// Geometries
								for (auto& geom : obj->GetGeometries()) {
									if (geom.geometry->active && (
										// #ifdef _DEBUG
											(DEBUG_OPTIONS::WIREFRAME && Geometry::geometryRenderTypes[geom.type].rasterShader->inputAssembly.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) || 
										// #endif
										s == Geometry::geometryRenderTypes[geom.type].rasterShader
										)) {
										// Frustum culling
										if (scene->camera.IsVisibleInScreen((obj->GetWorldTransform() * glm::dmat4(geom.transform))[3], geom.geometry->boundingDistance)) {
											if (geom.geometry->isProcedural) {
												s->vertexOffset = geom.geometry->vertexOffset * sizeof(Geometry::ProceduralVertexBuffer_T);
												GeometryPushConstant geometryPushConstant {};
													geometryPushConstant.objectIndex = obj->GetObjectOffset();
													geometryPushConstant.geometryIndex = geom.geometry->geometryOffset;
												s->Execute(device, commandBuffer, 1, &geometryPushConstant);
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
				} // else {
				// 	s->Execute(device, commandBuffer);
				// }
			}
			scene->Unlock();
		visibilityRasterPass.End(device, commandBuffer);
	}
	
	void RunRasterLighting(Device* device, VkCommandBuffer commandBuffer, bool runSubpass0, bool runSubpass1) {
		if (!runSubpass0 && !runSubpass1) return;
		lightingRenderPass.Begin(device, commandBuffer, img_lit);
			if (runSubpass0) {
				if (r->rayTracingFeatures.rayQuery) {
					for (auto* s : shaderGroups["sg_lighting_rtx"]) {
						s->Execute(device, commandBuffer);
					}
				} else {
					for (auto* s : shaderGroups["sg_lighting"]) {
						s->Execute(device, commandBuffer);
					}
				}
			}
			r->renderingDevice->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
			if (runSubpass1) {
				for (auto* s : shaderGroups["sg_fog"]) {
					s->Execute(device, commandBuffer);
				}
				for (auto* s : shaderGroups["sg_transparent"]) {
					// Transparent
					if (s->GetShaderPath("vert") == rasterTrianglesDefaultVertexShader) {
						scene->Lock();
							for (auto obj : scene->objectInstances) {
								if (obj) {
									// obj->Lock();
									if (obj->IsActive()) {
										// Geometries
										for (auto& geom : obj->GetGeometries()) {
											if (geom.geometry->active && (s == Geometry::geometryRenderTypes[geom.type].rasterShader)) {
												// Frustum culling
												if (scene->camera.IsVisibleInScreen((obj->GetWorldTransform() * glm::dmat4(geom.transform))[3], geom.geometry->boundingDistance)) {
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
									// obj->Unlock();
								}
							}
						scene->Unlock();
					} else {
						s->Execute(device, commandBuffer);
					}
				}
				for (auto* s : shaderGroups["sg_wireframe"]) {
					// Wireframe
					if (s->GetShaderPath("vert") == rasterTrianglesDefaultVertexShader) {
						scene->Lock();
							for (auto obj : scene->objectInstances) {
								if (obj) {
									// obj->Lock();
									if (obj->IsActive()) {
										// Geometries
										for (auto& geom : obj->GetGeometries()) {
											if (geom.geometry->active && geom.geometry->renderWireframe) {
												// Frustum culling
												if (scene->camera.IsVisibleInScreen((obj->GetWorldTransform() * glm::dmat4(geom.transform))[3], geom.geometry->boundingDistance)) {
													s->vertexOffset = geom.geometry->vertexOffset * sizeof(Geometry::VertexBuffer_T);
													s->indexOffset = geom.geometry->indexOffset * sizeof(Geometry::IndexBuffer_T);
													s->indexCount = geom.geometry->indexCount;
													GeometryPushConstant geometryPushConstant {};
														geometryPushConstant.objectIndex = obj->GetObjectOffset();
														geometryPushConstant.geometryIndex = geom.geometry->geometryOffset;
														geometryPushConstant.wireframeColor = geom.geometry->wireframeColor;
													s->Bind(device, commandBuffer);
													s->PushConstant(device, commandBuffer, &geometryPushConstant, 0);
													device->CmdSetLineWidth(commandBuffer, geom.geometry->wireframeThickness);
													s->Render(device, commandBuffer, 1);
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
			}
		lightingRenderPass.End(device, commandBuffer);
	}
	
#pragma endregion

#pragma region Ray-Tracing Pipeliens & Resouces
	VkPhysicalDeviceRayTracingPropertiesKHR rayTracingProperties{};
	AccelerationStructure topLevelAccelerationStructure {};
	Buffer rayTracingShaderBindingTableBuffer_visibility {VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR};
	Buffer rayTracingShaderBindingTableBuffer_lighting {VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR};
	std::recursive_mutex rayTracingInstanceMutex, blasBuildQueueMutex;
	std::vector<VkAccelerationStructureBuildGeometryInfoKHR> blasQueueBuildGeometryInfos {};
	std::vector<VkAccelerationStructureBuildOffsetInfoKHR*> blasQueueBuildOffsetInfos {};
	StagedBuffer rayTracingInstanceBuffer {VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, sizeof(RayTracingBLASInstance)*RAY_TRACING_TLAS_MAX_INSTANCES};
	RayTracingBLASInstance* rayTracingInstances = nullptr;
	uint32_t nbRayTracingInstances = 0;
	Buffer globalScratchBuffer {VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 16*1024*1024/* 16 MB */};
	static const bool globalScratchDynamicSize = false;
	static const int maxBlasBuildsPerFrame = 8;
	std::map<int/*instance index*/, std::shared_ptr<Geometry>> activeRayTracedGeometries {};
	std::mutex geometriesToRemoveFromRayTracingInstancesMutex;
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
		topLevelAccelerationStructure.SetInstanceBuffer(r->renderingDevice, rayTracingInstanceBuffer.deviceLocalBuffer.buffer);
	}
	void DestroyRayTracingResources() {
		topLevelAccelerationStructure.Free(r->renderingDevice);
		topLevelAccelerationStructure.Destroy(r->renderingDevice);
		
		activeRayTracedGeometries.clear();
		blasBuildsForGlobalScratchBufferReallocation.clear();
		ResetRayTracingBlasBuilds();
	}
	
	void AllocateRayTracingBuffers() {
		rayTracingInstanceBuffer.Allocate(r->renderingDevice);
		rayTracingInstances = (RayTracingBLASInstance*)rayTracingInstanceBuffer.stagingBuffer.data;
		
		if (AccelerationStructure::useGlobalScratchBuffer) {
			globalScratchBuffer.Allocate(r->renderingDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);
			topLevelAccelerationStructure.SetGlobalScratchBuffer(r->renderingDevice, globalScratchBuffer.buffer);
		}
	}
	void FreeRayTracingBuffers() {
		rayTracingInstances = nullptr;
		nbRayTracingInstances = 0;
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
		// Visibility Miss
		shaderBindingTables["sbt_visibility"]->AddMissShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.rmiss"));
		
		// Lighting Miss (Shadow)
		shaderBindingTables["sbt_lighting"]->AddMissShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_lighting.shadow.rmiss"));
		
		// Basic
		Geometry::geometryRenderTypes["basic"].sbtOffset = 
			shaderBindingTables["sbt_visibility"]->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.basic.rchit"));
			shaderBindingTables["sbt_lighting"]->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.basic.rchit"));
		
		// Standard
		Geometry::geometryRenderTypes["standard"].sbtOffset = 
			shaderBindingTables["sbt_visibility"]->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.standard.rchit"));
			shaderBindingTables["sbt_lighting"]->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.standard.rchit"));
		
		// Terrain
		Geometry::geometryRenderTypes["terrain"].sbtOffset = 
			shaderBindingTables["sbt_visibility"]->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.terrain.rchit"));
			shaderBindingTables["sbt_lighting"]->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.terrain.rchit"));
		
		// Sphere
		Geometry::geometryRenderTypes["aabb"].sbtOffset = 
			shaderBindingTables["sbt_visibility"]->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.aabb.rchit"), "", V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.aabb.rint"));
			shaderBindingTables["sbt_lighting"]->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.aabb.rchit"), "", V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.aabb.rint"));
		
		// Sphere
		Geometry::geometryRenderTypes["sphere"].sbtOffset = 
			shaderBindingTables["sbt_visibility"]->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.sphere.rchit"), "", V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.sphere.rint"));
			shaderBindingTables["sbt_lighting"]->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.sphere.rchit"), "", V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.sphere.rint"));
		
		// Light
		Geometry::geometryRenderTypes["light"].sbtOffset = 
			shaderBindingTables["sbt_visibility"]->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.light.rchit"), "", V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.sphere.rint"));
			shaderBindingTables["sbt_lighting"]->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.light.rchit"), "", V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.sphere.rint"));
	
		// Sun
		Geometry::geometryRenderTypes["sun"].sbtOffset = 
			shaderBindingTables["sbt_visibility"]->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.sun.rchit"), "", V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.sphere.rint"));
			shaderBindingTables["sbt_lighting"]->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.sun.rchit"), "", V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rtx_visibility.sphere.rint"));
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
	
		// Finish visibility before running lighting
		VkMemoryBarrier memoryBarrier {
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			nullptr,// pNext
			VK_ACCESS_MEMORY_WRITE_BIT,// VkAccessFlags srcAccessMask
			VK_ACCESS_MEMORY_READ_BIT,// VkAccessFlags dstAccessMask
		};
		r->renderingDevice->CmdPipelineBarrier(
			commandBuffer, 
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 
			RENDER_OPTIONS::RAY_TRACED_LIGHTING? VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
			0, 
			1, &memoryBarrier, 
			0, 0, 
			0, 0
		);
	}
	
	void RunRayTracingLightingCommands(VkCommandBuffer commandBuffer) {
		
		// Finish visibility before running lighting
		if (!RENDER_OPTIONS::RAY_TRACED_VISIBILITY) {
			VkMemoryBarrier memoryBarrier {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				nullptr,// pNext
				VK_ACCESS_MEMORY_WRITE_BIT,// VkAccessFlags srcAccessMask
				VK_ACCESS_MEMORY_READ_BIT,// VkAccessFlags dstAccessMask
			};
			r->renderingDevice->CmdPipelineBarrier(
				commandBuffer, 
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
				VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 
				0, 
				1, &memoryBarrier, 
				0, 0, 
				0, 0
			);
		}
		
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
			#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
				VkBufferMemoryBarrier bufferBarriers[2];
				bufferBarriers[0] = {};
					bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
					bufferBarriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
					bufferBarriers[0].dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
					bufferBarriers[0].offset = 0;
					bufferBarriers[0].size = Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer.size;
					bufferBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					bufferBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					bufferBarriers[0].buffer = Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer.buffer;
				bufferBarriers[1] = {};
					bufferBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
					bufferBarriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
					bufferBarriers[1].dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
					bufferBarriers[1].offset = 0;
					bufferBarriers[1].size = Geometry::globalBuffers.indexBuffer.deviceLocalBuffer.size;
					bufferBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					bufferBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					bufferBarriers[1].buffer = Geometry::globalBuffers.indexBuffer.deviceLocalBuffer.buffer;
				device->CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 0, nullptr, 2, bufferBarriers, 0, nullptr);
			#endif
			
			device->CmdBuildAccelerationStructureKHR(commandBuffer, blasQueueBuildGeometryInfos.size(), blasQueueBuildGeometryInfos.data(), blasQueueBuildOffsetInfos.data());
			
			{// Wait for BLAS to finish before building TLAS
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
	}
	
	void BuildTopLevelRayTracingAccelerationStructure(Device* device, VkCommandBuffer commandBuffer) {
		
		// Push new instance buffer
		rayTracingInstanceBuffer.Update(device, commandBuffer);
		
		{// Wait for buffer transfer to finish before (re)building TLAS
			VkMemoryBarrier memoryBarrier {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				nullptr,// pNext
				VK_ACCESS_TRANSFER_READ_BIT,// VkAccessFlags srcAccessMask
				VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,// VkAccessFlags dstAccessMask
			};
			device->CmdPipelineBarrier(
				commandBuffer, 
				VK_PIPELINE_STAGE_TRANSFER_BIT, 
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
				0, 
				1, &memoryBarrier, 
				0, 0, 
				0, 0
			);
		}
		
		std::lock_guard lock(rayTracingInstanceMutex);
		static const VkAccelerationStructureBuildOffsetInfoKHR* topLevelAccelerationStructureGeometriesOffsets = &topLevelAccelerationStructure.buildOffsetInfo;
		topLevelAccelerationStructure.SetInstanceCount(nbRayTracingInstances);
		device->CmdBuildAccelerationStructureKHR(commandBuffer, 1, &topLevelAccelerationStructure.buildGeometryInfo, &topLevelAccelerationStructureGeometriesOffsets);
	
		// Wait for TLAS to finish before calling any ray tracing shader
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
	
	void AddRayTracingInstance(ObjectInstancePtr obj, GeometryInstance* geometryInstance, const glm::dmat4& objectViewTransform) {
		std::lock_guard lock(rayTracingInstanceMutex);
		if (!geometryInstance->geometry->blas || !geometryInstance->geometry->blas->handle) return;
		// create new instance
		int index = nbRayTracingInstances++;
		rayTracingInstances[index].customInstanceId = geometryInstance->geometry->geometryOffset;
		rayTracingInstances[index].shaderInstanceOffset = (uint32_t)Geometry::geometryRenderTypes[geometryInstance->type].sbtOffset;
		activeRayTracedGeometries[index] = geometryInstance->geometry;
		// assign blas handle
		rayTracingInstances[index].accelerationStructureHandle = geometryInstance->geometry->blas->handle;
		// assign transform and data
		rayTracingInstances[index].mask = (geometryInstance->geometry->rayTracingMask | obj->rayTracingMaskAdded) & ~obj->rayTracingMaskRemoved;
		rayTracingInstances[index].flags = geometryInstance->geometry->flags;
		rayTracingInstances[index].transform = glm::transpose(glm::mat4(objectViewTransform * geometryInstance->transform));
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
				},
			};
			postProcessingRenderPass.renderPassInfo.dependencyCount = subPassDependencies.size();
			postProcessingRenderPass.renderPassInfo.pDependencies = subPassDependencies.data();
			
			// Create the render pass
			postProcessingRenderPass.Create(r->renderingDevice);
			postProcessingRenderPass.CreateFrameBuffers(r->renderingDevice, r->swapChain, {
				img_pp.view, 
				img_history.view, 
				VK_NULL_HANDLE,
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
		shader_raycast_compute.CreatePipeline(r->renderingDevice);
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
		shader_raycast_compute.DestroyPipeline(r->renderingDevice);
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
	
	void AllocateComputeBuffers() {
		// histogram
		totalLuminance.Allocate(r->renderingDevice, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		totalLuminance.MapMemory(r->renderingDevice);
		glm::vec4 luminance {0,0,0,1};
		totalLuminance.WriteToMappedData(&luminance);
		// raycast
		raycastBuffer.Allocate(r->renderingDevice, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		raycastBuffer.MapMemory(r->renderingDevice);
		currentRayCast = {};
		raycastBuffer.WriteToMappedData(&currentRayCast);
	}
	void FreeComputeBuffers() {
		// histogram
		totalLuminance.UnmapMemory(r->renderingDevice);
		totalLuminance.Free(r->renderingDevice);
		// raycast
		currentRayCast = {};
		raycastBuffer.UnmapMemory(r->renderingDevice);
		raycastBuffer.Free(r->renderingDevice);
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
		// histogram
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
	void RunTXAA() {
		// TXAA
		if (RENDER_OPTIONS::TXAA && !DEBUG_OPTIONS::WIREFRAME && !DEBUG_OPTIONS::PHYSICS) {
			static unsigned long frameCount = 0;
			static const glm::dvec2 samples8[8] = {
				glm::dvec2(-7.0, 1.0) / 8.0,
				glm::dvec2(-5.0, -5.0) / 8.0,
				glm::dvec2(-1.0, -3.0) / 8.0,
				glm::dvec2(3.0, -7.0) / 8.0,
				glm::dvec2(5.0, -1.0) / 8.0,
				glm::dvec2(7.0, 7.0) / 8.0,
				glm::dvec2(1.0, 3.0) / 8.0,
				glm::dvec2(-3.0, 5.0) / 8.0
			};
			glm::dvec2 texelSize = 1.0 / glm::dvec2(scene->camera.width, scene->camera.height);
			glm::dvec2 subSample = samples8[frameCount % 8] * texelSize * double(scene->camera.txaaKernelSize);
			scene->camera.projectionMatrix[2].x = subSample.x;
			scene->camera.projectionMatrix[2].y = subSample.y;
			// historyTxaaOffset = txaaOffset;
			scene->camera.txaaOffset = subSample / 2.0;
			frameCount++;
			
			scene->camera.reprojectionMatrix = (scene->camera.projectionMatrix * scene->camera.historyViewMatrix) * glm::inverse(scene->camera.projectionMatrix * scene->camera.viewMatrix);
			
			// Save Projection and View matrices from previous frame
			scene->camera.historyViewMatrix = scene->camera.viewMatrix;
		}
	}
	
#pragma endregion

#pragma region UI
	float img_overlayScale = 1.0;
	
	static const int MAX_OVERLAY_LINES = 4000000;
	static const int MAX_OVERLAY_TEXT_CHARS = 32768;
	static const int MAX_OVERLAY_CIRCLES = 4096;
	static const int MAX_OVERLAY_SQUARES = 4096;
	std::mutex overlayMutex;
	// Overlay Lines
	struct OverlayLine {
		float x;
		float y;
		float color;
		float width;
	};
	Buffer overlayLinesBuffer {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(OverlayLine) * MAX_OVERLAY_LINES*2};
	OverlayLine* overlayLines = nullptr;
	size_t overlayLinesCount = 0;
	void AddOverlayLine(float x1, float y1, float x2, float y2, glm::vec4 color = {1,1,1,1}, float lineWidth = 1) {
		std::lock_guard lock(overlayMutex);
		if (overlayLinesCount+2 > MAX_OVERLAY_LINES) {LOG_WARN("Overlay Line buffer overlow") overlayLinesCount=0;return;}
		auto floatColor = PackColorAsFloat(color);
		overlayLines[overlayLinesCount++] = {x1, y1, floatColor, lineWidth};
		overlayLines[overlayLinesCount++] = {x2, y2, floatColor, lineWidth};
	}
	// Overlay Text
	struct OverlayText {
		float x;
		float y;
		float color;
		float c;
	};
	Buffer overlayTextBuffer {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(OverlayText) * MAX_OVERLAY_TEXT_CHARS};
	OverlayText* overlayText = nullptr;
	size_t overlayTextSize = 0;
	void AddOverlayText(char c, float x, float y, glm::vec4 color = {1,1,1,1}, uint16_t size = 20, uint8_t flags = 0) {
		std::lock_guard lock(overlayMutex);
		if (overlayTextSize+1 > MAX_OVERLAY_TEXT_CHARS) {LOG_WARN("Overlay Text buffer overlow") overlayTextSize=0;return;}
		overlayText[overlayTextSize++] = {x, y, PackColorAsFloat(color), glm::uintBitsToFloat((size << 16) | (c << 8) | flags)};
	}
	void AddOverlayText(const std::string& text, float x, float y, glm::vec4 color = {1,1,1,1}, uint16_t size = 20, uint8_t flags = 0, float letterSpacing = 0, float fontSizeRatio = 0.5) {
		const float letterWidth = (float(size)*fontSizeRatio+letterSpacing) / scene->camera.width * 2;
		x -= letterWidth * float(text.length()-1) / 2;
		for (int i = 0; i < text.length(); ++i) {
			AddOverlayText(text[i], x, y, color, size, flags);
			x += letterWidth;
		}
	}
	// Overlay Circles
	struct OverlayCircle {
		float x;
		float y;
		float color;
		float size;
	};
	Buffer overlayCirclesBuffer {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(OverlayCircle) * MAX_OVERLAY_CIRCLES};
	OverlayCircle* overlayCircles = nullptr;
	size_t overlayCirclesCount = 0;
	void AddOverlayCircle(float x, float y, glm::vec4 color = {1,1,1,1}, uint16_t size = 20, uint8_t border = 0) {
		std::lock_guard lock(overlayMutex);
		if (overlayCirclesCount+1 > MAX_OVERLAY_CIRCLES) {LOG_WARN("Overlay Circles buffer overlow") overlayCirclesCount=0;return;}
		overlayCircles[overlayCirclesCount++] = {x, y, PackColorAsFloat(color), glm::uintBitsToFloat((size << 20) | (size << 8) | border)};
	}
	// Overlay Squares
	struct OverlaySquare {
		float x;
		float y;
		float color;
		float size;
	};
	Buffer overlaySquaresBuffer {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(OverlaySquare) * MAX_OVERLAY_SQUARES};
	OverlaySquare* overlaySquares = nullptr;
	size_t overlaySquaresCount = 0;
	void AddOverlaySquare(float x, float y, glm::vec4 color = {1,1,1,1}, glm::mediump_u16vec2 size = {20,20}, uint8_t border = 0) {
		std::lock_guard lock(overlayMutex);
		if (overlaySquaresCount+1 > MAX_OVERLAY_SQUARES) {LOG_WARN("Overlay Squares buffer overlow") overlaySquaresCount=0;return;}
		overlaySquares[overlaySquaresCount++] = {x, y, PackColorAsFloat(color), glm::uintBitsToFloat((size.x << 20) | (size.y << 8) | border)};
	}
	
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
	
	void ConfigureOverlayShaders() {
		// Lines
		shader_overlay_lines.AddVertexInputBinding(16, VK_VERTEX_INPUT_RATE_VERTEX, {{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT}});
		shader_overlay_lines.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		shader_overlay_lines.rasterizer.cullMode = VK_CULL_MODE_NONE;
		shader_overlay_lines.depthStencilState.depthTestEnable = false;
		shader_overlay_lines.depthStencilState.depthWriteEnable = false;
		shader_overlay_lines.dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
		shader_overlay_lines.SetData(&overlayLinesBuffer, 0);
		
		// Text
		shader_overlay_text.AddVertexInputBinding(16, VK_VERTEX_INPUT_RATE_VERTEX, {{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT}});
		shader_overlay_text.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		shader_overlay_text.rasterizer.cullMode = VK_CULL_MODE_NONE;
		shader_overlay_text.depthStencilState.depthTestEnable = false;
		shader_overlay_text.depthStencilState.depthWriteEnable = false;
		shader_overlay_text.SetData(&overlayTextBuffer, 0);
		
		// Circles
		shader_overlay_circles.AddVertexInputBinding(16, VK_VERTEX_INPUT_RATE_VERTEX, {{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT}});
		shader_overlay_circles.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		shader_overlay_circles.rasterizer.cullMode = VK_CULL_MODE_NONE;
		shader_overlay_circles.depthStencilState.depthTestEnable = false;
		shader_overlay_circles.depthStencilState.depthWriteEnable = false;
		shader_overlay_circles.SetData(&overlayCirclesBuffer, 0);
		
		// Squares
		shader_overlay_squares.AddVertexInputBinding(16, VK_VERTEX_INPUT_RATE_VERTEX, {{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT}});
		shader_overlay_squares.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		shader_overlay_squares.rasterizer.cullMode = VK_CULL_MODE_NONE;
		shader_overlay_squares.depthStencilState.depthTestEnable = false;
		shader_overlay_squares.depthStencilState.depthWriteEnable = false;
		shader_overlay_squares.SetData(&overlaySquaresBuffer, 0);
	}
	
	#ifdef _ENABLE_IMGUI
		void LoadImGui() {
			static ImGui_ImplVulkan_InitInfo init_info {};
				init_info.Instance = r->GetHandle();
				init_info.PhysicalDevice = r->renderingDevice->GetPhysicalDevice()->GetHandle();
				init_info.Device = r->renderingDevice->GetHandle();
				init_info.QueueFamily = r->renderingDevice->GetQueue("graphics").familyIndex;
				init_info.Queue = r->renderingDevice->GetQueue("graphics").handle;
				init_info.DescriptorPool = r->descriptorPool;
				init_info.MinImageCount = r->swapChain->images.size();
				init_info.ImageCount = r->swapChain->images.size();
				init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
				init_info.CheckVkResultFn = [](VkResult err){if (err) LOG_ERROR("ImGui Vk Error " << (int)err)};
			ImGui_ImplVulkan_Init(&init_info, uiRenderPass.handle);
			// Clear any existing draw data
			auto* drawData = ImGui::GetDrawData();
			if (drawData) drawData->Clear();
			// Font Upload
			auto cmdBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
				ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer);
			r->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), cmdBuffer);
			ImGui_ImplVulkan_DestroyFontUploadObjects();
		}
		void UnloadImGui() {
			// Clear any existing draw data
			auto* drawData = ImGui::GetDrawData();
			if (drawData) drawData->Clear();
			// Shutdown ImGui
			ImGui_ImplVulkan_Shutdown();
		}
		void DrawImGui(VkCommandBuffer commandBuffer) {
			auto* drawData = ImGui::GetDrawData();
			if (drawData) {
				ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
			}
		}
	#endif


#pragma endregion

#pragma region Frame Update

	void FrameUpdate(uint imageIndex) {
		
		{// Handle RayCast from previous frame here before reassigning anything in the geometry buffers
			currentRayCast = *((RayCast*)raycastBuffer.data);
			if (currentRayCast.hit) {
				auto objData = ((Geometry::ObjectBuffer_T*)(Geometry::globalBuffers.objectBuffer.stagingBuffer.data))[currentRayCast.objectBufferOffset];
				if (objData.moduleVen != 0 && objData.moduleId != 0) {
					auto mod = V4D_Game::LoadModule(v4d::modular::ModuleID(objData.moduleVen, objData.moduleId));
					if (mod && mod->RendererRayCast) {
						RenderRayCastHit hit;
							hit.position = glm::inverse(objData.modelTransform) * glm::inverse(scene->camera.viewMatrix) * glm::dvec4(currentRayCast.position, 1);
							hit.distance = currentRayCast.distance;
							hit.objId = objData.objId;
							hit.flags = currentRayCast.flags;
							hit.customType = currentRayCast.customType;
							hit.customData0 = currentRayCast.customData0;
							hit.customData1 = currentRayCast.customData1;
							hit.customData2 = currentRayCast.customData2;
						mod->RendererRayCast(hit);
					}
				}
			}
		}
		
		{// Reset camera information
			scene->camera.width = r->swapChain->extent.width;
			scene->camera.height = r->swapChain->extent.height;
			scene->camera.RefreshProjectionMatrix();
			scene->camera.time = float(v4d::Timer::GetCurrentTimestamp() - 1587838909.0);
		}
		
		RunTXAA();
		
		// Modules
		V4D_Game::ForEachSortedModule([](auto* mod){
			if (mod->RendererFrameUpdate) mod->RendererFrameUpdate();
		});
		
		// Ray Tracing
		size_t globalScratchBufferSize = 0;
		{
			if (r->rayTracingFeatures.rayTracing) {
				topLevelAccelerationStructure.GetMemoryRequirementSizeForScratchBuffer(r->renderingDevice);
			}
			if (r->rayTracingFeatures.rayTracing) {
				ResetRayTracingBlasBuilds();
				// inactiveBlas.clear();
			}
		}
		
		nbRayTracingInstances = 0;
		activeRayTracedGeometries.clear();
		
		scene->Lock();
		{// Update object transforms and light sources (Use all lights for now)
			std::scoped_lock lock(geometriesToRemoveFromRayTracingInstancesMutex, rayTracingInstanceMutex);
			nbActiveLights = 0;
			for (auto obj : scene->objectInstances) {
				obj->Lock();
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
								if (Geometry::geometryRenderTypes[geom.type].sbtOffset != -1) {
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
											AddRayTracingInstance(obj, &geom, scene->camera.viewMatrix * obj->GetWorldTransform());
										}
									}
								}
							}
						}
					}
				obj->Unlock();
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
		scene->camera.renderOptions = RENDER_OPTIONS::Get();
		scene->camera.debugOptions = DEBUG_OPTIONS::Get();
		cameraUniformBuffer.Update(r->renderingDevice, commandBuffer);
		activeLightsUniformBuffer.Update(r->renderingDevice, commandBuffer, sizeof(uint32_t)/*lightCount*/ + sizeof(uint32_t)*nbActiveLights/*lightIndices*/);
		Geometry::globalBuffers.PushObjects(r->renderingDevice, commandBuffer);
		Geometry::globalBuffers.PushLights(r->renderingDevice, commandBuffer);
		if (r->rayTracingFeatures.rayTracing) {
			for (auto[i,geometry] : activeRayTracedGeometries) {
				if (geometry) geometry->AutoPush(r->renderingDevice, commandBuffer, true);
			}
		}
		// Geometry::globalBuffers.PushGeometriesInfo(r->renderingDevice, commandBuffer);
		scene->Lock();
			for (auto obj : scene->objectInstances) if (obj && obj->IsActive()) {
				obj->Lock();
					for (auto& geom : obj->GetGeometries()) if (geom.geometry && geom.geometry->active && (!r->rayTracingFeatures.rayTracing || Geometry::geometryRenderTypes[geom.type].sbtOffset == -1)) {
						geom.geometry->AutoPush(r->renderingDevice, commandBuffer, true);
					}
				obj->Unlock();
			}
		scene->Unlock();
		
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
		// raycast compute
		shader_raycast_compute.SetGroupCounts(1, 1, 1);
		shader_raycast_compute.Execute(r->renderingDevice, commandBuffer);
		
		RecordPostProcessingCommands(commandBuffer, imageIndex);
	}

#pragma endregion

///////////////////////////////////////////////////////////

V4D_MODULE_CLASS(V4D_Renderer) {
	
	V4D_MODULE_FUNC(bool, ModuleIsPrimary) {return true;}
	V4D_MODULE_FUNC(int, OrderIndex) {return -1000;}
	
	#pragma region Containers Access
		
		V4D_MODULE_FUNC(Image,* GetImage, const std::string& name) {
			if (images.find(name) == images.end()) {
				throw std::runtime_error(std::string("Image '") + name + "' does not exist");
				return nullptr;
			}
			return images[name];
		}
		
		V4D_MODULE_FUNC(PipelineLayout*, GetPipelineLayout, const std::string& name) {
			if (pipelineLayouts.find(name) == pipelineLayouts.end()) {
				throw std::runtime_error(std::string("Pipeline layout '") + name + "' does not exist");
				return nullptr;
			}
			return pipelineLayouts[name];
		}
		
		V4D_MODULE_FUNC(void, AddShader, const std::string& groupName, RasterShaderPipeline* shader) {
			if (shaderGroups.find(groupName) == shaderGroups.end()) {
				throw std::runtime_error(std::string("Shader group '") + groupName + "' does not exist");
				return;
			}
			shaderGroups[groupName].push_back(shader);
		}
		
		V4D_MODULE_FUNC(ShaderBindingTable*, GetShaderBindingTable, const std::string& sbtName) {
			if (shaderBindingTables.find(sbtName) == shaderBindingTables.end()) {
				throw std::runtime_error(std::string("Shader binding table '") + sbtName + "' does not exist");
				return nullptr;
			}
			return shaderBindingTables[sbtName];
		}
		
	#pragma endregion
	
	#pragma region Graphics configuration / Init
		
		V4D_MODULE_FUNC(void, ScorePhysicalDeviceSelection, int& score, PhysicalDevice* physicalDevice) {
			// Higher score for Ray Tracing support
			if (physicalDevice->GetRayTracingFeatures().rayTracing) {
				score += 2;
			}
			if (physicalDevice->GetRayTracingFeatures().rayQuery) {
				score += 1;
			}
		}
		
		V4D_MODULE_FUNC(void, Init, Renderer* _r, Scene* _s) {
			r = _r;
			scene = _s;
			
			V4D_Renderer::SortModules([](auto* a, auto* b){
				return (a->RenderOrderIndex? a->RenderOrderIndex():0) < (b->RenderOrderIndex? b->RenderOrderIndex():0);
			}, "render");
			
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
		
		V4D_MODULE_FUNC(void, InitDeviceFeatures) {
			r->deviceFeatures.shaderFloat64 = VK_TRUE;
			r->deviceFeatures.depthClamp = VK_TRUE;
			r->deviceFeatures.fillModeNonSolid = VK_TRUE;
			r->deviceFeatures.geometryShader = VK_TRUE;
			r->deviceFeatures.wideLines = VK_TRUE;
			
			// Vulkan 1.2
			if (Loader::VULKAN_API_VERSION >= VK_API_VERSION_1_2) {
				r->EnableVulkan12DeviceFeatures()->bufferDeviceAddress = VK_TRUE;
				r->EnableRayTracingFeatures()->rayTracing = VK_TRUE;
				r->EnableRayTracingFeatures()->rayQuery = VK_TRUE;
			}
		}
		
		V4D_MODULE_FUNC(void, ConfigureRenderer) {
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
				RENDER_OPTIONS::RAY_TRACED_VISIBILITY = false;
				RENDER_OPTIONS::RAY_TRACED_LIGHTING = false;
				RENDER_OPTIONS::HARD_SHADOWS = false;
			}
		}
	
	#pragma endregion
	
	V4D_MODULE_FUNC(void, InitLayouts) {
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
			set0_base.AddBinding_accelerationStructure(7, &topLevelAccelerationStructure.accelerationStructure, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT);
		}
			
		{r->descriptorSets["set1_visibility_raster"] = &set1_visibility_raster;
			//
		}
		
		{r->descriptorSets["set1_visibility_rays"] = &set1_visibility_rays;
			int i = 0;
			for (auto* img : gBuffers) set1_visibility_rays.AddBinding_imageView(i++, img, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
			set1_visibility_rays.AddBinding_imageView(i++, &img_depth, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		}
		
		{r->descriptorSets["set1_lighting_rays"] = &set1_lighting_rays;
			int i = 0;
			for (auto* img : gBuffers) set1_lighting_rays.AddBinding_imageView(i++, img, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
			set1_lighting_rays.AddBinding_imageView(i++, &img_depth, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
			set1_lighting_rays.AddBinding_imageView(i++, &img_lit, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
			set1_lighting_rays.AddBinding_combinedImageSampler(i++, &img_rasterDepth, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		}
		
		{r->descriptorSets["set1_lighting_raster"] = &set1_lighting_raster;
			int i = 0;
			for (auto* img : gBuffers) set1_lighting_raster.AddBinding_inputAttachment(i++, img, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_lighting_raster.AddBinding_combinedImageSampler(i++, &img_depth, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_lighting_raster.AddBinding_combinedImageSampler(i++, &img_rasterDepth, VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_transparent"] = &set1_transparent;
			int i = 0;
			for (auto* img : gBuffers) set1_transparent.AddBinding_inputAttachment(i++, img, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_transparent.AddBinding_combinedImageSampler(i++, &img_depth, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_transparent.AddBinding_combinedImageSampler(i++, &img_rasterDepth, VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_fog_raster"] = &set1_fog_raster;
			int i = 0;
			for (auto* img : gBuffers) set1_fog_raster.AddBinding_inputAttachment(i++, img, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_fog_raster.AddBinding_combinedImageSampler(i++, &img_depth, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_fog_raster.AddBinding_combinedImageSampler(i++, &img_rasterDepth, VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_post"] = &set1_post;
			int i = 0;
			set1_post.AddBinding_combinedImageSampler(i++, &img_lit, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_overlay, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_history, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_depth, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_rasterDepth, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_inputAttachment(i++, &img_pp, VK_SHADER_STAGE_FRAGMENT_BIT);
			
				// This is there mostly for debugging them.... may remove if it affects performance
				for (auto* img : gBuffers) set1_post.AddBinding_combinedImageSampler(i++, img, VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_overlay"] = &set1_overlay;
			set1_overlay.AddBinding_combinedImageSampler(0, tex_img_font_atlas.GetImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_thumbnail"] = &set1_thumbnail;
			set1_thumbnail.AddBinding_combinedImageSampler(0, &img_lit, VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_histogram"] = &set1_histogram;
			set1_histogram.AddBinding_imageView(0, &img_thumbnail, VK_SHADER_STAGE_COMPUTE_BIT);
			set1_histogram.AddBinding_storageBuffer(1, &totalLuminance, VK_SHADER_STAGE_COMPUTE_BIT);
		}
		
		{r->descriptorSets["set1_raycast"] = &set1_raycast;
			int i = 0;
			for (auto* img : gBuffers) set1_raycast.AddBinding_imageView(i++, img, VK_SHADER_STAGE_COMPUTE_BIT);
			set1_raycast.AddBinding_storageBuffer(i++, &raycastBuffer, VK_SHADER_STAGE_COMPUTE_BIT);
		}
		
		{ // Assign descriptor sets to layouts
			// Add the same set 0 to all pipeline layouts
			for (auto[name, layout] : pipelineLayouts) {
				layout->AddDescriptorSet(&set0_base);
			}
			// Add specific set 1 to specific layout lists
			pipelineLayouts["pl_visibility_raster"]->AddDescriptorSet(&set1_visibility_raster);
			pipelineLayouts["pl_visibility_rays"]->AddDescriptorSet(&set1_visibility_rays);
			pipelineLayouts["pl_lighting_raster"]->AddDescriptorSet(&set1_lighting_raster);
			pipelineLayouts["pl_transparent"]->AddDescriptorSet(&set1_transparent);
			pipelineLayouts["pl_fog_raster"]->AddDescriptorSet(&set1_fog_raster);
			pipelineLayouts["pl_lighting_rays"]->AddDescriptorSet(&set1_lighting_rays);
			pipelineLayouts["pl_thumbnail"]->AddDescriptorSet(&set1_thumbnail);
			pipelineLayouts["pl_overlay"]->AddDescriptorSet(&set1_overlay);
			pipelineLayouts["pl_post"]->AddDescriptorSet(&set1_post);
			pipelineLayouts["pl_histogram"]->AddDescriptorSet(&set1_histogram);
			pipelineLayouts["pl_raycast"]->AddDescriptorSet(&set1_raycast);
		}
		
		// Assign raster shader to render types
		Geometry::geometryRenderTypes["basic"].rasterShader = &shader_visibility_basic;
		Geometry::geometryRenderTypes["standard"].rasterShader = &shader_visibility_standard;
		Geometry::geometryRenderTypes["terrain"].rasterShader = &shader_visibility_terrain;
		Geometry::geometryRenderTypes["aabb"].rasterShader = &shader_visibility_aabb;
		Geometry::geometryRenderTypes["sphere"].rasterShader = &shader_visibility_sphere;
		Geometry::geometryRenderTypes["light"].rasterShader = &shader_visibility_light;
		Geometry::geometryRenderTypes["sun"].rasterShader = &shader_visibility_sun;
		Geometry::geometryRenderTypes["transparent"].rasterShader = &shader_transparent;
	}
	
	#pragma region Load/Upload Renderer
		
		V4D_MODULE_FUNC(void, ConfigureShaders) {
			ConfigureRasterVisibilityShaders();
			ConfigureTransparentShaders();
			ConfigureRayTracingShaders();
			ConfigureLightingShaders();
			ConfigurePostProcessingShaders();
			ConfigureOverlayShaders();
		}

		V4D_MODULE_FUNC(void, ReadShaders) {
			for (auto&[grpName, shaderList] : shaderGroups) {
				for (auto* shader : shaderList) {
					shader->ReadShaders();
				}
			}
			
			for (auto&[name, sbt] : shaderBindingTables) {
				sbt->ReadShaders();
			}
			
			shader_histogram_compute.ReadShaders();
			shader_raycast_compute.ReadShaders();
		}
		
		V4D_MODULE_FUNC(void, CreateSyncObjects) {
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

		V4D_MODULE_FUNC(void, DestroySyncObjects) {
			for (int i = 0; i < r->NB_FRAMES_IN_FLIGHT; i++) {
				for (auto&[name, s] : semaphores) r->renderingDevice->DestroySemaphore(s[i], nullptr);
				for (auto&[name, f] : fences) r->renderingDevice->DestroyFence(f[i], nullptr);
			}
		}

		V4D_MODULE_FUNC(void, CreateResources) {
			CreateUiResources();
			CreateRenderingResources();
			CreatePostProcessingResources();
			if (r->rayTracingFeatures.rayTracing) CreateRayTracingResources();
			CreateRasterVisibilityResources();
			
			// Textures
			tex_img_font_atlas.SetMipLevels();
			// tex_img_font_atlas.SetSamplerAnisotropy(16);
			tex_img_font_atlas.AllocateAndWriteStagingMemory(r->renderingDevice);
			tex_img_font_atlas.CreateImage(r->renderingDevice);
			auto commandBuffer = r->renderingDevice->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
				tex_img_font_atlas.CopyStagingBufferToImage(r->renderingDevice, commandBuffer);
			r->renderingDevice->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
			tex_img_font_atlas.FreeStagingMemory(r->renderingDevice);

			// Modules
			V4D_Game::ForEachSortedModule([](auto* mod){
				if (mod->RendererCreateResources) mod->RendererCreateResources(r->renderingDevice);
			});
		}
		
		V4D_MODULE_FUNC(void, DestroyResources) {
			DestroyUiResources();
			DestroyRenderingResources();
			DestroyPostProcessingResources();
			if (r->rayTracingFeatures.rayTracing) DestroyRayTracingResources();
			DestroyRasterVisibilityResources();
			
			// Textures
			tex_img_font_atlas.DestroyImage(r->renderingDevice);

			// Modules
			V4D_Game::ForEachSortedModule([](auto* mod){
				if (mod->RendererDestroyResources) mod->RendererDestroyResources(r->renderingDevice);
			});
		}
		
		V4D_MODULE_FUNC(void, AllocateBuffers) {
			// Uniform Buffers
			cameraUniformBuffer.Allocate(r->renderingDevice);
			activeLightsUniformBuffer.Allocate(r->renderingDevice);
			
			// Overlays
			overlayLinesBuffer.Allocate(r->renderingDevice, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			overlayLinesBuffer.MapMemory(r->renderingDevice);
			overlayLines = (OverlayLine*)overlayLinesBuffer.data;
			overlayTextBuffer.Allocate(r->renderingDevice, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			overlayTextBuffer.MapMemory(r->renderingDevice);
			overlayText = (OverlayText*)overlayTextBuffer.data;
			overlayCirclesBuffer.Allocate(r->renderingDevice, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			overlayCirclesBuffer.MapMemory(r->renderingDevice);
			overlayCircles = (OverlayCircle*)overlayCirclesBuffer.data;
			overlaySquaresBuffer.Allocate(r->renderingDevice, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			overlaySquaresBuffer.MapMemory(r->renderingDevice);
			overlaySquares = (OverlaySquare*)overlaySquaresBuffer.data;
			
			AllocateComputeBuffers();
			if (r->rayTracingFeatures.rayTracing) AllocateRayTracingBuffers();
			
			Geometry::globalBuffers.DefragmentMemory();
			Geometry::globalBuffers.Allocate(r->renderingDevice, {r->renderingDevice->GetQueue("compute").familyIndex, r->renderingDevice->GetQueue("graphics").familyIndex});
		}
		
		V4D_MODULE_FUNC(void, FreeBuffers) {
			scene->ClenupObjectInstancesGeometries();
			activeRayTracedGeometries.clear();
			
			// Overlays
			overlayLinesBuffer.UnmapMemory(r->renderingDevice);
			overlayLinesBuffer.Free(r->renderingDevice);
			overlayLines = nullptr;
			overlayTextBuffer.UnmapMemory(r->renderingDevice);
			overlayTextBuffer.Free(r->renderingDevice);
			overlayText = nullptr;
			overlayCirclesBuffer.UnmapMemory(r->renderingDevice);
			overlayCirclesBuffer.Free(r->renderingDevice);
			overlayCircles = nullptr;
			overlaySquaresBuffer.UnmapMemory(r->renderingDevice);
			overlaySquaresBuffer.Free(r->renderingDevice);
			overlaySquares = nullptr;
			
			// Uniform Buffers
			cameraUniformBuffer.Free(r->renderingDevice);
			activeLightsUniformBuffer.Free(r->renderingDevice);
			
			FreeComputeBuffers();
			if (r->rayTracingFeatures.rayTracing) FreeRayTracingBuffers();
			
			Geometry::globalBuffers.Free(r->renderingDevice);
		}

		V4D_MODULE_FUNC(void, CreatePipelines) {
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
		
		V4D_MODULE_FUNC(void, DestroyPipelines) {
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
		
		V4D_MODULE_FUNC(void, CreateCommandBuffers) {
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

		V4D_MODULE_FUNC(void, DestroyCommandBuffers) {
			r->renderingDevice->FreeCommandBuffers(r->renderingDevice->GetQueue("graphics").commandPool, static_cast<uint32_t>(commandBuffers["graphics"].size()), commandBuffers["graphics"].data());
			r->renderingDevice->FreeCommandBuffers(r->renderingDevice->GetQueue("graphics").commandPool, static_cast<uint32_t>(commandBuffers["graphicsDynamic"].size()), commandBuffers["graphicsDynamic"].data());
		}
	
	#pragma endregion
	
	#pragma region Overlay
		
		V4D_MODULE_FUNC(void, DrawOverlayLine, float x1, float y1, float x2, float y2, glm::vec4 color, float lineWidth) {
			AddOverlayLine(x1, y1, x2, y2, color, lineWidth);
		}
		V4D_MODULE_FUNC(void, DrawOverlayText, const char* text, float x, float y, glm::vec4 color, float size) {
			AddOverlayText(std::string(text), x, y, color, uint16_t(size));
		}
		V4D_MODULE_FUNC(void, DrawOverlayCircle, float x, float y, glm::vec4 color, float size, float borderSize) {
			AddOverlayCircle(x, y, color, uint16_t(size), uint8_t(borderSize));
		}
		V4D_MODULE_FUNC(void, DrawOverlaySquare, float x, float y, glm::vec4 color, glm::vec2 size, float borderSize) {
			AddOverlaySquare(x, y, color, size, uint8_t(borderSize));
		}
		
	#pragma endregion

	V4D_MODULE_FUNC(void, Update) {
		
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
		
		// Next Frame In Flight
		size_t nextFrameInFlight = (r->currentFrameInFlight + 1) % r->NB_FRAMES_IN_FLIGHT;
		r->renderingDevice->WaitForFences(1, &fences["graphics"][r->currentFrameInFlight], VK_TRUE, timeout);
		r->renderingDevice->ResetFences(1, &fences["graphics"][nextFrameInFlight]);
		
		// Submit Graphics
		VkResult result = r->renderingDevice->QueueSubmit(r->renderingDevice->GetQueue("graphics").handle, graphicsSubmitInfo.size(), graphicsSubmitInfo.data(), fences["graphics"][nextFrameInFlight]);
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
		r->currentFrameInFlight = nextFrameInFlight;
	
		// //TODO find a better fix...
		r->renderingDevice->QueueWaitIdle(r->renderingDevice->GetQueue("graphics").handle); // Temporary fix for occasional crash with acceleration structures
	}
	
	V4D_MODULE_FUNC(void, Update2) {
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
	
	V4D_MODULE_FUNC(void, RunUi) {
		#ifdef _ENABLE_IMGUI
			ImGui::SetNextWindowSize({340, 150});
			ImGui::Begin("Settings and Modules");
			// #ifdef _DEBUG
				ImGui::Checkbox("Debug Wireframe", &DEBUG_OPTIONS::WIREFRAME);
				ImGui::Checkbox("Debug Physics", &DEBUG_OPTIONS::PHYSICS);
			// #endif
			if (DEBUG_OPTIONS::WIREFRAME) {
				// ...
			} else {
				if (r->rayTracingFeatures.rayTracing) {
					ImGui::Checkbox("Ray-traced visibility", &RENDER_OPTIONS::RAY_TRACED_VISIBILITY);
					ImGui::Checkbox("Ray-traced lighting", &RENDER_OPTIONS::RAY_TRACED_LIGHTING);
				} else {
					ImGui::Text("Ray-Tracing unavailable, using rasterization");
				}
				if (RENDER_OPTIONS::RAY_TRACED_LIGHTING || r->rayTracingFeatures.rayQuery) {
					ImGui::Checkbox("Ray-traced Shadows", &RENDER_OPTIONS::HARD_SHADOWS);
				} else {
					ImGui::Text("Shadows functionality unavailable");
				}
				if (!DEBUG_OPTIONS::WIREFRAME && !DEBUG_OPTIONS::PHYSICS) {
					ImGui::Checkbox("TXAA", &RENDER_OPTIONS::TXAA);
				}
				ImGui::Checkbox("Gamma correction", &RENDER_OPTIONS::GAMMA_CORRECTION);
				ImGui::Checkbox("HDR Tone Mapping", &RENDER_OPTIONS::HDR_TONE_MAPPING);
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
				// RayCast
				if (currentRayCast.hit) ImGui::Text((std::string("RayCast Hit object ") + std::to_string(currentRayCast.objectBufferOffset) + ", data: " + std::to_string(currentRayCast.customData0)).c_str());
				else ImGui::Text("RayCast no hit");
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
	
	V4D_MODULE_FUNC(void, Render2, VkCommandBuffer commandBuffer) {
		// Overlay/UI
		uiRenderPass.Begin(r->renderingDevice, commandBuffer, img_overlay, {{.0,.0,.0,.0}});
			{
				// Adjust lines and text buffers
				std::lock_guard lock(overlayMutex);
				auto tmpCurrentLinesVertexBufferOffset = shader_overlay_lines.vertexOffset;
				shader_overlay_lines.vertexCount = overlayLinesCount;
				shader_overlay_text.vertexCount = overlayTextSize;
				shader_overlay_circles.vertexCount = overlayCirclesCount;
				shader_overlay_squares.vertexCount = overlaySquaresCount;
				
				for (auto* shader : shaderGroups["sg_overlay"]) {
					if (shader == &shader_overlay_lines) {
						if (overlayLinesCount == 0) continue;
						shader->Bind(r->renderingDevice, commandBuffer);
						float lineWidth = 1.0f;
						uint32_t linesRendered = 0;
						do {
							shader_overlay_lines.vertexCount = 0;
							while (shader_overlay_lines.vertexCount < overlayLinesCount && overlayLines[linesRendered + shader_overlay_lines.vertexCount].width == lineWidth) {
								shader_overlay_lines.vertexCount += 2;
							}
							if (shader_overlay_lines.vertexCount > 0) {
								shader_overlay_lines.vertexOffset = sizeof(OverlayLine)*linesRendered + tmpCurrentLinesVertexBufferOffset;
								r->renderingDevice->CmdSetLineWidth(commandBuffer, lineWidth);
								shader->Render(r->renderingDevice, commandBuffer, 1);
								linesRendered += shader_overlay_lines.vertexCount;
							}
							if (linesRendered < overlayLinesCount) {
								lineWidth = overlayLines[linesRendered].width;
							} else {
								break;
							}
						} while (true);
					} else {
						if (shader == &shader_overlay_text && overlayTextSize == 0) continue;
						if (shader == &shader_overlay_circles && overlayCirclesCount == 0) continue;
						if (shader == &shader_overlay_squares && overlaySquaresCount == 0) continue;
						shader->Execute(r->renderingDevice, commandBuffer);
					}
				}
				shader_overlay_lines.vertexOffset = tmpCurrentLinesVertexBufferOffset;
				overlayLinesCount = 0;
				overlayTextSize = 0;
				overlayCirclesCount = 0;
				overlaySquaresCount = 0;
			}
			#ifdef _ENABLE_IMGUI
				DrawImGui(commandBuffer);
			#endif
		uiRenderPass.End(r->renderingDevice, commandBuffer);
	}
	
	V4D_MODULE_FUNC(void, Render, VkCommandBuffer commandBuffer) {
		if (DEBUG_OPTIONS::WIREFRAME) {
			RunRasterVisibility(r->renderingDevice, commandBuffer);
			RunRasterLighting(r->renderingDevice, commandBuffer, true, false);
		} else {
			if (RENDER_OPTIONS::RAY_TRACED_VISIBILITY) {
				RunRayTracingVisibilityCommands(commandBuffer);
			} else {
				RunRasterVisibility(r->renderingDevice, commandBuffer);
			}
			if (RENDER_OPTIONS::RAY_TRACED_LIGHTING) {
				RunRayTracingLightingCommands(commandBuffer);
				// Use Raster Fog Pass (for now)
				RunRasterLighting(r->renderingDevice, commandBuffer, false, true);
			} else {
				RunRasterLighting(r->renderingDevice, commandBuffer, true, true);
			}
		}
	}

};
