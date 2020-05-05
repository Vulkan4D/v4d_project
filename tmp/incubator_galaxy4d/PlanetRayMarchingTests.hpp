#pragma once

#include <v4d.h>
#include "../incubator_rendering/V4DRenderingPipeline.hh"
#include "../incubator_rendering/Camera.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

class Planet {
	
	PipelineLayout planetPipelineLayout;
	RasterShaderPipeline planetShader {planetPipelineLayout, {
		"incubator_galaxy4d/assets/shaders/planetRayMarching.vert",
		// "incubator_galaxy4d/assets/shaders/planetRayMarching.geom",
		"incubator_galaxy4d/assets/shaders/planetRayMarching.frag",
	}};
	
	RenderPass distanceFieldRenderPass;
	RasterShaderPipeline distanceFieldShader {planetPipelineLayout, {
		"incubator_galaxy4d/assets/shaders/planetRayMarching.vert",
		// "incubator_galaxy4d/assets/shaders/planetRayMarching.geom",
		"incubator_galaxy4d/assets/shaders/planetRayMarching.distancefield.frag",
	}};
	float distanceFieldScale = 1./8;
	Image distanceFieldImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1,1, { VK_FORMAT_R32G32B32A32_SFLOAT } };
	
	// struct PlanetInfo {
	// 	double radius;
	// };
	
	struct PlanetChunk { // max 128 bytes
		glm::dvec3 planetAbsolutePosition; // 24
		float planetRadius; // 4
		// bytes remaining: 100
	} planetPushConstant;
	
	// std::array<PlanetInfo, 255> planets {};
	// Buffer planetsBuffer { VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(PlanetInfo) * 255 };
	
	const int SPHERE_SIDE_QUAD_SUBDIVISIONS = 32;
	const int SPHERE_VERTICES = (SPHERE_SIDE_QUAD_SUBDIVISIONS * SPHERE_SIDE_QUAD_SUBDIVISIONS * 6) ;// * 6;
	struct Vertex {
		double posX;
		double posY;
		double posZ;
		glm::vec4 pos;
	};
	std::vector<Vertex> vertices {};
	Buffer vertexBuffer { VK_BUFFER_USAGE_VERTEX_BUFFER_BIT };
	
