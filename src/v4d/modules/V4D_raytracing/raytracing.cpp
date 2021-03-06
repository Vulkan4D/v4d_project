#define V4D_RAYTRACING_RENDERER_MODULE

#include <v4d.h>
#include <V4D_Mod.h>

#include "utilities/io/Logger.h"
#include "utilities/graphics/vulkan/ComputeShaderPipeline.h"
#include "utilities/graphics/vulkan/StagingBuffer.hpp"
#include "utilities/graphics/vulkan/RenderPass.h"

#include "Texture2D.hpp"
#include "camera_options.hh"
#include "substances.hh"

#include "v4d/game/Game.h"
#include "v4d/game/Entity.h"

#include <cmath>
#include <random>

using namespace v4d::scene;
using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::vulkan::rtx;

#pragma region Limits
	const uint32_t MAX_RENDERABLE_ENTITY_INSTANCES = 65536; // up to ~ 256 bytes each
	// const uint32_t COLLISION_MAX_LINES_PER_OBJECT = 255;
	// const uint32_t RAY_TRACING_MAX_COLLISION_PER_FRAME = MAX_RENDERABLE_ENTITY_INSTANCES * COLLISION_MAX_LINES_PER_OBJECT;
#pragma endregion

#pragma region Global objects

// Application
Window* window = nullptr;
Renderer* r = nullptr;
Scene* scene = nullptr;

RayCast previousRayCast {};

// Textures
Texture2D tex_img_font_atlas { V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/monospace_font_atlas.png"), STBI_grey_alpha};

VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties {};
VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties {};
AccelerationStructure topLevelAccelerationStructure {};
Buffer rayTracingRenderingShaderBindingTableBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
Buffer rayTracingCollisionShaderBindingTableBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
std::recursive_mutex rayTracingInstanceMutex, blasBuildQueueMutex;
std::vector<VkAccelerationStructureBuildGeometryInfoKHR> blasQueueBuildGeometryInfos {};
std::vector<VkAccelerationStructureBuildRangeInfoKHR*> blasQueueBuildRangeInfos {};
std::vector<std::shared_ptr<RenderableGeometryEntity>> currentRenderableEntities {};

std::mt19937_64 randomGenerator (std::chrono::system_clock::now().time_since_epoch().count());
std::uniform_real_distribution<double> uniformRandomDouble(-1.0, 1.0);
int nbStochasticRayTracedCollisions = 5;

#pragma endregion

#pragma region Buffers
	StagingBuffer<RenderableGeometryEntity::RenderableEntityInstance, MAX_RENDERABLE_ENTITY_INSTANCES> renderableEntityInstanceBuffer {};
	StagingBuffer<Camera> cameraUniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT};
	StagingBuffer<RenderableGeometryEntity::LightSource, MAX_ACTIVE_LIGHTS> lightSourcesBuffer {};
	StagingBuffer<RayTracingBLASInstance, RAY_TRACING_TLAS_MAX_INSTANCES> rayTracingInstanceBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR};
	uint32_t nbRayTracingInstances = 0;
	Buffer totalLuminance {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(glm::vec4)};
	StagingBuffer<RayCast> raycastBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
	// StagingBuffer<Collision, RAY_TRACING_MAX_COLLISION_PER_FRAME> collisionBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
	// uint32_t collisionLineCount = 0;
#pragma endregion

#pragma region Descriptor Sets
	DescriptorSet set0;
	DescriptorSet set1_raster;
	DescriptorSet set1_rendering;
	// DescriptorSet set1_collision;
	DescriptorSet set1_post;
	DescriptorSet set1_histogram;
	DescriptorSet set1_overlay;
#pragma endregion

