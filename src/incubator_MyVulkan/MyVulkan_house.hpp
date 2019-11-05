#pragma once

#include "MyVulkanRenderer.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "Texture2D.hpp"

// Test Object Vertex Data Structure
struct Vertex {
	glm::vec3 pos;
	glm::vec4 color;
	glm::vec2 uv;
	
	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && uv == other.uv;
	}
};
namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const &vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^
					(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
					(hash<glm::vec2>()(vertex.uv) << 1);
		}
	};
}

class MyVulkanTest : public MyVulkanRenderer {
	using MyVulkanRenderer::MyVulkanRenderer;
	
	VulkanShaderProgram* testShader;
	// Vertex Buffers for test object
	std::vector<Vertex> testObjectVertices;
	std::vector<uint32_t> testObjectIndices;
	std::unordered_map<Vertex, uint32_t> testObjectUniqueVertices;
	VkBuffer vertexBuffer, indexBuffer;
	VkDeviceMemory vertexBufferMemory, indexBufferMemory;
	struct UBO {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};
	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<VkDescriptorSet> descriptorSets;
	
	Texture2D* texture;
	
	void Init() override {
		clearColor = {0,0,0,1};
	}

	using MyVulkanRenderer::GenerateMipmaps;
	void GenerateMipmaps(Texture2D* texture) {
		GenerateMipmaps(texture->GetImage(), texture->GetFormat(), texture->GetWidth(), texture->GetHeight(), texture->GetMipLevels());
	}