public:
	
	void Init(Renderer* renderer) {
		// planetsBuffer.AddSrcDataPtr<PlanetInfo, 255>(&planets);
		
		
	}
	
	void Info(Renderer* renderer, Device* renderingDevice) {
		
	}
	
	void InitLayouts(Renderer* renderer, std::vector<DescriptorSet*>& descriptorSets, DescriptorSet* baseDescriptorSet_0) {
		auto* planetDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		// planetDescriptorSet_1->AddBinding_uniformBuffer(0, &planetsBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		planetDescriptorSet_1->AddBinding_combinedImageSampler(0, &distanceFieldImage, VK_SHADER_STAGE_FRAGMENT_BIT);
		
		planetPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		planetPipelineLayout.AddDescriptorSet(planetDescriptorSet_1);
		planetPipelineLayout.AddPushConstant<PlanetChunk>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	
	void ConfigureShaders() {
		planetShader.rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
		planetShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		planetShader.depthStencilState.depthWriteEnable = VK_FALSE;
		planetShader.depthStencilState.depthTestEnable = VK_FALSE;
		planetShader.AddVertexInputBinding(sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX, {
			{0, offsetof(Vertex, Vertex::posX), VK_FORMAT_R64_SFLOAT},
			{1, offsetof(Vertex, Vertex::posY), VK_FORMAT_R64_SFLOAT},
			{2, offsetof(Vertex, Vertex::posZ), VK_FORMAT_R64_SFLOAT},
			{3, offsetof(Vertex, Vertex::pos), VK_FORMAT_R32G32B32A32_SFLOAT},
		});
		
		distanceFieldShader.rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
		distanceFieldShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		distanceFieldShader.depthStencilState.depthWriteEnable = VK_FALSE;
		distanceFieldShader.depthStencilState.depthTestEnable = VK_FALSE;
		distanceFieldShader.AddVertexInputBinding(sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX, {
			{0, offsetof(Vertex, Vertex::posX), VK_FORMAT_R64_SFLOAT},
			{1, offsetof(Vertex, Vertex::posY), VK_FORMAT_R64_SFLOAT},
			{2, offsetof(Vertex, Vertex::posZ), VK_FORMAT_R64_SFLOAT},
			{3, offsetof(Vertex, Vertex::pos), VK_FORMAT_R32G32B32A32_SFLOAT},
		});
	}
	
	void LoadScene() {
		// planets[0].radius = 6000000;
		
		vertices.clear();
		vertices.reserve(SPHERE_VERTICES);
		
		double planetRadius = 24000000; // 4x earth
		
		double size = SPHERE_SIDE_QUAD_SUBDIVISIONS;
		
		enum FACE : int {
			FRONT,
			BACK,
			RIGHT,
			LEFT,
			TOP,
			BOTTOM
		};
		
		for (int face = 0; face < 6; ++face) {
			glm::dvec3 top;
			glm::dvec3 right;
			glm::dvec3 dir;
			switch (face) {
				case FRONT:
					dir = glm::dvec3(0, 0, 1);
					top = glm::dvec3(0, 1, 0);
					right = glm::dvec3(1, 0, 0);
					break;
				case BACK:
					dir = glm::dvec3(0, 0, -1);
					top = glm::dvec3(0, 1, 0);
					right = glm::dvec3(-1, 0, 0);
					break;
				case RIGHT:
					dir = glm::dvec3(1, 0, 0);
					top = glm::dvec3(0, 1, 0);
					right = glm::dvec3(0, 0, -1);
					break;
				case LEFT:
					dir = glm::dvec3(-1, 0, 0);
					top = glm::dvec3(0, -1, 0);
					right = glm::dvec3(0, 0, -1);
					break;
				case TOP:
					dir = glm::dvec3(0, 1, 0);
					top = glm::dvec3(0, 0, 1);
					right = glm::dvec3(-1, 0, 0);
					break;
				case BOTTOM:
					dir = glm::dvec3(0, -1, 0);
					top = glm::dvec3(0, 0, -1);
					right = glm::dvec3(-1, 0, 0);
					break;
			}
			top /= 2.0;
			right /= 2.0;
			
			for (int row = 0; row < SPHERE_SIDE_QUAD_SUBDIVISIONS; ++row) {
				for (int col = 0; col < SPHERE_SIDE_QUAD_SUBDIVISIONS; ++col) {
					glm::dvec3 pos;
					switch (face) {
						case FRONT:
							pos = {col, row, size-1};
							break;
						case BACK:
							pos = {col, row, 0};
							break;
						case RIGHT:
							pos = {size-1, row, col};
							break;
						case LEFT:
							pos = {0, row, col};
							break;
						case TOP:
							pos = {col, size-1, row};
							break;
						case BOTTOM:
							pos = {col, 0, row};
							break;
					}
					pos -= 0.5*size;
					pos += dir/2.0;
					
					glm::dvec3 posTopLeft = normalize(pos + top - right) * planetRadius;
					glm::dvec3 posTopRight = normalize(pos + top + right) * planetRadius;
					glm::dvec3 posBottomRight = normalize(pos - top + right) * planetRadius;
					glm::dvec3 posBottomLeft = normalize(pos - top - right) * planetRadius;
					
					// glm::dvec3 posCenter = (posTopLeft + posTopRight + posBottomRight + posBottomLeft) / 4.0;
					glm::dvec3 posCenter {0};
					
					float _someCachedData = 0; //TODO potentially cache something here
					
					glm::vec4 topLeft = glm::vec4(posTopLeft - posCenter, _someCachedData);
					glm::vec4 topRight = glm::vec4(posTopRight - posCenter, _someCachedData);
					glm::vec4 bottomRight = glm::vec4(posBottomRight - posCenter, _someCachedData);
					glm::vec4 bottomLeft = glm::vec4(posBottomLeft - posCenter, _someCachedData);
					
					vertices.push_back({
						posCenter.x, posCenter.y, posCenter.z,
						topLeft,
					});
					vertices.push_back({
						posCenter.x, posCenter.y, posCenter.z,
						bottomLeft,
					});
					vertices.push_back({
						posCenter.x, posCenter.y, posCenter.z,
						topRight,
					});
					vertices.push_back({
						posCenter.x, posCenter.y, posCenter.z,
						topRight,
					});
					vertices.push_back({
						posCenter.x, posCenter.y, posCenter.z,
						bottomLeft,
					});
					vertices.push_back({
						posCenter.x, posCenter.y, posCenter.z,
						bottomRight,
					});
					
				}
			}
		}
		
		vertexBuffer.AddSrcDataPtr(&vertices);
		planetShader.SetData(&vertexBuffer, vertices.size());
		distanceFieldShader.SetData(&vertexBuffer, vertices.size());
	}
	
	void ReadShaders() {
		planetShader.ReadShaders();
		distanceFieldShader.ReadShaders();
	}
	
	void CreateResources(Renderer* renderer, Device* renderingDevice, float screenWidth, float screenHeight) {
		uint distanceFieldWidth = (uint)(screenWidth * distanceFieldScale);
		uint distanceFieldHeight = (uint)(screenHeight * distanceFieldScale);
		distanceFieldImage.samplerInfo.minFilter = VK_FILTER_NEAREST;
		distanceFieldImage.samplerInfo.magFilter = VK_FILTER_NEAREST;
		distanceFieldImage.samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		distanceFieldImage.samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		distanceFieldImage.samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		distanceFieldImage.Create(renderingDevice, distanceFieldWidth, distanceFieldHeight);
		renderer->TransitionImageLayout(distanceFieldImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	}
	
	void DestroyResources(Device* renderingDevice) {
		distanceFieldImage.Destroy(renderingDevice);
	}
	
	void AllocateBuffers(Renderer* renderer, Device* renderingDevice, Queue& transferQueue) {
		// renderer->AllocateBufferStaged(transferQueue, planetsBuffer);
		
		renderer->AllocateBufferStaged(transferQueue, vertexBuffer);
	}
	
	void FreeBuffers(Renderer* renderer, Device* renderingDevice) {
		// planetsBuffer.Free(renderingDevice);
		
		vertexBuffer.Free(renderingDevice);
	}
	
	void CreatePipelines(Renderer* renderer, Device* renderingDevice, std::vector<RasterShaderPipeline*>& opaqueLightingShaders) {
		planetPipelineLayout.Create(renderingDevice);
		opaqueLightingShaders.push_back(&planetShader);
		
		// Distance Field
		{
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = distanceFieldImage.format;
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
				distanceFieldRenderPass.AddAttachment(colorAttachment),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
				subpass.pDepthStencilAttachment = nullptr;
			distanceFieldRenderPass.AddSubpass(subpass);
			
			// Create the render pass
			distanceFieldRenderPass.Create(renderingDevice);
			distanceFieldRenderPass.CreateFrameBuffers(renderingDevice, distanceFieldImage);
			
			// Shader pipeline
			distanceFieldShader.SetRenderPass(&distanceFieldImage, distanceFieldRenderPass.handle, 0);
			distanceFieldShader.AddColorBlendAttachmentState();
			distanceFieldShader.CreatePipeline(renderingDevice);
		}
	}
	
	void DestroyPipelines(Renderer* renderer, Device* renderingDevice) {
		planetShader.DestroyPipeline(renderingDevice);
		distanceFieldShader.DestroyPipeline(renderingDevice);
		planetPipelineLayout.Destroy(renderingDevice);
		distanceFieldRenderPass.DestroyFrameBuffers(renderingDevice);
		distanceFieldRenderPass.Destroy(renderingDevice);
	}
	
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) {
		
	}
	
	void RunDynamic(Device* renderingDevice, VkCommandBuffer commandBuffer) {
		//TODO For each planet
		
		planetPushConstant.planetRadius = 24000000;
		planetPushConstant.planetAbsolutePosition = glm::dvec3(0, 21800000, -10000000);
		
		distanceFieldRenderPass.Begin(renderingDevice, commandBuffer, distanceFieldImage, {{.0,.0,.0,.0}});
		distanceFieldShader.Execute(renderingDevice, commandBuffer, &planetPushConstant);
		distanceFieldRenderPass.End(renderingDevice, commandBuffer);
	}
	
	void RunInOpaqueLightingPass(Device* renderingDevice, VkCommandBuffer commandBuffer) {
		//TODO For each planet
		
		planetPushConstant.planetRadius = 24000000;
		planetPushConstant.planetAbsolutePosition = glm::dvec3(0, 21800000, -10000000);
		
		planetShader.Execute(renderingDevice, commandBuffer, &planetPushConstant);
	}
	
	void RunLowPriorityGraphics(Device* renderingDevice, VkCommandBuffer commandBuffer) {
		
	}
	
	void FrameUpdate(Renderer* renderer, Device* renderingDevice, Camera& mainCamera) {
		LOG(glm::distance(planetPushConstant.planetAbsolutePosition, mainCamera.GetWorldPosition()) - planetPushConstant.planetRadius);
	}
	
	void LowPriorityFrameUpdate(Renderer* renderer, Device* renderingDevice, Camera& mainCamera, Queue& lowPriorityGraphicsQueue) {
		
	}
	
	
	
	
};