#pragma region Images
	Image img_lit { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_depth { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT } };
	Image img_pos { VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT }};
	Image img_geometry { VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT }};
	Image img_albedo { VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_normal { VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	
	Image img_lit_history { VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_depth_history { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT } };
	Image img_pos_history { VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT }};
	Image img_geometry_history { VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT }};
	Image img_albedo_history { VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_normal_history { VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	
	Image img_thumbnail { VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_overlay { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8G8B8A8_UNORM }};
	
	CubeMapImage img_background { };
	
	// Textures
	// Texture2D tex_metal_normal {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/textures/metal_normal.tga"), STBI_rgb_alpha};
	std::array<Texture2D*, 0> textures {
		// &tex_metal_normal,
	};
	
	uint32_t currentSwapChainImageIndex = 0;

#pragma endregion

#pragma region Pipeline Layouts
	PipelineLayout pl_raster;
	PipelineLayout pl_rendering;
	// PipelineLayout pl_collision;
	PipelineLayout pl_overlay;
	PipelineLayout pl_post;
	PipelineLayout pl_histogram;
	PipelineLayout pl_background;
#pragma endregion

#pragma region Shaders

	// Ray Tracing
	ShaderBindingTable sbt_rendering {pl_rendering, V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rendering.rgen")};
	// ShaderBindingTable sbt_collision {pl_collision, V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/collision.rgen")};

	// Transparent/Wireframe
	RasterShaderPipeline shader_transparent {pl_raster, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster.transparent.frag"),
	}};
	RasterShaderPipeline shader_wireframe {pl_raster, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raster.wireframe.frag"),
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
	
	// Post Processing
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
	
#pragma endregion

#pragma region Containers
	std::unordered_map<std::string, Image*> images {
		{"img_lit", &img_lit},
		{"img_depth", &img_depth},
		{"img_pos", &img_pos},
		{"img_geometry", &img_geometry},
		{"img_albedo", &img_albedo},
		{"img_normal", &img_normal},
		
		{"img_lit_history", &img_lit_history},
		{"img_depth_history", &img_depth_history},
		{"img_pos_history", &img_pos_history},
		{"img_geometry_history", &img_geometry_history},
		{"img_albedo_history", &img_albedo_history},
		{"img_normal_history", &img_normal_history},
		
		{"img_thumbnail", &img_thumbnail},
		{"img_overlay", &img_overlay},
		
		{"img_background", &img_background},
	};
	std::unordered_map<std::string, PipelineLayout*> pipelineLayouts {
		{"pl_raster", &pl_raster},
		{"pl_rendering", &pl_rendering},
		// {"pl_collision", &pl_collision},
		{"pl_overlay", &pl_overlay},
		{"pl_post", &pl_post},
		{"pl_histogram", &pl_histogram},
		{"pl_background", &pl_background},
	};
	std::unordered_map<std::string, std::vector<RasterShaderPipeline*>> shaderGroups {
		{"sg_transparent", {&shader_transparent}},
		{"sg_wireframe", {&shader_wireframe}},
		{"sg_present", {&shader_present_hdr, &shader_present_overlay_apply}},
		{"sg_overlay", {&shader_overlay_lines, &shader_overlay_text, &shader_overlay_squares, &shader_overlay_circles}},
		{"sg_background", {}},
	};
	std::unordered_map<std::string, ShaderBindingTable*> shaderBindingTables {
		{"sbt_rendering", &sbt_rendering},
		// {"sbt_collision", &sbt_collision},
	};
	std::unordered_map<std::string, VkSemaphore> semaphores {
		{"push", {}},
		{"graphics", {}},
		// {"physics", {}},
		{"build", {}},
		{"pull1", {}},
		{"pull2", {}},
		{"post1", {}},
		{"post2", {}},
		{"present", {}},
	};
	std::unordered_map<std::string, VkFence> fences {
		{"beforeUpdate", {}},
		{"frameUpdate", {}},
		{"post", {}},
	};
	struct CommandBuffer {
		std::string queue;
		VkCommandBuffer commandBuffer;
		operator VkCommandBuffer&() {return commandBuffer;}
		operator VkCommandBuffer*() {return &commandBuffer;}
		void operator=(VkCommandBuffer value) {commandBuffer = value;}
		CommandBuffer(std::string queueName, const VkCommandBuffer& commandBuffer = VK_NULL_HANDLE) : queue(queueName), commandBuffer(commandBuffer) {}
		CommandBuffer(const VkCommandBuffer& commandBuffer = VK_NULL_HANDLE) : commandBuffer(commandBuffer), queue("") {}
	};
	std::unordered_map<std::string, CommandBuffer> commandBuffers {
		{"Push", {"transfer"}},
		{"Build", {"compute"}},
		{"Graphics", {"graphics"}},
		// {"Physics", {"compute"}},
		{"Pull", {"transfer"}},
		{"Post", {"graphics"}},
	};
#pragma endregion

#pragma region Render Passes (Rasterization)
	RenderPass postProcessingRenderPass;
	RenderPass uiRenderPass;
	RenderPass rasterRenderPass;
	
	struct RasterPushConstant {
		glm::vec4 wireframeColor = {1,1,1,1};
		int32_t instanceCustomIndexValue = 0;
		uint32_t geometryIndex = 0;
	};
	
#pragma endregion

#pragma region Rendering Resources

	float renderScale = 1.0;
	
	void CreateRenderingResources() {
		uint renderWidth = (uint)((float)r->swapChain->extent.width*renderScale);
		uint renderHeight = (uint)((float)r->swapChain->extent.height*renderScale);
		
		img_geometry.samplerInfo.magFilter = VK_FILTER_NEAREST;
		img_geometry.samplerInfo.minFilter = VK_FILTER_NEAREST;
		img_geometry_history.samplerInfo.magFilter = VK_FILTER_NEAREST;
		img_geometry_history.samplerInfo.minFilter = VK_FILTER_NEAREST;
		
		img_lit.Create(r->renderingDevice, renderWidth, renderHeight);
		img_depth.Create(r->renderingDevice, renderWidth, renderHeight);
		img_pos.Create(r->renderingDevice, renderWidth, renderHeight);
		img_geometry.Create(r->renderingDevice, renderWidth, renderHeight);
		img_albedo.Create(r->renderingDevice, renderWidth, renderHeight);
		img_normal.Create(r->renderingDevice, renderWidth, renderHeight);
		
		img_lit_history.Create(r->renderingDevice, renderWidth, renderHeight);
		img_depth_history.Create(r->renderingDevice, renderWidth, renderHeight);
		img_pos_history.Create(r->renderingDevice, renderWidth, renderHeight);
		img_geometry_history.Create(r->renderingDevice, renderWidth, renderHeight);
		img_albedo_history.Create(r->renderingDevice, renderWidth, renderHeight);
		img_normal_history.Create(r->renderingDevice, renderWidth, renderHeight);
		
		img_background.Create(r->renderingDevice, renderWidth, renderWidth);
		
		auto commandBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
		
			r->TransitionImageLayout(commandBuffer, img_lit, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_depth, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_pos, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_geometry, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_albedo, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_normal, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			
			r->TransitionImageLayout(commandBuffer, img_lit_history, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_depth_history, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_pos_history, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_geometry_history, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_albedo_history, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_normal_history, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			
			r->TransitionImageLayout(commandBuffer, img_background, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
	}
	void DestroyRenderingResources() {
		
		img_lit.Destroy(r->renderingDevice);
		img_depth.Destroy(r->renderingDevice);
		img_pos.Destroy(r->renderingDevice);
		img_geometry.Destroy(r->renderingDevice);
		img_albedo.Destroy(r->renderingDevice);
		img_normal.Destroy(r->renderingDevice);
		
		img_lit_history.Destroy(r->renderingDevice);
		img_depth_history.Destroy(r->renderingDevice);
		img_pos_history.Destroy(r->renderingDevice);
		img_geometry_history.Destroy(r->renderingDevice);
		img_albedo_history.Destroy(r->renderingDevice);
		img_normal_history.Destroy(r->renderingDevice);
		
		img_background.Destroy(r->renderingDevice);
		
	}
	
	// void ClearLitImage(VkCommandBuffer commandBuffer) {
	// 	r->TransitionImageLayout(commandBuffer, img_lit, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	// 		const VkClearColorValue clearValues = {0,0,0,0};
	// 		VkImageSubresourceRange range {VK_IMAGE_ASPECT_COLOR_BIT, 0,1,0,1};
	// 		r->renderingDevice->CmdClearColorImage(commandBuffer, img_lit.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValues, 1, &range);
	// 	r->TransitionImageLayout(commandBuffer, img_lit, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	// }
	
	void CreateRasterPipeline() {
		const int nbColorAttachments = 1;
		VkAttachmentDescription attachment {};
		VkAttachmentReference colorAttachmentRef {};
		
		// Color attachment
		attachment.format = img_lit.format;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
		attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		colorAttachmentRef = {
			rasterRenderPass.AddAttachment(attachment),
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};
	
		// VkSubpassDependency subpassDependency = {
		// 	VK_SUBPASS_EXTERNAL,// srcSubpass;
		// 	0,// dstSubpass;
		// 	VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// srcStageMask;
		// 	VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// dstStageMask;
		// 	VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// srcAccessMask;
		// 	VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// dstAccessMask;
		// 	0// dependencyFlags;
		// };
		VkSubpassDescription subpass {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
		rasterRenderPass.AddSubpass(subpass);
		// rasterRenderPass.renderPassInfo.dependencyCount = 1;
		// rasterRenderPass.renderPassInfo.pDependencies = &subpassDependency;
		
		// Create the render pass
		rasterRenderPass.Create(r->renderingDevice);
		rasterRenderPass.CreateFrameBuffers(r->renderingDevice, img_lit);
		
		// Shaders
		for (auto& sg : {shaderGroups["sg_transparent"], shaderGroups["sg_wireframe"]}) {
			for (auto* s : sg) {
				s->SetRenderPass(&img_lit, rasterRenderPass.handle, 0);
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
	void DestroyRasterPipeline() {
		for (auto& sg : {shaderGroups["sg_transparent"], shaderGroups["sg_wireframe"]}) {
			for (auto* s : sg) {
				s->DestroyPipeline(r->renderingDevice);
			}
		}
		rasterRenderPass.DestroyFrameBuffers(r->renderingDevice);
		rasterRenderPass.Destroy(r->renderingDevice);
	}
	
#pragma endregion

#pragma region Ray-Tracing Pipelines & Resouces

	void CreateRayTracingResources() {
		topLevelAccelerationStructure.AssignTopLevel();
		topLevelAccelerationStructure.CreateAndAllocate(r->renderingDevice, true);
		topLevelAccelerationStructure.SetInstanceBuffer(r->renderingDevice, rayTracingInstanceBuffer.GetDeviceLocalBuffer());
	}
	void DestroyRayTracingResources() {
		topLevelAccelerationStructure.FreeAndDestroy(r->renderingDevice);
	}
	
	void AllocateRayTracingBuffers() {
		rayTracingInstanceBuffer.Allocate(r->renderingDevice);
	}
	void FreeRayTracingBuffers() {
		nbRayTracingInstances = 0;
		rayTracingInstanceBuffer.Free();
	}
	
	void CreateRayTracingPipeline() {
		// Rendering
		sbt_rendering.CreateRayTracingPipeline(r->renderingDevice);
		rayTracingRenderingShaderBindingTableBuffer.size = sbt_rendering.GetSbtBufferSize(rayTracingPipelineProperties);
		rayTracingRenderingShaderBindingTableBuffer.Allocate(r->renderingDevice, MEMORY_USAGE_CPU_TO_GPU);
		sbt_rendering.WriteShaderBindingTableToBuffer(r->renderingDevice, &rayTracingRenderingShaderBindingTableBuffer, 0, rayTracingPipelineProperties);
		
		// // Collision
		// sbt_collision.CreateRayTracingPipeline(r->renderingDevice);
		// rayTracingCollisionShaderBindingTableBuffer.size = sbt_collision.GetSbtBufferSize(rayTracingPipelineProperties);
		// rayTracingCollisionShaderBindingTableBuffer.Allocate(r->renderingDevice, MEMORY_USAGE_CPU_TO_GPU);
		// sbt_collision.WriteShaderBindingTableToBuffer(r->renderingDevice, &rayTracingCollisionShaderBindingTableBuffer, 0, rayTracingPipelineProperties);
	}
	void DestroyRayTracingPipeline() {
		// Rendering
		rayTracingRenderingShaderBindingTableBuffer.Free(r->renderingDevice);
		sbt_rendering.DestroyRayTracingPipeline(r->renderingDevice);
		
		// // Collision
		// rayTracingCollisionShaderBindingTableBuffer.Free(r->renderingDevice);
		// sbt_collision.DestroyRayTracingPipeline(r->renderingDevice);
	}
	
	void AddRayTracingShader (v4d::modular::ModuleID mod, const std::string& name) {
		std::string path = V4D_MODULE_ASSET_PATH_STR(mod.String(), "shaders/" + name);
		
		std::string rint { path + ".rint" };
		
		std::string rendering_rchit { path + ".visibility.rchit" };
		std::string rendering_rahit { path + ".visibility.rahit" };
		std::string rendering_rint { path + ".visibility.rint" };
		
		std::string spectral_rchit { path + ".spectral.rchit" };
		std::string spectral_rahit { path + ".spectral.rahit" };
		std::string spectral_rint { path + ".spectral.rint" };
		
		std::string extra_rchit { path + ".extra.rchit" };
		std::string extra_rahit { path + ".extra.rahit" };
		std::string extra_rint { path + ".extra.rint" };
		
		// std::string collision_rchit { path + ".collision.rchit" };
		// std::string collision_rahit { path + ".collision.rahit" };
		// std::string collision_rint { path + ".collision.rint" };
		
		if (!v4d::io::FilePath::FileExists(rint)) {
			rint = "";
		}
		
		if (!v4d::io::FilePath::FileExists(rendering_rchit)) {
			rendering_rchit = V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/default.visibility.rchit");
		}
		if (!v4d::io::FilePath::FileExists(rendering_rahit)) {
			rendering_rahit = "";// V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/default.visibility.rahit");
		}
		if (!v4d::io::FilePath::FileExists(rendering_rint)) {
			rendering_rint = rint;
		}
		
		if (!v4d::io::FilePath::FileExists(spectral_rchit)) {
			spectral_rchit = V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/default.spectral.rchit");
		}
		if (!v4d::io::FilePath::FileExists(spectral_rahit)) {
			spectral_rahit = "";
		}
		if (!v4d::io::FilePath::FileExists(spectral_rint)) {
			spectral_rint = rint;
		}
		
		if (!v4d::io::FilePath::FileExists(extra_rchit)) {
			extra_rchit = "";
		}
		if (!v4d::io::FilePath::FileExists(extra_rahit)) {
			extra_rahit = "";
		}
		if (!v4d::io::FilePath::FileExists(extra_rint)) {
			extra_rint = rint;
		}
		
		// if (!v4d::io::FilePath::FileExists(collision_rchit)) {
		// 	collision_rchit = V4D_MODULE_ASSET_PATH_STR(THIS_MODULE, "shaders/collision.rchit");
		// }
		// if (!v4d::io::FilePath::FileExists(collision_rahit)) {
		// 	collision_rahit = V4D_MODULE_ASSET_PATH_STR(THIS_MODULE, "shaders/collision.rahit");
		// }
		// if (!v4d::io::FilePath::FileExists(collision_rint)) {
		// 	collision_rint = rint;
		// }
		
		// Rendering
		Renderer::sbtOffsets[std::string("rendering:hit:") + mod.String() + ":" + name] = 
		/* offset / payload location 0 */ sbt_rendering.AddHitShader(rendering_rchit, rendering_rahit, rendering_rint);
		/* offset / payload location 1 */ sbt_rendering.AddHitShader(spectral_rchit, spectral_rahit, spectral_rint);
		/* offset / payload location 2 */ sbt_rendering.AddHitShader(extra_rchit, extra_rahit, extra_rint);
		
		// // Collision
		// Renderer::sbtOffsets[std::string("collision:hit:") + mod.String() + ":" + name] = 
		// 								sbt_collision.AddHitShader(collision_rchit, collision_rahit, collision_rint);
		// 								sbt_collision.AddHitShader();
		// 								sbt_collision.AddHitShader();
		
	}

	void ConfigureRayTracingShaders() {
		{// Ray Miss shaders
			sbt_rendering.AddMissShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rendering.rmiss"));
			sbt_rendering.AddMissShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/rendering.void.rmiss"));
			
			// sbt_collision.AddMissShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/collision.rmiss"));
		}
		
		{// Ray Hit default shaders
			AddRayTracingShader(THIS_MODULE, "default");
			AddRayTracingShader(THIS_MODULE, "aabb_cube");
			AddRayTracingShader(THIS_MODULE, "aabb_sphere");
			AddRayTracingShader(THIS_MODULE, "aabb_sphere.light");
			AddRayTracingShader(THIS_MODULE, "aabb_capsule");
			AddRayTracingShader(THIS_MODULE, "aabb_cylinder");
			AddRayTracingShader(THIS_MODULE, "aabb_cone");
			AddRayTracingShader(THIS_MODULE, "aabb_ring");
			AddRayTracingShader(THIS_MODULE, "aabb_pipe");
		}
		
		{// default callables
			Renderer::sbtOffsets["call:material_default"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/material_default.rcall"));
			assert(Renderer::sbtOffsets["call:material_default"] == v4d::graphics::RenderableGeometryEntity::Material::DEFAULT_VISIBILITY_MATERIAL_RCALL);
			
			// Renderer::sbtOffsets["call:spectral_default.emit"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/spectral_default.emit.rcall"));
			// assert(Renderer::sbtOffsets["call:spectral_default.emit"] == v4d::graphics::RenderableGeometryEntity::Material::DEFAULT_SPECTRAL_EMIT_RCALL);
			
			// Renderer::sbtOffsets["call:spectral_default.absorb"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/spectral_default.absorb.rcall"));
			// assert(Renderer::sbtOffsets["call:spectral_default.absorb"] == v4d::graphics::RenderableGeometryEntity::Material::DEFAULT_SPECTRAL_ABSORB_RCALL);
			
			// Renderer::sbtOffsets["call:collider_default"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/collider_default.rcall"));
			// assert(Renderer::sbtOffsets["call:collider_default"] == v4d::graphics::RenderableGeometryEntity::Material::DEFAULT_COLLIDER_RCALL);
			
			// Renderer::sbtOffsets["call:physics_default"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/physics_default.rcall"));
			// assert(Renderer::sbtOffsets["call:physics_default"] == v4d::graphics::RenderableGeometryEntity::Material::DEFAULT_PHYSICS_RCALL);
			
			// Renderer::sbtOffsets["call:sound_default.prop"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/sound_default.prop.rcall"));
			// assert(Renderer::sbtOffsets["call:sound_default.prop"] == v4d::graphics::RenderableGeometryEntity::Material::DEFAULT_SOUND_PROP_RCALL);
			
			// Renderer::sbtOffsets["call:sound_default.gen"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/sound_default.gen.rcall"));
			// assert(Renderer::sbtOffsets["call:sound_default.gen"] == v4d::graphics::RenderableGeometryEntity::Material::DEFAULT_SOUND_GEN_RCALL);
		}
		
		{// texture callables for default materials (max 256)
			Renderer::sbtOffsets["call:tex_rough_normal"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/texture_rough_normal.rcall"));
			Renderer::sbtOffsets["call:tex_checker"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/texture_checker.rcall"));
			Renderer::sbtOffsets["call:tex_noisy"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_noisy.rcall"));
			Renderer::sbtOffsets["call:tex_grainy"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_grainy.rcall"));
			Renderer::sbtOffsets["call:tex_bumped"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_bumped.rcall"));
			Renderer::sbtOffsets["call:tex_brushed"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_brushed.rcall"));
			Renderer::sbtOffsets["call:tex_hammered"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_hammered.rcall"));
			Renderer::sbtOffsets["call:tex_polished"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_polished.rcall"));
			Renderer::sbtOffsets["call:tex_galvanized"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_galvanized.rcall"));
			Renderer::sbtOffsets["call:tex_perforated"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_perforated.rcall"));
			Renderer::sbtOffsets["call:tex_diamond"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_diamond.rcall"));
			Renderer::sbtOffsets["call:tex_carbon_fiber"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_carbon_fiber.rcall"));
			Renderer::sbtOffsets["call:tex_ceramic"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_ceramic.rcall"));
			Renderer::sbtOffsets["call:tex_oxydation_iron"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_oxydation_iron.rcall"));
			Renderer::sbtOffsets["call:tex_oxydation_copper"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_oxydation_copper.rcall"));
			Renderer::sbtOffsets["call:tex_oxydation_aluminum"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_oxydation_aluminum.rcall"));
			Renderer::sbtOffsets["call:tex_oxydation_silver"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_oxydation_silver.rcall"));
			Renderer::sbtOffsets["call:tex_scratches_metal"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_scratches_metal.rcall"));
			Renderer::sbtOffsets["call:tex_scratches_plastic"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_scratches_plastic.rcall"));
			Renderer::sbtOffsets["call:tex_scratches_glass"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_scratches_glass.rcall"));
			Renderer::sbtOffsets["call:tex_cracked_rubber"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_cracked_rubber.rcall"));
			Renderer::sbtOffsets["call:tex_cracked_ceramic"] = sbt_rendering.AddCallableShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/textures.tex_cracked_ceramic.rcall"));
		}
	}
	
	void BuildBottomLevelRayTracingAccelerationStructures(Device* device, VkCommandBuffer commandBuffer) {
		// Build all new/updated bottom levels
		std::lock_guard lock(blasBuildQueueMutex);
		if (blasQueueBuildGeometryInfos.size() > 0) {
			device->CmdBuildAccelerationStructuresKHR(commandBuffer, blasQueueBuildGeometryInfos.size(), blasQueueBuildGeometryInfos.data(), blasQueueBuildRangeInfos.data());
		}
	}
	
	void BuildTopLevelRayTracingAccelerationStructure(Device* device, VkCommandBuffer commandBuffer) {
		if (!device->TouchAllocation(topLevelAccelerationStructure.accelerationStructureAllocation)) {
			LOG_DEBUG("Top Level acceleration structure ALLOCATION LOST")
			return;
		}
		std::lock_guard lock(rayTracingInstanceMutex);
		topLevelAccelerationStructure.SetInstanceCount(nbRayTracingInstances);
		const VkAccelerationStructureBuildRangeInfoKHR* const topLevelAccelerationStructureGeometriesOffsets = topLevelAccelerationStructure.buildRangeInfo.data();
		device->CmdBuildAccelerationStructuresKHR(commandBuffer, 1, &topLevelAccelerationStructure.buildGeometryInfo, &topLevelAccelerationStructureGeometriesOffsets);
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
		
		img_thumbnail.Create(r->renderingDevice, thumbnailWidth, thumbnailHeight, {img_lit.format});
		
		auto commandBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
			r->TransitionImageLayout(commandBuffer, img_thumbnail, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
	}
	void DestroyPostProcessingResources() {
		img_thumbnail.Destroy(r->renderingDevice);
	}
	
	void CreatePostProcessingPipeline() {
		{// Post Processing render pass
			std::array<VkAttachmentDescription, 1> attachments {};
			// std::array<VkAttachmentReference, 0> inputAttachmentRefs {};
			
			// SwapChain image
			attachments[0].format = r->swapChain->format.format;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			
			// SubPasses
			std::array<VkAttachmentReference, 1> colorAttachmentRefs {
				VkAttachmentReference{ postProcessingRenderPass.AddAttachment(attachments[0]), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
			};
			{
				VkSubpassDescription subpass {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = colorAttachmentRefs.size();
				subpass.pColorAttachments = colorAttachmentRefs.data();
				postProcessingRenderPass.AddSubpass(subpass);
			}
			
			std::array<VkSubpassDependency, 1> subPassDependencies {
				VkSubpassDependency{
					VK_SUBPASS_EXTERNAL,// srcSubpass;
					0,// dstSubpass;
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
			postProcessingRenderPass.CreateFrameBuffers(r->renderingDevice, r->swapChain);
			
			// Shaders
			for (auto* shader : shaderGroups["sg_present"]) {
				shader->SetRenderPass(r->swapChain, postProcessingRenderPass.handle);
				shader->AddColorBlendAttachmentState();
				shader->CreatePipeline(r->renderingDevice);
			}
		}
		
		// Compute
		shader_histogram_compute.CreatePipeline(r->renderingDevice);
	}
	void DestroyPostProcessingPipeline() {
		// Present
		for (auto* s : shaderGroups["sg_present"]) {
			s->DestroyPipeline(r->renderingDevice);
		}
		postProcessingRenderPass.DestroyFrameBuffers(r->renderingDevice);
		postProcessingRenderPass.Destroy(r->renderingDevice);
		
		// Compute
		shader_histogram_compute.DestroyPipeline(r->renderingDevice);
	}
	
	void ConfigurePostProcessingShaders() {
		// Present
		for (auto* s : shaderGroups["sg_present"]) {
			s->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			s->depthStencilState.depthTestEnable = VK_FALSE;
			s->depthStencilState.depthWriteEnable = VK_FALSE;
			s->rasterizer.cullMode = VK_CULL_MODE_NONE;
			s->SetData(3);
		}
	}
	
	void AllocateComputeBuffers() {
		// histogram
		totalLuminance.Allocate(r->renderingDevice, MEMORY_USAGE_GPU_TO_CPU, false);
		totalLuminance.MapMemory(r->renderingDevice);
		glm::vec4 luminance {0,0,0,1};
		totalLuminance.WriteToMappedData(&luminance);
		// raycast
		raycastBuffer.Allocate(r->renderingDevice);
		previousRayCast = {};
		raycastBuffer.Init(previousRayCast);
		r->renderingDevice->RunSingleTimeCommands(r->renderingDevice->GetQueue("transfer"), [](VkCommandBuffer cmdBuffer){
			raycastBuffer.Push(cmdBuffer);
		});
		// // Collisions
		// collisionBuffer.Allocate(r->renderingDevice);
	}
	void FreeComputeBuffers() {
		// histogram
		totalLuminance.UnmapMemory(r->renderingDevice);
		totalLuminance.Free(r->renderingDevice);
		// raycast
		previousRayCast = {};
		raycastBuffer.Free();
		// // Collisions
		// collisionBuffer.Free();
	}
	
#pragma endregion

void ConfigureRasterShaders() {
	pl_raster.AddPushConstant<RasterPushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	
	for (auto* s : shaderGroups["sg_transparent"]) {
		// s->AddVertexInputBinding(sizeof(v4d::graphics::Mesh::VertexPosition), VK_VERTEX_INPUT_RATE_VERTEX, v4d::graphics::Mesh::VertexPosition::GetInputAttributes());
		s->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		s->rasterizer.cullMode = VK_CULL_MODE_NONE;
		s->depthStencilState.depthTestEnable = VK_FALSE;
		s->depthStencilState.depthWriteEnable = VK_FALSE;
	}
	
	for (auto* s : shaderGroups["sg_wireframe"]) {
		// s->AddVertexInputBinding(sizeof(v4d::graphics::Mesh::VertexPosition), VK_VERTEX_INPUT_RATE_VERTEX, v4d::graphics::Mesh::VertexPosition::GetInputAttributes());
		s->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		s->rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
		s->rasterizer.cullMode = VK_CULL_MODE_NONE;
		s->depthStencilState.depthTestEnable = VK_FALSE;
		s->depthStencilState.depthWriteEnable = VK_FALSE;
		s->dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
	}
}

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
	void AddOverlayLine(glm::dvec3 viewSpacePosition1, glm::dvec3 viewSpacePosition2, glm::vec4 color = {1,1,1,1}, float lineWidth = 1) {
		glm::vec4 clipSpace1 = scene->camera.projectionMatrix * glm::dvec4(viewSpacePosition1, 1);
		glm::vec4 clipSpace2 = scene->camera.projectionMatrix * glm::dvec4(viewSpacePosition2, 1);
		clipSpace1 /= clipSpace1.w;
		clipSpace2 /= clipSpace2.w;
		if (clipSpace1.z >= 0 && clipSpace2.z >= 0) {
			AddOverlayLine(clipSpace1.x, clipSpace1.y, clipSpace2.x, clipSpace2.y, color, lineWidth);
		}
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
		const float letterWidth = (float(size)*fontSizeRatio+letterSpacing) / (float)r->swapChain->extent.width * 2;
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
		
		// Text
		shader_overlay_text.AddVertexInputBinding(16, VK_VERTEX_INPUT_RATE_VERTEX, {{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT}});
		shader_overlay_text.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		shader_overlay_text.rasterizer.cullMode = VK_CULL_MODE_NONE;
		shader_overlay_text.depthStencilState.depthTestEnable = false;
		shader_overlay_text.depthStencilState.depthWriteEnable = false;
		
		// Circles
		shader_overlay_circles.AddVertexInputBinding(16, VK_VERTEX_INPUT_RATE_VERTEX, {{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT}});
		shader_overlay_circles.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		shader_overlay_circles.rasterizer.cullMode = VK_CULL_MODE_NONE;
		shader_overlay_circles.depthStencilState.depthTestEnable = false;
		shader_overlay_circles.depthStencilState.depthWriteEnable = false;
		
		// Squares
		shader_overlay_squares.AddVertexInputBinding(16, VK_VERTEX_INPUT_RATE_VERTEX, {{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT}});
		shader_overlay_squares.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		shader_overlay_squares.rasterizer.cullMode = VK_CULL_MODE_NONE;
		shader_overlay_squares.depthStencilState.depthTestEnable = false;
		shader_overlay_squares.depthStencilState.depthWriteEnable = false;
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

#pragma region Frame Update & Graphics commands

	void BeforeUpdate() {
		
		{// Reset camera information
			scene->camera.width = r->swapChain->extent.width;
			scene->camera.height = r->swapChain->extent.height;
			scene->camera.RefreshProjectionMatrix();
			scene->camera.time = float(v4d::Timer::GetCurrentTimestamp() - 1587838909.0);
			scene->camera.frameCount++;
			scene->camera.renderOptions = RENDER_OPTIONS::Get();
			scene->camera.debugOptions = DEBUG_OPTIONS::Get();
			if (scene->camera.accumulateFrames != -1) {
				scene->camera.accumulateFrames++;
			}
		}
		
		// RunTXAA();
		
		// Modules
		V4D_Mod::ForEachSortedModule([](auto* mod){
			if (mod->RenderFrame_BeforeUpdate) mod->RenderFrame_BeforeUpdate(); // camera's View Matrix is created here in FlyCam Module
		});
		
		// Generate Renderable Entities
		RenderableGeometryEntity::ForEach([](auto entity){
			if (!entity->generated) {
				// Generate/Load
				if (entity->Generate(r->renderingDevice)) {
					if (entity->sbtOffset == 0) {
						if (auto geometriesData = entity->meshGeometries.Lock(); geometriesData && geometriesData->data) {
							int i = 0;
							for (RenderableGeometryEntity::Geometry& geom : entity->sharedGeometryData->geometries) if (geom.materialName != "") {
								// Override material
								std::stringstream materialStr {std::string(geom.materialName)};
								std::string mat, variant;
								materialStr >> mat;
								materialStr >> variant;
								try {
									auto& substance = Substance::substances.at(mat);
									if (glm::vec3(substance.color) != glm::vec3{1,1,1}) {
										geom.material.visibility.baseColor = substance.color*255.0f;
									}
									geom.material.visibility.metallic = substance.metallic*255.0;
									geom.material.visibility.roughness = substance.roughness*255.0;
									geom.material.visibility.indexOfRefraction = substance.IOR*50;
									if (variant != "" && Renderer::sbtOffsets.count(std::string("call:tex_")+std::string(variant))) {
										geom.material.visibility.textures[0] = Renderer::sbtOffsets[std::string("call:tex_")+std::string(variant)];
										geom.material.visibility.texFactors[0] = 255;
									} else if (substance.baseMap != "") {
										geom.material.visibility.textures[0] = Renderer::sbtOffsets[std::string("call:")+std::string(substance.baseMap)];
										geom.material.visibility.texFactors[0] = 255;
										if (variant != "") {
											LOG_WARN("Unknown variant '" << variant << "'")
										}
									}
									if (substance.agingMap != "" && substance.agingFactor > 0) {
										geom.material.visibility.textures[1] = Renderer::sbtOffsets[std::string("call:")+std::string(substance.agingMap)];
										geom.material.visibility.texFactors[1] = (uint8_t)(substance.agingFactor*255.0);
									}
									if (substance.oxydationMap != "" && substance.oxydationFactor > 0) {
										geom.material.visibility.textures[2] = Renderer::sbtOffsets[std::string("call:")+std::string(substance.oxydationMap)];
										geom.material.visibility.texFactors[2] = (uint8_t)(substance.oxydationFactor*255.0);
									}
									if (substance.wearAndTearMap != "" && substance.wearAndTearFactor > 0) {
										geom.material.visibility.textures[3] = Renderer::sbtOffsets[std::string("call:")+std::string(substance.wearAndTearMap)];
										geom.material.visibility.texFactors[3] = (uint8_t)(substance.wearAndTearFactor*255.0);
									}
									if (substance.burnMap != "" && substance.burnFactor > 0) {
										geom.material.visibility.textures[4] = Renderer::sbtOffsets[std::string("call:")+std::string(substance.burnMap)];
										geom.material.visibility.texFactors[4] = (uint8_t)(substance.burnFactor*255.0);
									}
								} catch(...) {
									LOG_WARN("Unknown material '" << mat << "'")
									continue;
								}
								geometriesData->data[i++].material = geom.material;
							}
						}
					}
				}
			}
		});
		
	}
	
	void FrameUpdate() {
		
		{// Handle RayCast (captured during the previous frame)
			if (previousRayCast && previousRayCast != *raycastBuffer) {
				auto mod = V4D_Mod::GetModule(v4d::modular::ModuleID(previousRayCast.moduleVen, previousRayCast.moduleId));
				if (mod->OnRendererRayCastOut) mod->OnRendererRayCastOut(previousRayCast);
			}
			previousRayCast = raycastBuffer;
			if (previousRayCast) {
				auto mod = V4D_Mod::GetModule(v4d::modular::ModuleID(previousRayCast.moduleVen, previousRayCast.moduleId));
				if (mod->OnRendererRayCastHit) mod->OnRendererRayCastHit(previousRayCast);
			}
		}
		
		// {// Catch Collisions (captured during the previous frame)
		// 	if (collisionLineCount > 0) {
		// 		for (size_t i = 0; i < collisionLineCount; ++i) {
		// 			auto& collision = collisionBuffer[i];
		// 			auto entityA = RenderableGeometryEntity::Get(collision.collisionInstance >> 8);
		// 			auto entityB = RenderableGeometryEntity::Get(collision.objectInstanceB >> 8);
		// 			if (entityA && entityB && collision.objectGeometryA != -1 && collision.objectGeometryB != -1) {
		// 				if (auto physicsA = entityA->physics.Lock(); physicsA) {
		// 					//TODO catch collision events here
		// 					physicsA->collisionTest.collisions.push_back(collision);
		// 				}
		// 			}
		// 		}
		// 	}
		// 	collisionLineCount = 0;
		// }
		
		// Cleanup/Reset stuff
		RenderableGeometryEntity::CleanupOnThisThread();
		currentRenderableEntities.clear();
		int nbActiveLights = 0;
		std::scoped_lock lock(blasBuildQueueMutex, rayTracingInstanceMutex);
		blasQueueBuildGeometryInfos.clear();
		blasQueueBuildRangeInfos.clear();
		nbRayTracingInstances = 0;
		
		// RenderFrame_Update
		V4D_Mod::ForEachSortedModule([](auto* mod){
			if (mod->RenderFrame_Update) mod->RenderFrame_Update();
		}, "render");
		
		// Entities
		RenderableGeometryEntity::ForEach([&nbActiveLights](auto entity){
			if (entity->generated) {
				if (entity->sharedGeometryData && !entity->sharedGeometryData->blas.built && (entity->sharedGeometryData->isRayTracedTriangles || entity->sharedGeometryData->isRayTracedProceduralAABB)) {
					// First BLAS build
					if (entity->sharedGeometryData->isRayTracedTriangles) {
						entity->sharedGeometryData->blas.AssignBottomLevelGeometry(r->renderingDevice, entity->sharedGeometryData->geometriesAccelerationStructureInfo);
					} else {
						entity->sharedGeometryData->blas.AssignBottomLevelProceduralVertex(r->renderingDevice, entity->sharedGeometryData->geometriesAccelerationStructureInfo);
					}
					entity->sharedGeometryData->blas.CreateAndAllocate(r->renderingDevice);
					blasQueueBuildGeometryInfos.push_back(entity->sharedGeometryData->blas.buildGeometryInfo);
					blasQueueBuildRangeInfos.push_back(entity->sharedGeometryData->blas.buildRangeInfo.data());
					entity->sharedGeometryData->blas.built = true;
					entity->entityInstanceInfo.modelViewTransform = glm::mat4(0); // this is because we want to assign the history matrix to it and we need the reprojection to be invalid initially
				}
			
				// add BLAS instance to TLAS
				if (nbRayTracingInstances < RAY_TRACING_TLAS_MAX_INSTANCES) {
					glm::dmat4 worldTransform = entity->GetWorldTransform();
					
					/*
					// Physics/Collisions
					if (auto physics = entity->physics.Lock(); physics && physics->mass > 0.0 && physics->rigidbodyType == v4d::scene::PhysicsInfo::RigidBodyType::DYNAMIC) {
						
						{// Apply collision/transform from previous frame's collision test
							if (physics->collisionTest.collisions.size() > 0) {
								// Collision!!!
								
								// Get avg hit as collision point
								int collisionCount = 0;
								glm::dvec3 hitNormal {0};
								glm::dvec3 offset {0};
								for (auto& collision : physics->collisionTest.collisions) {
									hitNormal += glm::normalize(glm::transpose(glm::dmat3(scene->camera.historyViewMatrix)) * glm::normalize(glm::dvec3(collision.normalB)));
									offset += glm::normalize(glm::transpose(glm::dmat3(scene->camera.historyViewMatrix)) * glm::normalize(glm::dvec3(collision.direction))) * double(collision.normalB.w);
									++collisionCount;
								}
								hitNormal = glm::normalize(hitNormal/double(collisionCount));
								offset /= double(collisionCount);
								
								// Bounce & Drag
								do {physics->linearVelocity = glm::reflect(physics->linearVelocity, hitNormal);}
								while (glm::dot(glm::normalize(physics->linearVelocity), hitNormal) < 0);
								physics->linearVelocity *= 0.6; // restitution
								
								// Offset position to collision point
								if (physics->collisionTest.worldTransformAfter.has_value()) {
									worldTransform = glm::translate(glm::dmat4(1), offset) * physics->collisionTest.worldTransformAfter.value();
									entity->SetWorldTransform(worldTransform);
								}
								
							} else {
								// Collision tested, No collision occured
								if (physics->collisionTest.worldTransformAfter.has_value()) {
									worldTransform = physics->collisionTest.worldTransformAfter.value();
									entity->SetWorldTransform(worldTransform);
								}
							}
						}
						
						{// Solve constraints
							//... may reset position and velocity here
						}
						
						{// Apply Gravity
							physics->linearVelocity += scene->gravityVector * r->deltaTime;
						}
						
						{// Apply Forces
							if (physics->addedForce || physics->physicsForceImpulses.size() > 0) {
								if (physics->physicsForceImpulses.size() > 0) {
									auto&[impulseDir, atPoint] = physics->physicsForceImpulses.front();
									// if (atPoint.x == 0 && atPoint.y == 0 && atPoint.z == 0) {
										// rb->applyCentralImpulse(btVector3(impulseDir.x, impulseDir.y, impulseDir.z));
										physics->linearVelocity += impulseDir / double(physics->mass);
									// } else {
									// 	rb->applyImpulse(btVector3(impulseDir.x, impulseDir.y, impulseDir.z), btVector3(atPoint.x, atPoint.y, atPoint.z));
									// }
									physics->physicsForceImpulses.pop();
								}
							}
						}
						
						{// Prepare ray-traced collision detection
							physics->collisionTest = {worldTransform, physics->linearVelocity, r->deltaTime};
							
							glm::dvec3 velocityDir = glm::normalize(glm::inverse(glm::transpose(glm::mat3(scene->camera.viewMatrix))) * glm::normalize(physics->linearVelocity));
							glm::dvec3 gravityDir = glm::normalize(glm::inverse(glm::transpose(glm::mat3(scene->camera.viewMatrix))) * glm::normalize(scene->gravityVector));
							
							// Test collision towards velocity (Continuous collisions detection)
							if (collisionLineCount < RAY_TRACING_MAX_COLLISION_PER_FRAME) {
								glm::dvec3 direction = velocityDir;
								glm::dvec3 position = (scene->camera.viewMatrix * worldTransform)[3];
								double speed = glm::length(physics->linearVelocity);
								collisionBuffer[collisionLineCount++] = Collision {
									(uint32_t)entity->GetIndex(),
									0, // Collision Flags
									RAY_TRACED_ENTITY_DEFAULT|RAY_TRACED_ENTITY_TERRAIN,
									{position.x, position.y, position.z, physics->boundingDistance + speed * r->deltaTime}, 
									{direction.x, direction.y, direction.z, 0}, 
								};
								if (DEBUG_OPTIONS::PHYSICS) {
									AddOverlayLine(position, position + direction * (speed + physics->boundingDistance), {0, 1, 0, 1}, 4);
								}
							}
							
							// Test collision downwards (using gravity vector)
							if (collisionLineCount < RAY_TRACING_MAX_COLLISION_PER_FRAME) {
								glm::dvec3 direction = gravityDir;
								glm::dvec3 position = (scene->camera.viewMatrix * worldTransform)[3];
								collisionBuffer[collisionLineCount++] = Collision {
									(uint32_t)entity->GetIndex(),
									0, // Collision Options
									RAY_TRACED_ENTITY_DEFAULT|RAY_TRACED_ENTITY_TERRAIN,
									{position.x, position.y, position.z, physics->boundingDistance}, 
									{direction.x, direction.y, direction.z, 0}, 
								};
								if (DEBUG_OPTIONS::PHYSICS) {
									AddOverlayLine(position, position + direction * double(physics->boundingDistance), {0, 0, 1, 1}, 1);
								}
							}
							
							// Test if under terrain (launch a ray upwards to find terrain)
							if (collisionLineCount < RAY_TRACING_MAX_COLLISION_PER_FRAME) {
								glm::dvec3 direction = -gravityDir;
								glm::dvec3 position = glm::dvec3((scene->camera.viewMatrix * worldTransform)[3]) - direction * double(physics->boundingDistance);
								collisionBuffer[collisionLineCount++] = Collision {
									(uint32_t)entity->GetIndex(),
									0, // Collision Flags
									RAY_TRACED_ENTITY_TERRAIN,
									{position.x, position.y, position.z, 0}, 
									{direction.x, direction.y, direction.z, 10000}, 
								};
								if (DEBUG_OPTIONS::PHYSICS) {
									AddOverlayLine(position, position + direction*double(physics->boundingDistance)*4.0, {1, 0, 0, 1}, 2);
								}
							}
							
							// Test collision stochastically by throwing rays randomly within the collider
							for (int i = 0; i < nbStochasticRayTracedCollisions; ++i) {
								if (collisionLineCount < RAY_TRACING_MAX_COLLISION_PER_FRAME) {
									glm::dvec3 direction = glm::normalize(glm::dvec3(uniformRandomDouble(randomGenerator),uniformRandomDouble(randomGenerator),uniformRandomDouble(randomGenerator)));
									glm::dvec3 position = (scene->camera.viewMatrix * worldTransform)[3];
									// if (glm::dot(direction, velocityDir) < 0) direction *= -1; //TODO uncomment this when collisions are correctly applied to both rigidbodies
									collisionBuffer[collisionLineCount++] = Collision {
										(uint32_t)entity->GetIndex(),
										0, // Collision Options
										RAY_TRACED_ENTITY_DEFAULT|RAY_TRACED_ENTITY_TERRAIN,
										{position.x, position.y, position.z, physics->boundingDistance}, 
										{direction.x, direction.y, direction.z, 0}, 
									};
									if (DEBUG_OPTIONS::PHYSICS) {
										AddOverlayLine(position, position + direction * double(physics->boundingDistance), {0, 0, 1, 1}, 1);
									}
								}
							}
							
						}
						
					}
					*/
					
					// Update and Assign transform
					entity->entityInstanceInfo.modelViewTransform_history = entity->entityInstanceInfo.modelViewTransform;
					entity->entityInstanceInfo.modelViewTransform = scene->camera.viewMatrix * worldTransform;
					
					if (entity->sharedGeometryData && entity->sharedGeometryData->blas.built) {
						int index = nbRayTracingInstances++;
						rayTracingInstanceBuffer[index].instanceCustomIndex = entity->GetIndex();
						rayTracingInstanceBuffer[index].accelerationStructureReference = entity->sharedGeometryData->blas.deviceAddress;
						rayTracingInstanceBuffer[index].instanceShaderBindingTableRecordOffset = entity->sbtOffset;
						rayTracingInstanceBuffer[index].mask = entity->rayTracingMask;
						rayTracingInstanceBuffer[index].flags = entity->rayTracingFlags;
						rayTracingInstanceBuffer[index].transform = glm::transpose(entity->entityInstanceInfo.modelViewTransform);
						renderableEntityInstanceBuffer[(size_t)entity->GetIndex()] = entity->entityInstanceInfo;
					}
					
					// Light Source
					if (nbActiveLights < MAX_ACTIVE_LIGHTS) {
						entity->lightSource.Do([&nbActiveLights, &entity, &worldTransform](auto& lightSource){
							lightSourcesBuffer[nbActiveLights] = lightSource;
							lightSourcesBuffer[nbActiveLights].position = glm::vec4(scene->camera.viewMatrix * worldTransform * glm::dvec4(glm::dvec3(lightSource.position), 1));
							++nbActiveLights;
						});
					}
					
				}
			}
			
			currentRenderableEntities.push_back(entity);
		});
		
		// Lights
		for (int i = nbActiveLights; i < MAX_ACTIVE_LIGHTS; ++i) {
			lightSourcesBuffer[i].Reset();
		}
		
	}
	
	void RunPushCommands(VkCommandBuffer commandBuffer) {
		RenderableGeometryEntity::PushComponents(r->renderingDevice, commandBuffer);
		cameraUniformBuffer = scene->camera;
		cameraUniformBuffer.Push(commandBuffer);
		renderableEntityInstanceBuffer.Push(commandBuffer);
		lightSourcesBuffer.Push(commandBuffer);
		rayTracingInstanceBuffer.Push(commandBuffer, nbRayTracingInstances);
		
		/*if (collisionLineCount > 0) {
			collisionBuffer.Push(commandBuffer, collisionLineCount);
		}*/
		
		V4D_Mod::ForEachSortedModule([commandBuffer](auto* mod){
			if (mod->RenderFrame_Push) mod->RenderFrame_Push(commandBuffer);
		}, "render");
	}
	void RunBuildCommands(VkCommandBuffer commandBuffer) {
		// Build Buttom-level Acceleration Structures
		BuildBottomLevelRayTracingAccelerationStructures(r->renderingDevice, commandBuffer);
	
		{// Wait for BLAS to finish before building TLAS
			VkMemoryBarrier memoryBarrier {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				nullptr,// pNext
				VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,// VkAccessFlags srcAccessMask
				VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,// VkAccessFlags dstAccessMask
			};
			r->renderingDevice->CmdPipelineBarrier(
				commandBuffer, 
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
				0, 
				1, &memoryBarrier, 
				0, 0, 
				0, 0
			);
		}
		
		// Build Top-Level acceleration structure
		BuildTopLevelRayTracingAccelerationStructure(r->renderingDevice, commandBuffer);
		
		V4D_Mod::ForEachSortedModule([commandBuffer](auto* mod){
			if (mod->RenderFrame_Build) mod->RenderFrame_Build(commandBuffer);
		}, "render");
	}
	void RunGraphicsCommands(VkCommandBuffer commandBuffer) {
		
		// Run Ray-Traced rendering
		r->renderingDevice->CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, sbt_rendering.GetPipeline());
		sbt_rendering.GetPipelineLayout()->Bind(r->renderingDevice, commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
		r->renderingDevice->CmdTraceRaysKHR(
			commandBuffer, 
			sbt_rendering.GetRayGenDeviceAddressRegion(),
			sbt_rendering.GetRayMissDeviceAddressRegion(),
			sbt_rendering.GetRayHitDeviceAddressRegion(),
			sbt_rendering.GetRayCallableDeviceAddressRegion(),
			img_lit.width, img_lit.height, 1
		);
		
		// RenderFrame_Graphics
		V4D_Mod::ForEachSortedModule([&commandBuffer](auto* mod){
			if (mod->RenderFrame_Graphics) mod->RenderFrame_Graphics(commandBuffer);
		}, "render");
		
		{// Rasterization
			rasterRenderPass.Begin(r->renderingDevice, commandBuffer, img_lit);
				for (auto* s : shaderGroups["sg_transparent"]) {
					// Transparent
					RenderableGeometryEntity::ForEach([s, commandBuffer](auto entity){
						if (entity->raster_transparent && entity->sharedGeometryData) {
							uint32_t i = 0;
							for (auto& geometry : entity->sharedGeometryData->geometries) {
								RasterPushConstant pushConstant {entity->raster_wireframe_color, (int32_t)entity->GetIndex(), i++};
								s->Bind(r->renderingDevice, commandBuffer);
								s->PushConstant(r->renderingDevice, commandBuffer, &pushConstant, 0);
								if (geometry.vertexCount > 0) {
									r->renderingDevice->CmdDraw(commandBuffer,
										geometry.indexCount > 0 ? geometry.indexCount : geometry.vertexCount/*TODO must test and fix this*/, // vertexCount
										1, // instanceCount
										0, // firstVertex (defines the lowest value of gl_VertexIndex)
										0  // firstInstance (defines the lowest value of gl_InstanceIndex)
									);
								}
							}
						}
					});
				}
				for (auto* s : shaderGroups["sg_wireframe"]) {
					// Wireframe
					RenderableGeometryEntity::ForEach([s, commandBuffer](auto entity){
						if ((entity->raster_wireframe || (DEBUG_OPTIONS::WIREFRAME && entity->rayTracingMask)) && entity->sharedGeometryData) {
							uint32_t i = 0;
							for (auto& geometry : entity->sharedGeometryData->geometries) {
								RasterPushConstant pushConstant {entity->raster_wireframe_color, (int32_t)entity->GetIndex(), i++};
								r->renderingDevice->CmdSetLineWidth(commandBuffer, std::max(1.0f, entity->raster_wireframe));
								s->Bind(r->renderingDevice, commandBuffer);
								s->PushConstant(r->renderingDevice, commandBuffer, &pushConstant, 0);
								r->renderingDevice->CmdSetLineWidth(commandBuffer, std::max(1.0f, entity->raster_wireframe));
								if (geometry.vertexCount > 0) {
									r->renderingDevice->CmdDraw(commandBuffer,
										geometry.indexCount > 0 ? geometry.indexCount : geometry.vertexCount/*TODO must test and fix this*/, // vertexCount
										1, // instanceCount
										0, // firstVertex (defines the lowest value of gl_VertexIndex)
										0  // firstInstance (defines the lowest value of gl_InstanceIndex)
									);
								}
							}
						}
					});
				}
			rasterRenderPass.End(r->renderingDevice, commandBuffer);
		}
		
	}
	void RunPhysicsCommands(VkCommandBuffer commandBuffer) {
		// // Run Ray-Traced collisions
		// if (collisionLineCount > 0) {
		// 	r->renderingDevice->CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, sbt_collision.GetPipeline());
		// 	sbt_collision.GetPipelineLayout()->Bind(r->renderingDevice, commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
		// 	r->renderingDevice->CmdTraceRaysKHR(
		// 		commandBuffer, 
		// 		sbt_collision.GetRayGenDeviceAddressRegion(),
		// 		sbt_collision.GetRayMissDeviceAddressRegion(),
		// 		sbt_collision.GetRayHitDeviceAddressRegion(),
		// 		sbt_collision.GetRayCallableDeviceAddressRegion(),
		// 		collisionLineCount, 1, 1
		// 	);
		// }
		
		// // RenderFrame_Physics
		// V4D_Mod::ForEachSortedModule([&commandBuffer](auto* mod){
		// 	if (mod->RenderFrame_Physics) mod->RenderFrame_Physics(commandBuffer);
		// }, "render");
	}
	void RunPullCommands(VkCommandBuffer commandBuffer) {
		// // Pull collisions to catch it in the next frame
		// if (collisionLineCount > 0) {
		// 	collisionBuffer.Pull(commandBuffer, collisionLineCount);
		// }
		
		// Pull RayCast to catch it in the next frame
		raycastBuffer.Pull(commandBuffer);
		
		V4D_Mod::ForEachSortedModule([commandBuffer](auto* mod){
			if (mod->RenderFrame_Pull) mod->RenderFrame_Pull(commandBuffer);
		}, "render");
	}
	void RunPostCommands(VkCommandBuffer commandBuffer) {
		V4D_Mod::ForEachSortedModule([commandBuffer](auto* mod){
			if (mod->RenderFrame_Post) mod->RenderFrame_Post(commandBuffer);
		}, "render");
		
		// Post Processing
		postProcessingRenderPass.Begin(r->renderingDevice, commandBuffer, r->swapChain, {{.0,.0,.0,.0}}, currentSwapChainImageIndex);
			for (auto* shader : shaderGroups["sg_present"]) {
				shader->Execute(r->renderingDevice, commandBuffer);
			}
		postProcessingRenderPass.End(r->renderingDevice, commandBuffer);
		
		{// Copy images
			// Transition images to transfer
			r->TransitionImageLayout(commandBuffer, img_lit, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			r->TransitionImageLayout(commandBuffer, img_depth, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			r->TransitionImageLayout(commandBuffer, img_pos, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			r->TransitionImageLayout(commandBuffer, img_geometry, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			r->TransitionImageLayout(commandBuffer, img_albedo, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			r->TransitionImageLayout(commandBuffer, img_normal, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			
			r->TransitionImageLayout(commandBuffer, img_lit_history, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			r->TransitionImageLayout(commandBuffer, img_depth_history, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			r->TransitionImageLayout(commandBuffer, img_pos_history, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			r->TransitionImageLayout(commandBuffer, img_geometry_history, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			r->TransitionImageLayout(commandBuffer, img_albedo_history, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			r->TransitionImageLayout(commandBuffer, img_normal_history, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			
			r->TransitionImageLayout(commandBuffer, img_thumbnail, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			
			const VkImageSubresourceLayers imageSubresourceLayers {
				VK_IMAGE_ASPECT_COLOR_BIT,
				0,// mipLevel
				0,// baseArrayLayer
				1 // layerCount
			};
			
			// Copy to history
			VkImageCopy copyRegion {
				imageSubresourceLayers,// srcSubresource
				{0,0,0},// srcOffset
				imageSubresourceLayers,// dstSubresource
				{0,0,0},// dstOffset
				{img_lit.width,img_lit.height,1}// extent
			};
			r->renderingDevice->CmdCopyImage(commandBuffer, img_lit.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img_lit_history.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
			r->renderingDevice->CmdCopyImage(commandBuffer, img_depth.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img_depth_history.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
			r->renderingDevice->CmdCopyImage(commandBuffer, img_pos.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img_pos_history.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
			r->renderingDevice->CmdCopyImage(commandBuffer, img_geometry.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img_geometry_history.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
			r->renderingDevice->CmdCopyImage(commandBuffer, img_albedo.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img_albedo_history.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
			r->renderingDevice->CmdCopyImage(commandBuffer, img_normal.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img_normal_history.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
			
			// Generate thumbnail
			VkImageBlit blitRegion {
				imageSubresourceLayers, // srcSubresource
				{{0,0,0},{(int)img_lit.width,(int)img_lit.height,1}}, // srcOffsets
				imageSubresourceLayers, // dstSubresource
				{{0,0,0},{(int)img_thumbnail.width,(int)img_thumbnail.height,1}} // dstOffsets
			};
			r->renderingDevice->CmdBlitImage(commandBuffer, img_lit.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img_thumbnail.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_LINEAR);
			
			// Transition images back to general
			r->TransitionImageLayout(commandBuffer, img_lit, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_depth, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_pos, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_geometry, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_albedo, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_normal, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
			
			r->TransitionImageLayout(commandBuffer, img_lit_history, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_depth_history, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_pos_history, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_geometry_history, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_albedo_history, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_normal_history, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
			
			r->TransitionImageLayout(commandBuffer, img_thumbnail, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		}
	}

	// void RunTXAA() {
	// 	// TXAA
	// 	if (RENDER_OPTIONS::TXAA && !DEBUG_OPTIONS::PHYSICS && !DEBUG_OPTIONS::WIREFRAME) {
	// 		static unsigned long frameCount = 0;
	// 		static const glm::dvec2 samples8[8] = {
	// 			glm::dvec2(-7.0, 1.0) / 8.0,
	// 			glm::dvec2(-5.0, -5.0) / 8.0,
	// 			glm::dvec2(-1.0, -3.0) / 8.0,
	// 			glm::dvec2(3.0, -7.0) / 8.0,
	// 			glm::dvec2(5.0, -1.0) / 8.0,
	// 			glm::dvec2(7.0, 7.0) / 8.0,
	// 			glm::dvec2(1.0, 3.0) / 8.0,
	// 			glm::dvec2(-3.0, 5.0) / 8.0
	// 		};
	// 		glm::dvec2 texelSize = 1.0 / glm::dvec2(scene->camera.width, scene->camera.height);
	// 		glm::dvec2 subSample = samples8[frameCount % 8] * texelSize * double(scene->camera.txaaKernelSize);
	// 		scene->camera.projectionMatrix[2].x = subSample.x;
	// 		scene->camera.projectionMatrix[2].y = subSample.y;
	// 		// historyTxaaOffset = txaaOffset;
	// 		scene->camera.historyTxaaOffset = scene->camera.txaaOffset;
	// 		scene->camera.txaaOffset = subSample;
	// 		frameCount++;
	// 		scene->camera.reprojectionMatrix = (scene->camera.projectionMatrix * scene->camera.historyViewMatrix) * glm::inverse(scene->camera.projectionMatrix * scene->camera.viewMatrix);
	// 		// Save Projection and View matrices from previous frame
	// 		scene->camera.historyViewMatrix = scene->camera.viewMatrix;
	// 	}
	// }
	
#pragma endregion

///////////////////////////////////////////////////////////

V4D_MODULE_CLASS(V4D_Mod) {
	
	V4D_MODULE_FUNC(int, OrderIndex) {return -1000;}
	
	#pragma region Containers Access
		
		V4D_MODULE_FUNC(Image*, GetImage, const std::string& name) {
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
			score *= physicalDevice->deviceFeatures.rayTracingPipelineFeatures.rayTracingPipeline;
			score *= physicalDevice->deviceFeatures.rayQueryFeatures.rayQuery;
		}
		
		V4D_MODULE_FUNC(void, LoadScene, Scene* _s) {
			scene = _s;
		}
		
		V4D_MODULE_FUNC(void, UnloadScene) {
			RenderableGeometryEntity::ClearAll();
		}
		
		V4D_MODULE_FUNC(void, InitWindow, v4d::graphics::Window* w) {
			window = w;
		}
		
		V4D_MODULE_FUNC(void, InitRenderer, Renderer* _r) {
			r = _r;
			
			V4D_Mod::SortModules([](auto* a, auto* b){
				return (a->RenderOrderIndex? a->RenderOrderIndex():0) < (b->RenderOrderIndex? b->RenderOrderIndex():0);
			}, "render");
			
			r->queuesInfo.emplace_back("secondary", VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
			
			// Device Extensions
			r->RequiredDeviceExtension(VK_KHR_SHADER_CLOCK_EXTENSION_NAME);
			r->RequiredDeviceExtension(VK_KHR_16BIT_STORAGE_EXTENSION_NAME);
			if (Loader::VULKAN_API_VERSION >= VK_API_VERSION_1_2) {
				r->RequiredDeviceExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
				// RayTracing extensions
				r->RequiredDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
				r->RequiredDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
				r->RequiredDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME);
				r->RequiredDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
				r->RequiredDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
				r->RequiredDeviceExtension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
			}
		}
		
		V4D_MODULE_FUNC(void, InitVulkanDeviceFeatures, v4d::graphics::vulkan::PhysicalDevice::DeviceFeatures* deviceFeaturesToEnable, const v4d::graphics::vulkan::PhysicalDevice::DeviceFeatures* supportedDeviceFeatures) {
			deviceFeaturesToEnable->deviceFeatures2.features.shaderFloat64 = VK_TRUE;
			deviceFeaturesToEnable->deviceFeatures2.features.shaderInt64 = VK_TRUE;
			deviceFeaturesToEnable->deviceFeatures2.features.shaderInt16 = VK_TRUE;
			deviceFeaturesToEnable->deviceFeatures2.features.depthClamp = VK_TRUE;
			deviceFeaturesToEnable->deviceFeatures2.features.fillModeNonSolid = VK_TRUE;
			deviceFeaturesToEnable->deviceFeatures2.features.geometryShader = VK_TRUE;
			deviceFeaturesToEnable->deviceFeatures2.features.wideLines = VK_TRUE;
			deviceFeaturesToEnable->deviceFeatures2.features.largePoints = VK_TRUE;
			deviceFeaturesToEnable->deviceFeatures2.features.shaderTessellationAndGeometryPointSize = VK_TRUE;
			
			deviceFeaturesToEnable->shaderClockFeatures.shaderDeviceClock = VK_TRUE;
			deviceFeaturesToEnable->shaderClockFeatures.shaderSubgroupClock = VK_TRUE;
			deviceFeaturesToEnable->_16bitStorageFeatures.storageBuffer16BitAccess = VK_TRUE;
			
			// Vulkan 1.2
			if (Loader::VULKAN_API_VERSION >= VK_API_VERSION_1_2) {
				deviceFeaturesToEnable->vulkan12DeviceFeatures.bufferDeviceAddress = VK_TRUE;
				deviceFeaturesToEnable->vulkan12DeviceFeatures.shaderFloat16 = VK_TRUE;
				deviceFeaturesToEnable->vulkan12DeviceFeatures.shaderInt8 = VK_TRUE;
				deviceFeaturesToEnable->vulkan12DeviceFeatures.descriptorIndexing = VK_TRUE;
				deviceFeaturesToEnable->vulkan12DeviceFeatures.storageBuffer8BitAccess = VK_TRUE;
				deviceFeaturesToEnable->vulkan12DeviceFeatures.shaderOutputLayer = VK_TRUE;
				deviceFeaturesToEnable->accelerationStructureFeatures.accelerationStructure = VK_TRUE;
				deviceFeaturesToEnable->accelerationStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;
				deviceFeaturesToEnable->rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
				deviceFeaturesToEnable->rayTracingPipelineFeatures.rayTracingPipelineTraceRaysIndirect = VK_TRUE;
				deviceFeaturesToEnable->rayQueryFeatures.rayQuery = VK_TRUE;
			}
		}
		
		V4D_MODULE_FUNC(void, ConfigureRenderer) {
			// Query the ray tracing properties of the current implementation
			accelerationStructureProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
			rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
			accelerationStructureProperties.pNext = &rayTracingPipelineProperties;
			VkPhysicalDeviceProperties2 deviceProps2 {};{
				deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				deviceProps2.pNext = &accelerationStructureProperties;
			}
			r->GetPhysicalDeviceProperties2(r->renderingDevice->GetPhysicalDevice()->GetHandle(), &deviceProps2);
		}
	
	#pragma endregion
	
	V4D_MODULE_FUNC(void, InitVulkanLayouts) {
		{r->descriptorSets["set0"] = &set0;
			set0.AddBinding_uniformBuffer(0, cameraUniformBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR);
			set0.AddBinding_accelerationStructure(1, &topLevelAccelerationStructure.accelerationStructure, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CALLABLE_BIT_KHR);
			set0.AddBinding_storageBuffer(2, renderableEntityInstanceBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR);
			set0.AddBinding_storageBuffer(3, lightSourcesBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR);
			
			// set0.AddBinding_combinedImageSampler(4, tex_metal_normal.GetImage(), VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CALLABLE_BIT_KHR);
		}
		
		{r->descriptorSets["set1_rendering"] = &set1_rendering;
			int i = 0;
			
			set1_rendering.AddBinding_imageView(i++, &img_lit, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			set1_rendering.AddBinding_imageView(i++, &img_depth, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			set1_rendering.AddBinding_imageView(i++, &img_pos, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			set1_rendering.AddBinding_imageView(i++, &img_geometry, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			set1_rendering.AddBinding_imageView(i++, &img_albedo, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			set1_rendering.AddBinding_imageView(i++, &img_normal, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			
			set1_rendering.AddBinding_combinedImageSampler(i++, &img_lit_history, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			set1_rendering.AddBinding_combinedImageSampler(i++, &img_depth_history, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			set1_rendering.AddBinding_combinedImageSampler(i++, &img_pos_history, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			set1_rendering.AddBinding_combinedImageSampler(i++, &img_geometry_history, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			set1_rendering.AddBinding_combinedImageSampler(i++, &img_albedo_history, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			set1_rendering.AddBinding_combinedImageSampler(i++, &img_normal_history, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			
			set1_rendering.AddBinding_storageBuffer(i++, raycastBuffer, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			
			set1_rendering.AddBinding_combinedImageSampler(i++, &img_background, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR);
		}
		
		// {r->descriptorSets["set1_collision"] = &set1_collision;
		// 	set1_collision.AddBinding_storageBuffer(0, collisionBuffer, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		// }
		
		{r->descriptorSets["set1_raster"] = &set1_raster;
			set1_raster.AddBinding_combinedImageSampler(0, &img_depth, VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_post"] = &set1_post;
			int i = 0;
			
			set1_post.AddBinding_combinedImageSampler(i++, &img_lit, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_depth, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_pos, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_geometry, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_albedo, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_normal, VK_SHADER_STAGE_FRAGMENT_BIT);
			
			set1_post.AddBinding_combinedImageSampler(i++, &img_overlay, VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_overlay"] = &set1_overlay;
			set1_overlay.AddBinding_combinedImageSampler(0, tex_img_font_atlas.GetImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_histogram"] = &set1_histogram;
			set1_histogram.AddBinding_imageView(0, &img_thumbnail, VK_SHADER_STAGE_COMPUTE_BIT);
			set1_histogram.AddBinding_storageBuffer(1, totalLuminance, VK_SHADER_STAGE_COMPUTE_BIT);
		}
		
		{ // Assign descriptor sets to layouts
			// Add the same set 0 to all pipeline layouts
			for (auto[name, layout] : pipelineLayouts) {
				layout->AddDescriptorSet(&set0);
			}
			// Add specific set 1 to specific layout lists
			pipelineLayouts["pl_rendering"]->AddDescriptorSet(&set1_rendering);
			// pipelineLayouts["pl_collision"]->AddDescriptorSet(&set1_collision);
			pipelineLayouts["pl_raster"]->AddDescriptorSet(&set1_raster);
			pipelineLayouts["pl_overlay"]->AddDescriptorSet(&set1_overlay);
			pipelineLayouts["pl_post"]->AddDescriptorSet(&set1_post);
			pipelineLayouts["pl_histogram"]->AddDescriptorSet(&set1_histogram);
		}
	}
	
	V4D_MODULE_FUNC(void, AddRayTracingHitShader, v4d::modular::ModuleID mod, const std::string& name) {
		AddRayTracingShader(mod, name);
	}
	
	#pragma region Load/Upload Renderer
		
		V4D_MODULE_FUNC(void, ConfigureShaders) {
			ConfigureRayTracingShaders();
			ConfigureRasterShaders();
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
		}
		
		V4D_MODULE_FUNC(void, CreateVulkanSyncObjects) {
			VkSemaphoreCreateInfo semaphoreInfo = {};
				semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkFenceCreateInfo fenceInfo = {};
				fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Initialize in the signaled state so that we dont get stuck on the first frame

			for (auto&[name, s] : semaphores) {
				if (r->renderingDevice->CreateSemaphore(&semaphoreInfo, nullptr, &s) != VK_SUCCESS) {
					throw std::runtime_error("Failed to create semaphores");
				}
			}
			for (auto&[name, f] : fences) {
				if (r->renderingDevice->CreateFence(&fenceInfo, nullptr, &f) != VK_SUCCESS) {
					throw std::runtime_error("Failed to create fences");
				}
			}
		}

		V4D_MODULE_FUNC(void, DestroyVulkanSyncObjects) {
			for (auto&[name, s] : semaphores) r->renderingDevice->DestroySemaphore(s, nullptr);
			for (auto&[name, f] : fences) r->renderingDevice->DestroyFence(f, nullptr);
		}

		V4D_MODULE_FUNC(void, CreateVulkanResources) {
			CreateUiResources();
			CreateRenderingResources();
			CreatePostProcessingResources();
		
			CreateRayTracingResources();
			
			// ImGui
			tex_img_font_atlas.SetMipLevels();
			tex_img_font_atlas.AllocateAndWriteStagingMemory(r->renderingDevice);
			tex_img_font_atlas.CreateImage(r->renderingDevice);
			auto commandBuffer = r->renderingDevice->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
				tex_img_font_atlas.CopyStagingBufferToImage(r->renderingDevice, commandBuffer);
			r->renderingDevice->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
			tex_img_font_atlas.FreeStagingMemory(r->renderingDevice);
			
			for (auto* tex : textures) {
				tex->SetSamplerAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT);
				tex->SetMipLevels();
				tex->AllocateAndWriteStagingMemory(r->renderingDevice);
				tex->CreateImage(r->renderingDevice);
				auto commandBuffer = r->renderingDevice->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
					tex->CopyStagingBufferToImage(r->renderingDevice, commandBuffer);
				r->renderingDevice->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
				tex->FreeStagingMemory(r->renderingDevice);
			}
		}
		
		V4D_MODULE_FUNC(void, DestroyVulkanResources) {
			DestroyUiResources();
			DestroyRenderingResources();
			DestroyPostProcessingResources();
			DestroyRayTracingResources();
			
			// Textures
			tex_img_font_atlas.DestroyImage(r->renderingDevice);
			
			for (auto* tex : textures) {
				tex->DestroyImage(r->renderingDevice);
			}
		}
		
		V4D_MODULE_FUNC(void, AllocateVulkanBuffers) {
			// Buffers
			cameraUniformBuffer.Allocate(r->renderingDevice);
			renderableEntityInstanceBuffer.Allocate(r->renderingDevice);
			lightSourcesBuffer.Allocate(r->renderingDevice);
			
			// Overlays
			overlayLinesBuffer.Allocate(r->renderingDevice, MEMORY_USAGE_CPU_TO_GPU);
			overlayLinesBuffer.MapMemory(r->renderingDevice);
			overlayLines = (OverlayLine*)overlayLinesBuffer.data;
			overlayTextBuffer.Allocate(r->renderingDevice, MEMORY_USAGE_CPU_TO_GPU);
			overlayTextBuffer.MapMemory(r->renderingDevice);
			overlayText = (OverlayText*)overlayTextBuffer.data;
			overlayCirclesBuffer.Allocate(r->renderingDevice, MEMORY_USAGE_CPU_TO_GPU);
			overlayCirclesBuffer.MapMemory(r->renderingDevice);
			overlayCircles = (OverlayCircle*)overlayCirclesBuffer.data;
			overlaySquaresBuffer.Allocate(r->renderingDevice, MEMORY_USAGE_CPU_TO_GPU);
			overlaySquaresBuffer.MapMemory(r->renderingDevice);
			overlaySquares = (OverlaySquare*)overlaySquaresBuffer.data;
			
			AllocateComputeBuffers();
			AllocateRayTracingBuffers();
		}
		
		V4D_MODULE_FUNC(void, FreeVulkanBuffers) {
			
			// Entities
			RenderableGeometryEntity::CleanupOnThisThread();
			currentRenderableEntities.clear();
			RenderableGeometryEntity::ForEach([](auto entity){
				entity->FreeComponentsBuffers();
			});
			
			{// Overlays
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
			}
			
			{// Buffers
				cameraUniformBuffer.Free();
				renderableEntityInstanceBuffer.Free();
				lightSourcesBuffer.Free();
				
				FreeComputeBuffers();
				FreeRayTracingBuffers();
			}
		}

		V4D_MODULE_FUNC(void, CreateVulkanPipelines) {
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
			
			// Ray Tracing
			CreateRayTracingPipeline();
			
			// Raster
			CreateRasterPipeline();
			
			// Post Processing
			CreatePostProcessingPipeline();
		}
		
		V4D_MODULE_FUNC(void, DestroyVulkanPipelines) {
			// UI
			#ifdef _ENABLE_IMGUI
				UnloadImGui();
			#endif
			DestroyUiPipeline();
			
			// Ray Tracing
			DestroyRayTracingPipeline();
			
			// Raster
			DestroyRasterPipeline();
			
			// Post Processing
			DestroyPostProcessingPipeline();
			
			// Pipeline layouts
			for (auto[name,layout] : pipelineLayouts) {
				layout->Destroy(r->renderingDevice);
			}
			
		}
		
		V4D_MODULE_FUNC(void, CreateVulkanCommandBuffers) {
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandBufferCount = 1;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
							/*	VK_COMMAND_BUFFER_LEVEL_PRIMARY = Can be submitted to a queue for execution, but cannot be called from other command buffers
								VK_COMMAND_BUFFER_LEVEL_SECONDARY = Cannot be submitted directly, but can be called from primary command buffers
							*/
			for (auto&[name, cmdBuffer] : commandBuffers) {
				allocInfo.commandPool = r->renderingDevice->GetQueue(cmdBuffer.queue).commandPool;
				if (r->renderingDevice->AllocateCommandBuffers(&allocInfo, cmdBuffer) != VK_SUCCESS) {
					throw std::runtime_error((std::string("Failed to allocate ") + name + " command buffer").c_str());
				}
				// if (name == "Post") {
				// 	// Starting command buffer recording...
				// 	VkCommandBufferBeginInfo beginInfo = {};
				// 	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				// 	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
				// 	beginInfo.pInheritanceInfo = nullptr;
				// 	if (r->renderingDevice->BeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS) {
				// 		throw std::runtime_error("Faild to begin recording command buffer");
				// 	}
					
				// 	// Record commands
				// 	RecordPostCommandBuffer(cmdBuffer, 0);
					
				// 	if (r->renderingDevice->EndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
				// 		throw std::runtime_error("Failed to record command buffer");
				// 	}
				// }
			}
		}

		V4D_MODULE_FUNC(void, DestroyVulkanCommandBuffers) {
			for (auto&[name, cmdBuffer] : commandBuffers) {
				r->renderingDevice->FreeCommandBuffers(r->renderingDevice->GetQueue(cmdBuffer.queue).commandPool, 1, cmdBuffer);
				cmdBuffer = VK_NULL_HANDLE;
			}
		}
	
	#pragma endregion
	
	#pragma region Overlay
		
		V4D_MODULE_FUNC(void, DrawOverlayLine, float x1, float y1, float x2, float y2, glm::vec4 color, float lineWidth) {
			AddOverlayLine(x1, y1, x2, y2, color, lineWidth);
		}
		V4D_MODULE_FUNC(void, DrawOverlayLineViewSpace, glm::dvec3 line1, glm::dvec3 line2, glm::vec4 color, float lineWidth) {
			AddOverlayLine(line1, line2, color, lineWidth);
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

	#define RecordAndSubmitCommandBuffer(cmdBufferName) {\
		auto& cmdBuffer = commandBuffers[ #cmdBufferName ];\
		VkCommandBufferBeginInfo beginInfo = {};\
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;\
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;\
		VkSubmitInfo submitInfo {};\
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;\
			submitInfo.waitSemaphoreCount = waitSemaphores.size();\
			submitInfo.pWaitSemaphores = waitSemaphores.data();\
			submitInfo.pWaitDstStageMask = waitStages.data();\
			submitInfo.commandBufferCount = 1;\
			submitInfo.pCommandBuffers = cmdBuffer;\
			submitInfo.signalSemaphoreCount = signalSemaphores.size();\
			submitInfo.pSignalSemaphores = signalSemaphores.data();\
		\
		if (triggerFence) r->renderingDevice->ResetFences(1, &triggerFence);\
		r->renderingDevice->ResetCommandBuffer(cmdBuffer, 0);\
		if (r->renderingDevice->BeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS) {\
			throw std::runtime_error("Faild to begin recording " #cmdBufferName " command buffer");\
		}\
		\
		Run ## cmdBufferName ## Commands(cmdBuffer);\
		\
		if (r->renderingDevice->EndCommandBuffer(cmdBuffer) != VK_SUCCESS) {\
			throw std::runtime_error("Failed to record " #cmdBufferName " command buffer");\
		}\
		\
		VkResult result = r->renderingDevice->QueueSubmit(r->renderingDevice->GetQueue(cmdBuffer.queue).handle, 1, &submitInfo, triggerFence);\
		if (result != VK_SUCCESS) {\
			if (result == VK_ERROR_DEVICE_LOST) {\
				LOG_ERROR("Failed to submit " #cmdBufferName " command buffer : VK_ERROR_DEVICE_LOST")\
			}\
			LOG_ERROR((int)result)\
			throw std::runtime_error("Failed to submit " #cmdBufferName " command buffer");\
		}\
	}

	V4D_MODULE_FUNC(void, RenderUpdate) {

		{// Aquire image from the swapchain
			VkResult result = r->renderingDevice->AcquireNextImageKHR(
				r->swapChain->GetHandle(), // swapChain
				1000UL * 1000 * 1000, // timeout in nanoseconds (using max disables the timeout)
				semaphores["post1"], // semaphore
				VK_NULL_HANDLE, // fence
				&currentSwapChainImageIndex // output the index of the swapchain image in there
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
		
		{// Wait For BeforeUpdate Fence
			if (r->renderingDevice->WaitForFences(1, &fences["beforeUpdate"], VK_TRUE, /*timeout*/1000UL * 1000 * 1000) != VK_SUCCESS) {
				throw std::runtime_error("Failed to Wait for beforeUpdate fence");
			}
		}
		
		BeforeUpdate();

		{// Wait For FrameUpdate Fence
			if (r->renderingDevice->WaitForFences(1, &fences["frameUpdate"], VK_TRUE, /*timeout*/1000UL * 1000 * 1000) != VK_SUCCESS) {
				throw std::runtime_error("Failed to Wait for frameUpdate fence");
			}
		}
		
		FrameUpdate();

		{// Push
			std::vector<VkSemaphore> waitSemaphores {};
			std::vector<VkPipelineStageFlags> waitStages {};
			if (r->frameIndex > 0) {
				waitSemaphores.push_back(semaphores["push"]);
				waitStages.push_back(VK_PIPELINE_STAGE_TRANSFER_BIT);
			}
			const std::vector<VkSemaphore> signalSemaphores {
				semaphores["build"],
			};
			const VkFence triggerFence = fences["beforeUpdate"];
			
			RecordAndSubmitCommandBuffer(Push)
		}
		
		{// Build
			const std::vector<VkSemaphore> waitSemaphores {
				semaphores["build"],
			};
			const std::vector<VkPipelineStageFlags> waitStages {
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
			};
			const std::vector<VkSemaphore> signalSemaphores {
				semaphores["graphics"],
				// semaphores["physics"],
			};
			const VkFence triggerFence = VK_NULL_HANDLE;
			
			RecordAndSubmitCommandBuffer(Build)
		}
		
		{// Graphics
			const std::vector<VkSemaphore> waitSemaphores {
				semaphores["graphics"],
			};
			const std::vector<VkPipelineStageFlags> waitStages {
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
			};
			const std::vector<VkSemaphore> signalSemaphores {
				semaphores["pull1"],
				semaphores["post2"],
			};
			const VkFence triggerFence = VK_NULL_HANDLE;
			
			RecordAndSubmitCommandBuffer(Graphics)
		}
		
		// {// Physics
		// 	const std::vector<VkSemaphore> waitSemaphores {
		// 		semaphores["physics"],
		// 	};
		// 	const std::vector<VkPipelineStageFlags> waitStages {
		// 		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
		// 	};
		// 	const std::vector<VkSemaphore> signalSemaphores {
		// 		semaphores["pull2"],
		// 	};
		// 	const VkFence triggerFence = VK_NULL_HANDLE;
			
		// 	RecordAndSubmitCommandBuffer(Physics)
		// }
		
		{// Pull
			const std::vector<VkSemaphore> waitSemaphores {
				semaphores["pull1"],
				// semaphores["pull2"],
			};
			const std::vector<VkPipelineStageFlags> waitStages {
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
			};
			const std::vector<VkSemaphore> signalSemaphores {};
			const VkFence triggerFence = fences["frameUpdate"];
			
			RecordAndSubmitCommandBuffer(Pull)
		}
		
		{// Wait For Post Fence
			if (r->renderingDevice->WaitForFences(1, &fences["post"], VK_TRUE, /*timeout*/1000UL * 1000 * 1000) != VK_SUCCESS) {
				throw std::runtime_error("Failed to Wait for post fence");
			}
		}
		
		{// Post
			const std::vector<VkSemaphore> waitSemaphores {
				semaphores["post1"],
				semaphores["post2"],
			};
			const std::vector<VkPipelineStageFlags> waitStages {
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			};
			const std::vector<VkSemaphore> signalSemaphores {
				semaphores["present"],
				semaphores["push"],
			};
			const VkFence triggerFence = fences["post"];
			
			RecordAndSubmitCommandBuffer(Post)
		}
		
		{// Present
			const std::vector<VkSemaphore> waitSemaphores {
				semaphores["present"],
			};
			
			VkPresentInfoKHR presentInfo = {};
				presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
				// Specify which semaphore to wait on before presentation can happen
				presentInfo.waitSemaphoreCount = waitSemaphores.size();
				presentInfo.pWaitSemaphores = waitSemaphores.data();
				// Specify the swap chains to present images to and the index for each swap chain. (almost always a single one)
				VkSwapchainKHR swapChains[] = {r->swapChain->GetHandle()};
				presentInfo.swapchainCount = 1;
				presentInfo.pSwapchains = swapChains;
				presentInfo.pImageIndices = &currentSwapChainImageIndex;
				// The next param allows to specify an array of VkResult values to check for every individual swap chain if presentation was successful.
				// its not necessary if only using a single swap chain, because we can simply use the return value of the present function.
				presentInfo.pResults = nullptr;
				// Send the present info to the presentation queue !
				// This submits the request to present an image to the swap chain.
				
			VkResult result = r->renderingDevice->QueuePresentKHR(r->renderingDevice->GetQueue("present").handle, &presentInfo);

			// Check for errors
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				// SwapChain is out of date, for instance if the window was resized, stop here and ReCreate the swapchain.
				r->RecreateSwapChains();
				return;
			} else if (result == VK_SUBOPTIMAL_KHR) {
				LOG_VERBOSE("Swapchain is suboptimal...")
			} else if (result != VK_SUCCESS) {
				throw std::runtime_error("Failed to present to swap chain image");
			}
		}
		
	}
	
	V4D_MODULE_FUNC(void, SecondaryRenderUpdate) {
		// Histogram
		glm::vec4 luminance;
		totalLuminance.ReadFromMappedData(&luminance);
		if (luminance.a > 0) {
			scene->camera.luminance = glm::vec4(glm::vec3(luminance) / luminance.a, exposureFactor);
		}
		
		// Modules
		V4D_Mod::ForEachSortedModule([](auto* mod){
			if (mod->BeginSecondaryFrameUpdate) mod->BeginSecondaryFrameUpdate();
		});
	
		// Secondary Dynamic graphics/compute
		auto cmdBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("secondary"));
		
			// histogram
			shader_histogram_compute.SetGroupCounts(1, 1, 1);
			shader_histogram_compute.Execute(r->renderingDevice, cmdBuffer);
			
			// Modules
			V4D_Mod::ForEachSortedModule([cmdBuffer](auto* mod){
				if (mod->SecondaryFrameCompute) mod->SecondaryFrameCompute(cmdBuffer);
			});
			V4D_Mod::ForEachSortedModule([cmdBuffer](auto* mod){
				if (mod->SecondaryRenderUpdate2) mod->SecondaryRenderUpdate2(cmdBuffer);
			}, "render");
			
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("secondary"), cmdBuffer);
	}
	
	V4D_MODULE_FUNC(void, DrawUi) {
		#ifdef _ENABLE_IMGUI
			ImGui::SetNextWindowSize({340, 150}, ImGuiCond_FirstUseEver);
			ImGui::Begin("Settings and Modules");
			// #ifdef _DEBUG
				ImGui::Checkbox("Debug Wireframe", &DEBUG_OPTIONS::WIREFRAME);
				ImGui::Checkbox("Debug Physics", &DEBUG_OPTIONS::PHYSICS);
				{
					const char* items[] = {
						"NOTHING",
						"STANDARD",
						"NORMALS",
						"ALBEDO",
						"EMISSION",
						"DEPTH",
						"DISTANCE",
						"METALLIC",
						"ROUGNESS",
						"TIME",
						"BOUNCES",
					};
					static const char* currentItem = "STANDARD";
					if (ImGui::BeginCombo("Render mode", currentItem)) {
						for (int n = 0; n < IM_ARRAYSIZE(items); ++n) {
							bool isSelected = (currentItem == items[n]);
							if (ImGui::Selectable(items[n], isSelected)) {
								currentItem = items[n];
								scene->camera.renderMode = n;
							}
							if (isSelected) {
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}
				}
				if (scene->camera.renderMode > RENDER_MODE_STANDARD) {
					ImGui::SliderFloat("RenderMode Scaling", &scene->camera.renderDebugScaling, 0.1f, 2.0f);
					if (scene->camera.renderMode == RENDER_MODE_DISTANCE) {
						float maxDistance = glm::pow(10.0f, scene->camera.renderDebugScaling*4);
						std::string distanceStr = std::to_string(glm::round(maxDistance)) + " meters";
						if (maxDistance > 3000) {
							distanceStr = std::to_string(glm::round(maxDistance/1000*10)/10) + " km";
						}
						ImGui::Text((std::string("White is >= ") + distanceStr).c_str());
					}
					if (scene->camera.renderMode == RENDER_MODE_BOUNCES) {
						ImGui::Text((std::string("White is >= ") + std::to_string((int)glm::ceil(scene->camera.renderDebugScaling*5)) + " bounces").c_str());
					}
				}
			// #endif
			if (scene->camera.renderMode == RENDER_MODE_STANDARD || scene->camera.renderMode == RENDER_MODE_BOUNCES || scene->camera.renderMode == RENDER_MODE_TIME) {
				ImGui::Separator();
				ImGui::SliderInt("Light Bounces", &scene->camera.maxBounces, -1, 10); // -1 = infinite bounces
				ImGui::SliderFloat("Max bounce time", &scene->camera.bounceTimeBudget, 0.0f, 2.0f);
				ImGui::Separator();
				ImGui::Text("LTBL (Path-tracing preview)");
				ImGui::Checkbox("Soft Shadows", &RENDER_OPTIONS::SOFT_SHADOWS);
				ImGui::Checkbox("Path tracing & Rough surfaces", &RENDER_OPTIONS::PATH_TRACING);
				if (scene->camera.renderMode == RENDER_MODE_STANDARD) {
					ImGui::SliderFloat("Denoise", &scene->camera.denoise, 0.0f, 64.0f);
					// if (!DEBUG_OPTIONS::WIREFRAME && !DEBUG_OPTIONS::PHYSICS) {
					// 	ImGui::Checkbox("TXAA", &RENDER_OPTIONS::TXAA);
					// }
					ImGui::Checkbox("Gamma correction", &RENDER_OPTIONS::GAMMA_CORRECTION);
					ImGui::Checkbox("HDR Tone Mapping", &RENDER_OPTIONS::HDR_TONE_MAPPING);
					ImGui::SliderFloat("HDR Exposure", &exposureFactor, 0, 10);
					ImGui::SliderFloat("brightness", &scene->camera.brightness, 0, 2);
					ImGui::SliderFloat("contrast", &scene->camera.contrast, 0, 2);
					ImGui::SliderFloat("gamma", &scene->camera.gamma, 0, 5);
					ImGui::SliderFloat("Multi-Sampling kernel size", &scene->camera.multisamplingKernelSize, 0, 4);
				}
			}
			// Collision detection
			ImGui::SliderInt("Stochastic collision rays", &nbStochasticRayTracedCollisions, 1, 100);
		#endif
		// Modules
		V4D_Mod::ForEachSortedModule([](auto* mod){
			if (mod->DrawUi2) {
				#ifdef _ENABLE_IMGUI
					ImGui::Separator();
				#endif
				mod->DrawUi2();
			}
		});
		#ifdef _ENABLE_IMGUI
			ImGui::End();
		#endif
		// #ifdef _DEBUG
			#ifdef _ENABLE_IMGUI
				ImGui::SetNextWindowPos({425+345,0}, ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowSize({250, 100}, ImGuiCond_FirstUseEver);
				ImGui::Begin("Debug");
				if (scene->camera.accumulateFrames != -1) {
					ImGui::Text("Accumulated frames: %d", scene->camera.accumulateFrames);
				}
				// RayCast
				if (previousRayCast) ImGui::Text((std::string("RayCast ") + std::string(v4d::modular::ModuleID(previousRayCast.moduleVen, previousRayCast.moduleId)) + ":" + std::to_string(previousRayCast.objId)).c_str());
				else ImGui::Text("RayCast no hit");
			#endif
				// Modules
				V4D_Mod::ForEachSortedModule([](auto* mod){
					if (mod->DrawUiDebug2) {
						#ifdef _ENABLE_IMGUI
							ImGui::Separator();
						#endif
						mod->DrawUiDebug2();
					}
				});
			#ifdef _ENABLE_IMGUI
				ImGui::End();
			#endif
		// #endif
	}
	
	V4D_MODULE_FUNC(void, DrawUiDebug2) {
		#ifdef _ENABLE_IMGUI
			ImGui::Text("%d rendered objects", RenderableGeometryEntity::CountActive());
		#endif
	}
	
	V4D_MODULE_FUNC(void, SecondaryRenderUpdate2, VkCommandBuffer commandBuffer) {
		
		shader_overlay_lines.SetData(overlayLinesBuffer.buffer, 0);
		shader_overlay_text.SetData(overlayTextBuffer.buffer, 0);
		shader_overlay_circles.SetData(overlayCirclesBuffer.buffer, 0);
		shader_overlay_squares.SetData(overlaySquaresBuffer.buffer, 0);
		
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
	
	#pragma region Input
	
	V4D_MODULE_FUNC(std::string, InputCallbackName) {return THIS_MODULE;}
	
	V4D_MODULE_FUNC(void, InputKeyCallback, int key, int scancode, int action, int mods) {
		
		if (key == GLFW_KEY_F12) {
			if (action == GLFW_RELEASE) {
				if (scene->camera.accumulateFrames == -1) {
					scene->camera.accumulateFrames = 0;
				} else {
					scene->camera.accumulateFrames = -1;
				}
			}
			return;
		}
		
		if (action != GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)
			#endif
		) {
			switch (key) {
				
				// Reload Renderer
				case GLFW_KEY_R:
					r->ReloadRenderer();
					break;
				
				// Quit
				case GLFW_KEY_ESCAPE:
					glfwSetWindowShouldClose(window->GetHandle(), 1);
					break;
					
			}
		}
	}
	
	V4D_MODULE_FUNC(void, MouseButtonCallback, int button, int action, int mods) {
		if (action == GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)
			#endif
		) {
			switch (button) {
				case GLFW_MOUSE_BUTTON_1:
					glfwSetInputMode(window->GetHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
					glfwSetCursorPos(window->GetHandle(), 0, 0);
					break;
				case GLFW_MOUSE_BUTTON_2:
					glfwSetCursorPos(window->GetHandle(), 0, 0);
					glfwSetInputMode(window->GetHandle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
					break;
			}
		}
	}
	
	#pragma endregion
	
};