	void LoadScene() override {
		// 3D Model
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err, warn;
		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "incubator_MyVulkan/assets/models/chalet.obj")) {
			throw std::runtime_error(err);
		}
		if (warn != "") LOG_WARN(warn);
		for (const auto &shape : shapes) {
			for (const auto &index : shape.mesh.indices) {
				Vertex vertex = {};
				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2],
				};
				vertex.uv = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1], // flipped vertical component
				};
				vertex.color = {1, 1, 1, 1};
				if (testObjectUniqueVertices.count(vertex) == 0) {
					testObjectUniqueVertices[vertex] = testObjectVertices.size();
					testObjectVertices.push_back(vertex);
				}
				testObjectIndices.push_back(testObjectUniqueVertices[vertex]);
			}
		}

		// Texture
		texture = new Texture2D("incubator_MyVulkan/assets/models/chalet.jpg", STBI_rgb_alpha);
		texture->SetMipLevels();
		texture->SetSamplerAnisotropy(16);
		texture->AllocateVulkanStagingMemory(renderingDevice);
		texture->CreateVulkanImage(renderingDevice, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		texture->CreateVulkanImageView(renderingDevice);
		texture->CreateVulkanSampler(renderingDevice);
		TransitionImageLayout(texture->GetImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->GetMipLevels());
		CopyBufferToImage(texture->GetStagingBuffer(), texture->GetImage(), texture->GetWidth(), texture->GetHeight());
		// TransitionImageLayout(texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture->GetMipLevels());
		GenerateMipmaps(texture); // This automatically does the transition to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		texture->FreeVulkanStagingMemory(renderingDevice);

		// Shader program
		testShader = new VulkanShaderProgram(renderingDevice, {
			{"incubator_MyVulkan/assets/shaders/test.vert"},
			{"incubator_MyVulkan/assets/shaders/test.frag"},
		});
		// testShader = new VulkanShaderProgram(renderingDevice, "incubator_MyVulkan/assets/shaders/test");

		// Vertex Input structure
		testShader->AddVertexInputBinding(sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX /*VK_VERTEX_INPUT_RATE_INSTANCE*/, {
			{0, offsetof(Vertex, Vertex::pos), VK_FORMAT_R32G32B32_SFLOAT},
			{1, offsetof(Vertex, Vertex::color), VK_FORMAT_R32G32B32A32_SFLOAT},
			{2, offsetof(Vertex, Vertex::uv), VK_FORMAT_R32G32_SFLOAT},
		});

		// Uniforms
		testShader->AddLayoutBindings({
			{// ubo
				0, // binding
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // descriptorType
				1, // count (for array)
				VK_SHADER_STAGE_VERTEX_BIT, // VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_ALL_GRAPHICS, ...
				nullptr, // pImmutableSamplers
			},
			{// tex
				1, // binding
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // descriptorType
				1, // count (for array)
				VK_SHADER_STAGE_FRAGMENT_BIT, // VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_ALL_GRAPHICS, ...
				nullptr, // pImmutableSamplers
			},
		});
		
	}

	void UnloadScene() override {
		texture->DestroyVulkanSampler(renderingDevice);
		texture->DestroyVulkanImageView(renderingDevice);
		texture->DestroyVulkanImage(renderingDevice);
		delete texture;
		delete testShader;
	}

	void SendSceneToDevice() override {
		// Vertices
		VkDeviceSize bufferSize = sizeof(testObjectVertices[0]) * testObjectVertices.size();
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
		void *data;
		renderingDevice->MapMemory(stagingBufferMemory, 0, bufferSize, 0, &data);
			memcpy(data, testObjectVertices.data(), bufferSize);
		renderingDevice->UnmapMemory(stagingBufferMemory);
		renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
		CopyBuffer(stagingBuffer, vertexBuffer, bufferSize);
		renderingDevice->DestroyBuffer(stagingBuffer, nullptr);
		renderingDevice->FreeMemory(stagingBufferMemory, nullptr);

		// Indices
		/*VkDeviceSize*/ bufferSize = sizeof(testObjectIndices[0]) * testObjectIndices.size();
		// VkBuffer stagingBuffer;
		// VkDeviceMemory stagingBufferMemory;
		renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
		// void *data;
		renderingDevice->MapMemory(stagingBufferMemory, 0, bufferSize, 0, &data);
			memcpy(data, testObjectIndices.data(), bufferSize);
		renderingDevice->UnmapMemory(stagingBufferMemory);
		renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
		CopyBuffer(stagingBuffer, indexBuffer, bufferSize);
		renderingDevice->DestroyBuffer(stagingBuffer, nullptr);
		renderingDevice->FreeMemory(stagingBufferMemory, nullptr);

		// Uniform buffers
		/*VkDeviceSize*/ bufferSize = sizeof(UBO);
		uniformBuffers.resize(swapChain->imageViews.size());
		uniformBuffersMemory.resize(uniformBuffers.size());
		for (size_t i = 0; i < uniformBuffers.size(); i++) {
			renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
		}

		// Descriptor sets (for Uniform Buffers)
		std::vector<VkDescriptorSetLayout> layouts;
		layouts.reserve(swapChain->imageViews.size() * testShader->GetDescriptorSetLayouts().size());
		for (size_t i = 0; i < swapChain->imageViews.size(); i++) {
			for (auto layout : testShader->GetDescriptorSetLayouts()) {
				layouts.push_back(layout);
			}
		}
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = layouts.size();
		allocInfo.pSetLayouts = layouts.data();
		descriptorSets.resize(swapChain->imageViews.size());
		if (renderingDevice->AllocateDescriptorSets(&allocInfo, descriptorSets.data()) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate descriptor sets");
		}
		for (size_t i = 0; i < swapChain->imageViews.size(); i++) {
			std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

			// ubo
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = descriptorSets[i];
			descriptorWrites[0].dstBinding = 0; // layout(binding = 0) uniform...
			descriptorWrites[0].dstArrayElement = 0; // array
			descriptorWrites[0].descriptorCount = 1; // array
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			VkDescriptorBufferInfo bufferInfo = {uniformBuffers[i], 0, sizeof(UBO)};
			descriptorWrites[0].pBufferInfo = &bufferInfo;

			// tex
			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = descriptorSets[i];
			descriptorWrites[1].dstBinding = 1; // layout(binding = 0) uniform...
			descriptorWrites[1].dstArrayElement = 0; // array
			descriptorWrites[1].descriptorCount = 1; // array
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			VkDescriptorImageInfo imageInfo = {texture->GetSampler(), texture->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
			descriptorWrites[1].pImageInfo = &imageInfo;
			
			// Update Descriptor Sets
			renderingDevice->UpdateDescriptorSets(descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
		}
	}

	void DeleteSceneFromDevice() override {
		// Uniform buffers
		for (size_t i = 0; i < uniformBuffers.size(); i++) {
			renderingDevice->DestroyBuffer(uniformBuffers[i], nullptr);
			renderingDevice->FreeMemory(uniformBuffersMemory[i], nullptr);
		}

		// Descriptor Sets
		renderingDevice->FreeDescriptorSets(descriptorPool, descriptorSets.size(), descriptorSets.data());

		// Vertices
		renderingDevice->DestroyBuffer(vertexBuffer, nullptr);
		renderingDevice->FreeMemory(vertexBufferMemory, nullptr);

		// Indices
		renderingDevice->DestroyBuffer(indexBuffer, nullptr);
		renderingDevice->FreeMemory(indexBufferMemory, nullptr);
	}
	
	void ConfigureRayTracingPipelines() override {
		
	}
	
	void ConfigureGraphicsPipelines() override {
		auto* graphicsPipeline1 = renderPass->NewGraphicsPipeline(renderingDevice, 0);
		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		
		// Shader stages (programmable stages of the graphics pipeline)
		
		graphicsPipeline1->SetShaderProgram(testShader);

		// Input Assembly
		graphicsPipeline1->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		graphicsPipeline1->inputAssembly.primitiveRestartEnable = VK_FALSE; // If set to VK_TRUE, then it's possible to break up lines and triangles in the _STRIP topology modes by using a special index of 0xFFFF or 0xFFFFFFFF.

		// Color Blending
		graphicsPipeline1->AddAlphaBlendingAttachment(); // Fragment Shader output 0

		
		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Fixed-function state
		// Global graphics settings when starting the game

		// Rasterizer
		graphicsPipeline1->rasterizer.depthClampEnable = VK_FALSE; // if set to VK_TRUE, then fragments that are beyond the near and far planes are clamped to them as opposed to discarding them. This is useful in some special cases like shadow maps. Using this requires enabling a GPU feature.
		graphicsPipeline1->rasterizer.rasterizerDiscardEnable = VK_FALSE; // if set to VK_TRUE, then geometry never passes through the rasterizer stage. This basically disables any output to the framebuffer.
		graphicsPipeline1->rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // Using any mode other than fill requires enabling a GPU feature.
		graphicsPipeline1->rasterizer.lineWidth = 1; // The maximum line width that is supported depends on the hardware and any line thicker than 1.0f requires you to enable the wideLines GPU feature.
		graphicsPipeline1->rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; // Face culling
		graphicsPipeline1->rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Vertex faces draw order
		graphicsPipeline1->rasterizer.depthBiasEnable = VK_FALSE;
		graphicsPipeline1->rasterizer.depthBiasConstantFactor = 0;
		graphicsPipeline1->rasterizer.depthBiasClamp = 0;
		graphicsPipeline1->rasterizer.depthBiasSlopeFactor = 0;

		// Multisampling (AntiAliasing)
		graphicsPipeline1->multisampling.sampleShadingEnable = VK_TRUE;
		graphicsPipeline1->multisampling.minSampleShading = 0.2f; // Min fraction for sample shading (0-1). Closer to one is smoother.
		graphicsPipeline1->multisampling.rasterizationSamples = msaaSamples;
		graphicsPipeline1->multisampling.pSampleMask = nullptr;
		graphicsPipeline1->multisampling.alphaToCoverageEnable = VK_FALSE;
		graphicsPipeline1->multisampling.alphaToOneEnable = VK_FALSE;

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Dynamic settings that CAN be changed at runtime but NOT every frame
		graphicsPipeline1->dynamicStates = {};

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Create DAS GRAFIKS PIPELINE !!!
		// Fixed-function stage
		graphicsPipeline1->pipelineCreateInfo.pViewportState = &swapChain->viewportState;
		
		// Depth stencil
		graphicsPipeline1->depthStencilState.depthTestEnable = VK_TRUE;
		graphicsPipeline1->depthStencilState.depthWriteEnable = VK_TRUE;
		graphicsPipeline1->depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
		// Used for the optional depth bound test. This allows to only keep fragments that fall within the specified depth range.
		graphicsPipeline1->depthStencilState.depthBoundsTestEnable = VK_FALSE;
			// graphicsPipeline1->depthStencilState.minDepthBounds = 0.0f;
			// graphicsPipeline1->depthStencilState.maxDepthBounds = 1.0f;
		// Stencil Buffer operations
		graphicsPipeline1->depthStencilState.stencilTestEnable = VK_FALSE;
		graphicsPipeline1->depthStencilState.front = {};
		graphicsPipeline1->depthStencilState.back = {};

		
		// Optional.
		// Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline. 
		// The idea of pipeline derivatives is that it is less expensive to set up pipelines when they have much functionality in common with an existing pipeline and switching between pipelines from the same parent can also be done quicker.
		// You can either specify the handle of an existing pipeline with basePipelineHandle or reference another pipeline that is about to be created by index with basePipelineIndex. 
		// These are only used if VK_PIPELINE_CREATE_DERIVATIVE_BIT flag is also specified in the flags field of VkGraphicsPipelineCreateInfo.
		graphicsPipeline1->pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		graphicsPipeline1->pipelineCreateInfo.basePipelineIndex = -1;
	}

	void ConfigureRayTracingCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		
	}

	void ConfigureCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		
		// We can now bind the graphics pipeline
		renderingDevice->CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPass->graphicsPipelines[0]->handle); // The second parameter specifies if the pipeline object is a graphics or compute pipeline.
		
		// Bind Descriptor Sets (Uniforms)
		renderingDevice->CmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPass->graphicsPipelines[0]->pipelineLayout, 0/*firstSet*/, 1/*count*/, &descriptorSets[imageIndex], 0, nullptr);


		// Test Object
		VkBuffer vertexBuffers[] = {vertexBuffer};
		VkDeviceSize offsets[] = {0};
		renderingDevice->CmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		// renderingDevice->CmdDraw(commandBuffer,
		// 	static_cast<uint32_t>(testObjectVertices.size()), // vertexCount
		// 	1, // instanceCount
		// 	0, // firstVertex (defines the lowest value of gl_VertexIndex)
		// 	0  // firstInstance (defines the lowest value of gl_InstanceIndex)
		// );
		renderingDevice->CmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		renderingDevice->CmdDrawIndexed(commandBuffer,
			static_cast<uint32_t>(testObjectIndices.size()), // indexCount
			1, // instanceCount
			0, // firstVertex (defines the lowest value of gl_VertexIndex)
			0, // vertexOffset
			0  // firstInstance (defines the lowest value of gl_InstanceIndex)
		);
		
	}

	void FrameUpdate(uint imageIndex) override {
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		UBO ubo = {};
		// Slowly rotate the test object
		ubo.model = glm::rotate(glm::mat4(1), time * glm::radians(10.0f), glm::vec3(0,0,1));
		// Current camera position
		ubo.view = glm::lookAt(glm::vec3(2,2,2), glm::vec3(0,0,0), glm::vec3(0,0,1));
		// Projection
		ubo.proj = glm::perspective(glm::radians(45.0f), (float) swapChain->extent.width / (float) swapChain->extent.height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;
		// Update memory
		void *data;
		renderingDevice->MapMemory(uniformBuffersMemory[imageIndex], 0/*offset*/, sizeof(ubo), 0/*flags*/, &data);
			memcpy(data, &ubo, sizeof(ubo));
		renderingDevice->UnmapMemory(uniformBuffersMemory[imageIndex]);
	}

	
};
