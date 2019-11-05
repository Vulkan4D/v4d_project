#pragma once

#include "MyVulkanRenderer.hpp"

// Test Object Vertex Data Structure
struct Vertex {
	glm::vec3 pos;
	glm::vec4 color;
	
	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color;
	}
};
namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const &vertex) const {
			return (hash<glm::vec3>()(vertex.pos) ^
					(hash<glm::vec3>()(vertex.color) << 1));
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
	VkBuffer vertexBuffer, indexBuffer, instanceBuffer;
	VkDeviceMemory vertexBufferMemory, indexBufferMemory, instanceBufferMemory;
	
	struct UBO {
		glm::mat4 viewInverse;
		glm::mat4 projInverse;
	};
	
	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<VkDescriptorSet> descriptorSets;
	
	// Ray tracing geometry instance
	struct GeometryInstance {
		glm::mat3x4 transform;
		uint32_t instanceId : 24;
		uint32_t mask : 8;
		uint32_t instanceOffset : 24;
		uint32_t flags : 8;
		uint64_t accelerationStructureHandle;
	};

	struct StorageImage {
		VkDeviceMemory memory;
		VkImage image;
		VkImageView view;
		VkFormat format;
	} storageImage;

	// Ray Tracing
	std::vector<VkGeometryNV> testObjectGeometries;
	std::vector<GeometryInstance> testObjectGeometryInstances;
	
	void Init() override {
		clearColor = {0,0,0,1};
		useRayTracing = true;
	}

	void LoadScene() override {

		testObjectVertices = {
			{{-0.5f,-0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1}},
			{{ 0.5f,-0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1}},
			{{ 0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1}},
			{{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 1.0f, 1}},
			//
			{{-0.5f,-0.5f,-0.5f}, {1.0f, 0.0f, 0.0f, 1}},
			{{ 0.5f,-0.5f,-0.5f}, {0.0f, 1.0f, 0.0f, 1}},
			{{ 0.5f, 0.5f,-0.5f}, {0.0f, 0.0f, 1.0f, 1}},
			{{-0.5f, 0.5f,-0.5f}, {0.0f, 1.0f, 1.0f, 1}},
		};
		testObjectIndices = {
			0, 1, 2, 2, 3, 0,
			4, 5, 6, 6, 7, 4,
		};

		if (useRayTracing) {
			testShader = new VulkanShaderProgram(renderingDevice, {
				{"incubator_MyVulkan/assets/shaders/rtx.rgen"},
				{"incubator_MyVulkan/assets/shaders/rtx.rmiss"},
				{"incubator_MyVulkan/assets/shaders/rtx.rchit"},
			});
			testShader->GenerateRayTracingGroups();
			
			// Uniforms
			testShader->AddLayoutBindings({
				{// accelerationStructure
					0, // binding
					VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, // descriptorType
					1,
					VK_SHADER_STAGE_RAYGEN_BIT_NV, // stage flags
					nullptr // pImmutableSamplers
				},
				{// resultImage
					1, // binding
					VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // descriptorType
					1,
					VK_SHADER_STAGE_RAYGEN_BIT_NV, // stage flags
					nullptr // pImmutableSamplers
				},
				{// ubo
					2, // binding
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // descriptorType
					1, // count (for array)
					VK_SHADER_STAGE_RAYGEN_BIT_NV, // stage flags
					nullptr // pImmutableSamplers
				},
			});
		} else {
			// Shader program
			testShader = new VulkanShaderProgram(renderingDevice, {
				{"incubator_MyVulkan/assets/shaders/triangle.vert"},
				{"incubator_MyVulkan/assets/shaders/triangle.frag"},
			});
				
			// Vertex Input structure
			testShader->AddVertexInputBinding(sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX /*VK_VERTEX_INPUT_RATE_INSTANCE*/, {
				{0, offsetof(Vertex, Vertex::pos), VK_FORMAT_R32G32B32_SFLOAT},
				{1, offsetof(Vertex, Vertex::color), VK_FORMAT_R32G32B32A32_SFLOAT},
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
			});
		}

		
		if (useRayTracing) {
			////////////////////////////////////////////
			// RTX
			
			// Geometries for bottom level acceleration structure
			VkGeometryNV geometry {};
			geometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
			geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
			geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
			geometry.geometry.triangles.vertexData = vertexBuffer;
			geometry.geometry.triangles.vertexOffset = 0;
			geometry.geometry.triangles.vertexCount = (uint)testObjectVertices.size();
			geometry.geometry.triangles.vertexStride = sizeof(Vertex);
			geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
			geometry.geometry.triangles.indexData = indexBuffer;
			geometry.geometry.triangles.indexOffset = 0;
			geometry.geometry.triangles.indexCount = (uint)testObjectIndices.size();
			geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
			geometry.geometry.triangles.transformData = VK_NULL_HANDLE;
			geometry.geometry.triangles.transformOffset = 0;
			geometry.geometry.aabbs = {};
			geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
			geometry.flags = VK_GEOMETRY_OPAQUE_BIT_NV;
			testObjectGeometries.push_back(geometry);
			
			{
				// Bottom Level acceleration structure
				VkAccelerationStructureCreateInfoNV accStructCreateInfo {
					VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV,
					nullptr,// pNext
					0,// VkDeviceSize compactedSize
					{// VkAccelerationStructureInfoNV info
						VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV,
						nullptr,// pNext
						VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV,// type
						0,// flags
						0,// instanceCount
						(uint)testObjectGeometries.size(),// geometryCount
						testObjectGeometries.data()// VkGeometryNV pGeometries
					}
				};
				if (renderingDevice->CreateAccelerationStructureNV(&accStructCreateInfo, nullptr, &rayTracingBottomLevelAccelerationStructure) != VK_SUCCESS)
					throw std::runtime_error("Failed to create bottom level acceleration structure");
					
				VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
				memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
				memoryRequirementsInfo.accelerationStructure = rayTracingBottomLevelAccelerationStructure;
					
				VkMemoryRequirements2 memoryRequirements2;
				renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirements2);
				
				VkMemoryAllocateInfo memoryAllocateInfo {
					VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					nullptr,// pNext
					memoryRequirements2.memoryRequirements.size,// VkDeviceSize allocationSize
					renderingDevice->GetGPU()->FindMemoryType(memoryRequirements2.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)// memoryTypeIndex
				};
				if (renderingDevice->AllocateMemory(&memoryAllocateInfo, nullptr, &rayTracingBottomLevelAccelerationStructureMemory) != VK_SUCCESS)
					throw std::runtime_error("Failed to allocate memory for bottom level acceleration structure");
				
				VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo {
					VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV,
					nullptr,// pNext
					rayTracingBottomLevelAccelerationStructure,// accelerationStructure
					rayTracingBottomLevelAccelerationStructureMemory,// memory
					0,// VkDeviceSize memoryOffset
					0,// uint32_t deviceIndexCount
					nullptr// const uint32_t* pDeviceIndices
				};
				if (renderingDevice->BindAccelerationStructureMemoryNV(1, &accelerationStructureMemoryInfo) != VK_SUCCESS)
					throw std::runtime_error("Failed to bind bottom level acceleration structure memory");
				
				if (renderingDevice->GetAccelerationStructureHandleNV(rayTracingBottomLevelAccelerationStructure, sizeof(uint64_t), &rayTracingBottomLevelAccelerationStructureHandle))
					throw std::runtime_error("Failed to get bottom level acceleration structure handle");
			}
			
			
			// Geometry instances for top level acceleration structure
			glm::mat3x4 transform {
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f
			};
			testObjectGeometryInstances.push_back({
				transform,
				0, // instanceId
				0xff, // mask
				0, // instanceOffset
				VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV, // flags
				rayTracingBottomLevelAccelerationStructureHandle // accelerationStructureHandle
			});
			
			{
				// Top Level acceleration structure
				VkAccelerationStructureCreateInfoNV accStructCreateInfo {
					VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV,
					nullptr,// pNext
					0,// VkDeviceSize compactedSize
					{// VkAccelerationStructureInfoNV info
						VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV,
						nullptr,// pNext
						VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV,// type
						0,// flags
						1,// instanceCount
						0,// geometryCount
						nullptr// VkGeometryNV pGeometries
					}
				};
				if (renderingDevice->CreateAccelerationStructureNV(&accStructCreateInfo, nullptr, &rayTracingTopLevelAccelerationStructure) != VK_SUCCESS)
					throw std::runtime_error("Failed to create top level acceleration structure");
				
				VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
				memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
				memoryRequirementsInfo.accelerationStructure = rayTracingTopLevelAccelerationStructure;
					
				VkMemoryRequirements2 memoryRequirements2;
				renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirements2);
				
				VkMemoryAllocateInfo memoryAllocateInfo {
					VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					nullptr,// pNext
					memoryRequirements2.memoryRequirements.size,// VkDeviceSize allocationSize
					renderingDevice->GetGPU()->FindMemoryType(memoryRequirements2.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)// memoryTypeIndex
				};
				if (renderingDevice->AllocateMemory(&memoryAllocateInfo, nullptr, &rayTracingTopLevelAccelerationStructureMemory) != VK_SUCCESS)
					throw std::runtime_error("Failed to allocate memory for top level acceleration structure");
				
				VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo {
					VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV,
					nullptr,// pNext
					rayTracingTopLevelAccelerationStructure,// accelerationStructure
					rayTracingTopLevelAccelerationStructureMemory,// memory
					0,// VkDeviceSize memoryOffset
					0,// uint32_t deviceIndexCount
					nullptr// const uint32_t* pDeviceIndices
				};
				if (renderingDevice->BindAccelerationStructureMemoryNV(1, &accelerationStructureMemoryInfo) != VK_SUCCESS)
					throw std::runtime_error("Failed to bind top level acceleration structure memory");
				
				if (renderingDevice->GetAccelerationStructureHandleNV(rayTracingTopLevelAccelerationStructure, sizeof(uint64_t), &rayTracingTopLevelAccelerationStructureHandle))
					throw std::runtime_error("Failed to get top level acceleration structure handle");
			}
		}
		
	}

	void UnloadScene() override {
		delete testShader;
	}

	void SendSceneToDevice() override {

		// Vertices
		{
			void* data;
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			VkDeviceSize bufferSize = sizeof(Vertex) * testObjectVertices.size();
			renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
			renderingDevice->MapMemory(stagingBufferMemory, 0, bufferSize, 0, &data);
				memcpy(data, testObjectVertices.data(), bufferSize);
			renderingDevice->UnmapMemory(stagingBufferMemory);
			renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
			CopyBuffer(stagingBuffer, vertexBuffer, bufferSize);
			renderingDevice->DestroyBuffer(stagingBuffer, nullptr);
			renderingDevice->FreeMemory(stagingBufferMemory, nullptr);
		}

		// Indices
		{
			void* data;
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			VkDeviceSize bufferSize = sizeof(uint32_t) * testObjectIndices.size();
			renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
			renderingDevice->MapMemory(stagingBufferMemory, 0, bufferSize, 0, &data);
				memcpy(data, testObjectIndices.data(), bufferSize);
			renderingDevice->UnmapMemory(stagingBufferMemory);
			renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
			CopyBuffer(stagingBuffer, indexBuffer, bufferSize);
			renderingDevice->DestroyBuffer(stagingBuffer, nullptr);
			renderingDevice->FreeMemory(stagingBufferMemory, nullptr);
		}
		
		if (useRayTracing) {
			// Ray Tracing Geometry Instances
			{
				// void* data;
				// VkBuffer stagingBuffer;
				// VkDeviceMemory stagingBufferMemory;
				// VkDeviceSize bufferSize = sizeof(GeometryInstance) * testObjectGeometryInstances.size();
				// renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
				// renderingDevice->MapMemory(stagingBufferMemory, 0, bufferSize, 0, &data);
				// 	memcpy(data, testObjectGeometryInstances.data(), bufferSize);
				// renderingDevice->UnmapMemory(stagingBufferMemory);
				// renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, instanceBuffer, instanceBufferMemory);
				// CopyBuffer(stagingBuffer, indexBuffer, bufferSize);
				// renderingDevice->DestroyBuffer(stagingBuffer, nullptr);
				// renderingDevice->FreeMemory(stagingBufferMemory, nullptr);
				
				renderingDevice->CreateBuffer(sizeof(GeometryInstance) * testObjectGeometryInstances.size(), VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instanceBuffer, instanceBufferMemory, testObjectGeometryInstances.data());
			}
			
			// Build Ray Tracing acceleration structures
			{
				VkBuffer scratchBuffer;
				VkDeviceMemory scratchBufferMemory;
				
				// RayTracing Scratch buffer
				{
					VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
					memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
					memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
					VkMemoryRequirements2 memoryRequirementsBottomLevel, memoryRequirementsTopLevel;
					memoryRequirementsInfo.accelerationStructure = rayTracingBottomLevelAccelerationStructure;
					renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirementsBottomLevel);
					memoryRequirementsInfo.accelerationStructure = rayTracingTopLevelAccelerationStructure;
					renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirementsTopLevel);
					// Send scratch buffer
					const VkDeviceSize scratchBufferSize = std::max(memoryRequirementsBottomLevel.memoryRequirements.size, memoryRequirementsTopLevel.memoryRequirements.size);
					renderingDevice->CreateBuffer(scratchBufferSize, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchBufferMemory);
				}
				
				VkMemoryBarrier memoryBarrier {
					VK_STRUCTURE_TYPE_MEMORY_BARRIER,
					nullptr,// pNext
					VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,// VkAccessFlags srcAccessMask
					VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,// VkAccessFlags dstAccessMask
				};
				
				VkAccelerationStructureInfoNV accelerationStructBuildInfo {};
				accelerationStructBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
				
				auto rayTracingCmdBuffer = BeginSingleTimeCommands(graphicsCommandPool);
					
					accelerationStructBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
					accelerationStructBuildInfo.geometryCount = (uint)testObjectGeometries.size();
					accelerationStructBuildInfo.pGeometries = testObjectGeometries.data();
					accelerationStructBuildInfo.instanceCount = 0;
					
					renderingDevice->CmdBuildAccelerationStructureNV(
						rayTracingCmdBuffer, 
						&accelerationStructBuildInfo, 
						VK_NULL_HANDLE, 
						0, 
						VK_FALSE, 
						rayTracingBottomLevelAccelerationStructure, 
						VK_NULL_HANDLE, 
						scratchBuffer, 
						0
					);
					
					renderingDevice->CmdPipelineBarrier(
						rayTracingCmdBuffer, 
						VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
						VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
						0, 
						1, &memoryBarrier, 
						0, 0, 
						0, 0
					);
					
					accelerationStructBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
					accelerationStructBuildInfo.geometryCount = 0;
					accelerationStructBuildInfo.pGeometries = nullptr;
					accelerationStructBuildInfo.instanceCount = 1;
					
					renderingDevice->CmdBuildAccelerationStructureNV(
						rayTracingCmdBuffer, 
						&accelerationStructBuildInfo, 
						instanceBuffer, 
						0, 
						VK_FALSE, 
						rayTracingTopLevelAccelerationStructure, 
						VK_NULL_HANDLE, 
						scratchBuffer, 
						0
					);
					
					renderingDevice->CmdPipelineBarrier(
						rayTracingCmdBuffer, 
						VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
						VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
						0, 
						1, &memoryBarrier, 
						0, 0, 
						0, 0
					);
				
				EndSingleTimeCommands(graphicsCommandPool, rayTracingCmdBuffer);
					
				renderingDevice->DestroyBuffer(scratchBuffer, nullptr);
				renderingDevice->FreeMemory(scratchBufferMemory, nullptr);
			}
			
		}

		// Uniform buffers
		{
			VkDeviceSize bufferSize = sizeof(UBO);
			uniformBuffers.resize(swapChain->imageViews.size());
			uniformBuffersMemory.resize(uniformBuffers.size());
			for (size_t i = 0; i < uniformBuffers.size(); i++) {
				renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
			}
		}
		// Descriptor sets for Uniform Buffers
		std::vector<VkDescriptorSetLayout> layouts;
		layouts.reserve(swapChain->imageViews.size() * testShader->GetDescriptorSetLayouts().size());
		for (size_t i = 0; i < swapChain->imageViews.size(); i++) {
			for (auto layout : testShader->GetDescriptorSetLayouts()) {
				layouts.push_back(layout);
			}
		}
		
		if (useRayTracing) {
			// Descriptor sets for Ray Tracing Acceleration Structure
			VkWriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo {};
			descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
			descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
			descriptorAccelerationStructureInfo.pAccelerationStructures = &rayTracingTopLevelAccelerationStructure;
			
			// Storage image
			VkDescriptorImageInfo storageImageDescriptor{};
			storageImageDescriptor.imageView = storageImage.view;
			storageImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

			// allocate and update descriptor sets
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
				std::array<VkWriteDescriptorSet, 3> descriptorWrites = {};

				// acceleration structure
				descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[0].pNext = &descriptorAccelerationStructureInfo;
				descriptorWrites[0].dstSet = descriptorSets[i];
				descriptorWrites[0].dstBinding = 0; // layout(binding = 0) uniform...
				descriptorWrites[0].dstArrayElement = 0; // array
				descriptorWrites[0].descriptorCount = 1; // array
				descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;

				// result image
				descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[1].dstSet = descriptorSets[i];
				descriptorWrites[1].dstBinding = 1; // layout(binding = 1) uniform...
				descriptorWrites[1].dstArrayElement = 0; // array
				descriptorWrites[1].descriptorCount = 1; // array
				descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				descriptorWrites[1].pImageInfo = &storageImageDescriptor;

				// ubo
				descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[2].dstSet = descriptorSets[i];
				descriptorWrites[2].dstBinding = 2; // layout(binding = 2) uniform...
				descriptorWrites[2].dstArrayElement = 0; // array
				descriptorWrites[2].descriptorCount = 1; // array
				descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				VkDescriptorBufferInfo bufferInfo = {uniformBuffers[i], 0, sizeof(UBO)};
				descriptorWrites[2].pBufferInfo = &bufferInfo;

				// Update Descriptor Sets
				renderingDevice->UpdateDescriptorSets(descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
			}
		} else {
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
				std::array<VkWriteDescriptorSet, 1> descriptorWrites = {};

				// ubo
				descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[0].dstSet = descriptorSets[i];
				descriptorWrites[0].dstBinding = 0; // layout(binding = 0) uniform...
				descriptorWrites[0].dstArrayElement = 0; // array
				descriptorWrites[0].descriptorCount = 1; // array
				descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				VkDescriptorBufferInfo bufferInfo = {uniformBuffers[i], 0, sizeof(UBO)};
				descriptorWrites[0].pBufferInfo = &bufferInfo;

				// Update Descriptor Sets
				renderingDevice->UpdateDescriptorSets(descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
			}
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
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = (uint)testShader->GetDescriptorSetLayouts().size();
		pipelineLayoutCreateInfo.pSetLayouts = testShader->GetDescriptorSetLayouts().data();
		
		if (renderingDevice->CreatePipelineLayout(&pipelineLayoutCreateInfo, nullptr, &rayTracingPipelineLayout) != VK_SUCCESS)
			throw std::runtime_error("Failed to create ray tracing pipeline layout");
			
		VkRayTracingPipelineCreateInfoNV rayTracingPipelineInfo {};
		rayTracingPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
		rayTracingPipelineInfo.stageCount = (uint)testShader->GetStages().size();
		rayTracingPipelineInfo.pStages = testShader->GetStages().data();
		rayTracingPipelineInfo.groupCount = (uint)testShader->GetRayTracingGroups().size();
		rayTracingPipelineInfo.pGroups = testShader->GetRayTracingGroups().data();
		rayTracingPipelineInfo.maxRecursionDepth = 1;
		rayTracingPipelineInfo.layout = rayTracingPipelineLayout;
		
		if (renderingDevice->CreateRayTracingPipelinesNV(VK_NULL_HANDLE, 1, &rayTracingPipelineInfo, nullptr, &rayTracingPipeline) != VK_SUCCESS) //TODO support multiple ray tracing pipelines
			throw std::runtime_error("Failed to create ray tracing pipelines");
		
		// Shader Binding Table
		const uint32_t sbtSize = rayTracingProperties.shaderGroupHandleSize * testShader->GetRayTracingGroups().size();
		renderingDevice->CreateBuffer(sbtSize, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, rayTracingShaderBindingTableBuffer, rayTracingShaderBindingTableBufferMemory);
		uint8_t* data;
		renderingDevice->MapMemory(rayTracingShaderBindingTableBufferMemory, 0, sbtSize, 0, (void**)&data);
		auto shaderHandleStorage = new uint8_t[sbtSize];
		if (!renderingDevice->GetRayTracingShaderGroupHandlesNV(rayTracingPipeline, 0, (uint)testShader->GetRayTracingGroups().size(), sbtSize, shaderHandleStorage))
			throw std::runtime_error("Failed to get ray tracing shader group handles");
		data += CopyShaderIdentifier(data, shaderHandleStorage, testShader->RAYTRACING_GROUP_INDEX_RGEN);
		data += CopyShaderIdentifier(data, shaderHandleStorage, testShader->RAYTRACING_GROUP_INDEX_RMISS);
		data += CopyShaderIdentifier(data, shaderHandleStorage, testShader->RAYTRACING_GROUP_INDEX_RCHIT);
		renderingDevice->UnmapMemory(rayTracingShaderBindingTableBufferMemory);
		
	}
	
	void ConfigureGraphicsPipelines() override { // This method is not called when using Ray Tracing
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
		
		renderingDevice->CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, rayTracingPipeline);
		renderingDevice->CmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, rayTracingPipelineLayout, 0, 1, &descriptorSets[imageIndex], 0, 0);
		
		VkDeviceSize bindingOffsetRayGenShader = rayTracingProperties.shaderGroupHandleSize * testShader->RAYTRACING_GROUP_INDEX_RGEN;
		VkDeviceSize bindingOffsetMissShader = rayTracingProperties.shaderGroupHandleSize * testShader->RAYTRACING_GROUP_INDEX_RMISS;
		VkDeviceSize bindingOffsetHitShader = rayTracingProperties.shaderGroupHandleSize * testShader->RAYTRACING_GROUP_INDEX_RCHIT;
		VkDeviceSize bindingStride = rayTracingProperties.shaderGroupHandleSize;
		
		renderingDevice->CmdTraceRaysNV(
			commandBuffer, 
			rayTracingShaderBindingTableBuffer, bindingOffsetRayGenShader,
			rayTracingShaderBindingTableBuffer, bindingOffsetMissShader, bindingStride,
			rayTracingShaderBindingTableBuffer, bindingOffsetHitShader, bindingStride,
			VK_NULL_HANDLE, 0, 0,
			swapChain->extent.width, swapChain->extent.height, 1
		);
		
		TransitionImageLayout(commandBuffer, swapChain->images[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
		
		VkImageCopy copyRegion {};
		copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copyRegion.srcOffset = {0,0,0};
		copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copyRegion.dstOffset = {0,0,0};
		copyRegion.extent = {swapChain->extent.width, swapChain->extent.height, 1};
		renderingDevice->CmdCopyImage(commandBuffer, storageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapChain->images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
		
		TransitionImageLayout(commandBuffer, swapChain->images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1);
		TransitionImageLayout(commandBuffer, storageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1);
		
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
		
		// // Draw One Single F***ing Triangle !
		// renderingDevice->CmdDraw(commandBuffer,
		// 	3, // vertexCount
		// 	1, // instanceCount
		// 	0, // firstVertex (defines the lowest value of gl_VertexIndex)
		// 	0  // firstInstance (defines the lowest value of gl_InstanceIndex)
		// );
	}

	void FrameUpdate(uint imageIndex) override {
		// static auto startTime = std::chrono::high_resolution_clock::now();
		// auto currentTime = std::chrono::high_resolution_clock::now();
		// float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		UBO ubo = {};
		// Slowly rotate the test object
		// ubo.model = glm::rotate(glm::mat4(1), time * glm::radians(10.0f), glm::vec3(0,0,1));
		if (useRayTracing) {
			// Current camera position
			ubo.viewInverse = glm::inverse(glm::lookAt(glm::vec3(2,2,2), glm::vec3(0,0,0), glm::vec3(0,0,1)));
			// Projection
			ubo.projInverse = glm::inverse(glm::perspective(glm::radians(60.0f), (float) swapChain->extent.width / (float) swapChain->extent.height, 0.1f, 10.0f));
			// ubo.projInverse[1][1] *= -1;
		} else {
			// Current camera position
			ubo.viewInverse = glm::lookAt(glm::vec3(2,2,2), glm::vec3(0,0,0), glm::vec3(0,0,1));
			// Projection
			ubo.projInverse = glm::perspective(glm::radians(60.0f), (float) swapChain->extent.width / (float) swapChain->extent.height, 0.1f, 10.0f);
			ubo.projInverse[1][1] *= -1;
		}
		// Update memory
		void* data;
		renderingDevice->MapMemory(uniformBuffersMemory[imageIndex], 0/*offset*/, sizeof(ubo), 0/*flags*/, &data);
			memcpy(data, &ubo, sizeof(ubo));
		renderingDevice->UnmapMemory(uniformBuffersMemory[imageIndex]);
	}

	
};
