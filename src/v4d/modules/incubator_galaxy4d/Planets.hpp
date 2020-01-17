#pragma once
#include <v4d.h>

#include "../../incubator_galaxy4d/Noise.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

// #define SPHERIFY_CUBE_BY_NORMALIZE // otherwise use technique shown here : http://mathproofs.blogspot.com/2005/07/mapping-cube-to-sphere.html

class PlanetShaderPipeline : public RasterShaderPipeline {
public:
	using RasterShaderPipeline::RasterShaderPipeline;
	
	glm::dmat4 viewMatrix {1};

	struct PlanetChunkPushConstant { // max 128 bytes
		glm::mat4 modelView; // 64
		glm::vec4 testColor; // 16
	} planetChunkPushConstant {};
	
	using RasterShaderPipeline::Execute;
	void Execute(Device* device, VkCommandBuffer cmdBuffer) override {
		Bind(device, cmdBuffer);
		
		//TODO For Each Planet
			//TODO For Each Chunk
				
				glm::dvec3 planetAbsolutePosition {0};
				glm::dvec3 chunkCenterPos {0};
				planetChunkPushConstant.modelView = viewMatrix * glm::translate(glm::dmat4(1), planetAbsolutePosition + chunkCenterPos);
				planetChunkPushConstant.testColor = glm::vec4(1,1,1,1);
				PushConstant(device, cmdBuffer, &planetChunkPushConstant);
				SetData(3);
				Render(device, cmdBuffer, 1);
	}

};

class Planets : public v4d::modules::Rendering {
	
	#pragma region Graphics
	PipelineLayout planetPipelineLayout;
	PlanetShaderPipeline planetShader {planetPipelineLayout, {
		"modules/incubator_galaxy4d/assets/shaders/planets.vert",
		"modules/incubator_galaxy4d/assets/shaders/planets.wireframe.geom",
		"modules/incubator_galaxy4d/assets/shaders/planets.surface.frag",
	}};
	#pragma endregion
	
	#pragma region Static graphics configuration
	static const int chunkSubdivisionsPerFace = 8;
	static const int vertexSubdivisionsPerChunk = 32;
	static constexpr float targetVertexSeparationInMeters = 1.0; // approximative vertex separation in meters for the most precise level of detail
	#pragma endregion
	
	#pragma region Calculated constants
	static const int nbChunksPerFace = chunkSubdivisionsPerFace * chunkSubdivisionsPerFace;
	static const int nbBaseChunksPerPlanet = nbChunksPerFace * 6;
	static const int nbVerticesPerChunk = (vertexSubdivisionsPerChunk+1) * (vertexSubdivisionsPerChunk+1);
	static const int nbIndicesPerChunk = (vertexSubdivisionsPerChunk) * (vertexSubdivisionsPerChunk) * 6;
	#pragma endregion
	
	struct Vertex {
		glm::vec4 pos;
		glm::vec4 normal;
	};

	enum FACE : int {
		FRONT,
		BACK,
		RIGHT,
		LEFT,
		TOP,
		BOTTOM
	};
	
public:

	// // Executed when calling InitRenderer() on the main Renderer
	// void Init() override {}
	
	void InitLayouts(std::vector<DescriptorSet*>& descriptorSets) override {
		auto* planetDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		planetPipelineLayout.AddDescriptorSet(descriptorSets[0]);
		planetPipelineLayout.AddDescriptorSet(planetDescriptorSet_1);
		planetPipelineLayout.AddPushConstant<PlanetShaderPipeline::PlanetChunkPushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders) override {
		shaders["opaqueRasterization"].push_back(&planetShader);
		
		planetShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
		planetShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		planetShader.depthStencilState.depthWriteEnable = VK_FALSE;
		planetShader.depthStencilState.depthTestEnable = VK_FALSE;
		// planetShader.AddVertexInputBinding(sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX, {
		// 	{0, offsetof(Vertex, Vertex::pos), VK_FORMAT_R32G32B32A32_SFLOAT},
		// 	{1, offsetof(Vertex, Vertex::normal), VK_FORMAT_R32G32B32A32_SFLOAT},
		// });
	}
	
	// // Executed when calling their respective methods on the main Renderer
	// void ReadShaders() override {}
	// void LoadScene() override {}
	// void UnloadScene() override {}
	
	// // Executed when calling LoadRenderer()
	// void ScorePhysicalDeviceSelection(int& score, PhysicalDevice*) override {}
	// // after selecting rendering device and queues
	// void Info() override {}
	// void CreateResources() {}
	// void DestroyResources() override {}
	// void AllocateBuffers() override {}
	// void FreeBuffers() override {}
	
	void CreatePipelines() override {
		planetPipelineLayout.Create(renderingDevice);
	}
	void DestroyPipelines() override {
		planetPipelineLayout.Destroy(renderingDevice);
	}
	
	// // Rendering methods potentially executed on each frame
	// void RunDynamicGraphicsTop(VkCommandBuffer) override {}
	// void RunDynamicGraphicsBottom(VkCommandBuffer) override {}
	// void RunDynamicLowPriorityCompute(VkCommandBuffer) override {}
	// void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {}
	
	// Executed before each frame
	void FrameUpdate(uint imageIndex, glm::dmat4& projection, glm::dmat4& view) override {
		planetShader.viewMatrix = view;
	}
	void LowPriorityFrameUpdate() override {
		
	}
	
};
