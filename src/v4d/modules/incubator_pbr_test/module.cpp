// Metadata
#define _V4D_MODULE
#define THIS_MODULE_VENDOR_ID 16
#define THIS_MODULE_ID 102
#define THIS_MODULE_REV 1
#define THIS_MODULE_NAME "PBR Test"
#define THIS_MODULE_DESCRIPTION ""

// V4D Core Header
#include <v4d.h>

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

class PbrShaderPipeline : public RasterShaderPipeline {
public:
	using RasterShaderPipeline::RasterShaderPipeline;
	using RasterShaderPipeline::Execute;
	void Execute(Device* device, VkCommandBuffer cmdBuffer) override {
		Bind(device, cmdBuffer);
		Render(device, cmdBuffer, 4);
	}
};

struct Vertex {
	alignas(16) glm::vec3 pos;
	alignas(16) glm::vec3 normal;
	
	Vertex(glm::vec3 pos, glm::vec3 normal) : pos(pos), normal(normal) {}
	
	static std::vector<VertexInputAttributeDescription> GetInputAttributes() {
		return {
			{0, offsetof(Vertex, pos), VK_FORMAT_R32G32B32A32_SFLOAT},
			{1, offsetof(Vertex, normal), VK_FORMAT_R32G32B32A32_SFLOAT},
		};
	}
};

struct PbrRenderer : v4d::modules::Rendering {
	
	PipelineLayout pipelineLayout;
	PbrShaderPipeline pbrObjectShader {pipelineLayout, {
		"modules/incubator_pbr_test/assets/shaders/test.vert",
		"modules/incubator_pbr_test/assets/shaders/test.surface.frag",
	}};
	
	Buffer vertexBuffer { VK_BUFFER_USAGE_VERTEX_BUFFER_BIT };
	Buffer indexBuffer { VK_BUFFER_USAGE_INDEX_BUFFER_BIT };
	
	std::array<Vertex, 24> vertices {
		
		// front
		Vertex{/*pos*/{-1,-1,-1}, /*normal*/{0,-1,0}},
		Vertex{/*pos*/{ 1,-1,-1}, /*normal*/{0,-1,0}},
		Vertex{/*pos*/{ 1,-1, 1}, /*normal*/{0,-1,0}},
		Vertex{/*pos*/{-1,-1, 1}, /*normal*/{0,-1,0}},
		
		// back
		Vertex{/*pos*/{-1, 1,-1}, /*normal*/{0,1,0}},
		Vertex{/*pos*/{ 1, 1,-1}, /*normal*/{0,1,0}},
		Vertex{/*pos*/{ 1, 1, 1}, /*normal*/{0,1,0}},
		Vertex{/*pos*/{-1, 1, 1}, /*normal*/{0,1,0}},

		// right
		Vertex{/*pos*/{ 1,-1,-1}, /*normal*/{1,0,0}},
		Vertex{/*pos*/{ 1, 1,-1}, /*normal*/{1,0,0}},
		Vertex{/*pos*/{ 1, 1, 1}, /*normal*/{1,0,0}},
		Vertex{/*pos*/{ 1,-1, 1}, /*normal*/{1,0,0}},

		// left
		Vertex{/*pos*/{-1,-1,-1}, /*normal*/{-1,0,0}},
		Vertex{/*pos*/{-1, 1,-1}, /*normal*/{-1,0,0}},
		Vertex{/*pos*/{-1, 1, 1}, /*normal*/{-1,0,0}},
		Vertex{/*pos*/{-1,-1, 1}, /*normal*/{-1,0,0}},
		
		// top
		Vertex{/*pos*/{-1,-1, 1}, /*normal*/{0,0,1}},
		Vertex{/*pos*/{ 1,-1, 1}, /*normal*/{0,0,1}},
		Vertex{/*pos*/{ 1, 1, 1}, /*normal*/{0,0,1}},
		Vertex{/*pos*/{-1, 1, 1}, /*normal*/{0,0,1}},

		// bottom
		Vertex{/*pos*/{-1,-1,-1}, /*normal*/{0,0,-1}},
		Vertex{/*pos*/{ 1,-1,-1}, /*normal*/{0,0,-1}},
		Vertex{/*pos*/{ 1, 1,-1}, /*normal*/{0,0,-1}},
		Vertex{/*pos*/{-1, 1,-1}, /*normal*/{0,0,-1}},
		
	};
	
	std::array<uint32_t, 36> indices {
		0,1,2,  2,3,0, // front
		4,6,5,  4,7,6, // back
		8,9,10, 10,11,8, // right
		12,14,13, 12,15,14, // left
		16,17,18, 18,19,16, // top
		20,22,21, 20,23,22, // bottom
	};
	
	void Init() override {
		vertexBuffer.AddSrcDataPtr(vertices.data(), vertices.size() * sizeof(Vertex));
		indexBuffer.AddSrcDataPtr(indices.data(), indices.size() * sizeof(uint32_t));
	}
	
	void InitLayouts(std::map<std::string, DescriptorSet*>& descriptorSets, std::unordered_map<std::string, Image*>& images, PipelineLayout*) override {
		pipelineLayout.AddDescriptorSet(descriptorSets["base"]);
	}
	
	void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders, v4d::graphics::vulkan::rtx::ShaderBindingTable*) override {
		shaders["opaqueRasterization"].push_back(&pbrObjectShader);
		pbrObjectShader.AddVertexInputBinding(sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX, Vertex::GetInputAttributes());
		pbrObjectShader.SetData(&vertexBuffer, &indexBuffer, indices.size());
	}
	
	void CreatePipelines(std::unordered_map<std::string, Image*>& images) override {
		pipelineLayout.Create(renderingDevice);
	}
	
	void DestroyPipelines() override {
		pipelineLayout.Destroy(renderingDevice);
	}
	
	void AllocateBuffers() override {
		vertexBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true);
		indexBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true);
	}
	
	void FreeBuffers() override {
		vertexBuffer.Free(renderingDevice);
		indexBuffer.Free(renderingDevice);
	}
	
};

V4DMODULE void V4D_ModuleCreate() {
	V4D_LOAD_SUBMODULE(PbrRenderer)
}

V4DMODULE void V4D_ModuleDestroy() {
	V4D_UNLOAD_SUBMODULES(PbrRenderer)
}
