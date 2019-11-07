#pragma once

#include "MyVulkanRenderer.hpp"

// Test Object Vertex Data Structure
struct Vertex {
	glm::vec3 pos;
	float roughness;
	glm::vec3 normal;
	float scatter;
	glm::vec4 color;
	glm::vec2 uv;
	float specular;
	float metallic;
};

struct Sphere {
	std::pair<glm::vec3,glm::vec3> boundingBox;
	float scatter;
	float roughness;
	glm::vec3 pos;
	float radius;
	glm::vec4 color;
	float specular;
	float metallic;
	float refraction;
	float density;
	
	Sphere(float scatter, float roughness, glm::vec3 pos, float radius, glm::vec4 color, float specular, float metallic, float refraction, float density)
	: boundingBox(pos - radius, pos + radius), scatter(scatter), roughness(roughness), pos(pos), radius(radius), color(color), specular(specular), metallic(metallic), refraction(refraction), density(density) {}
};

class MyVulkanTest : public MyVulkanRenderer {
	using MyVulkanRenderer::MyVulkanRenderer;
	
	VulkanShaderProgram* testShader;
	// Vertex Buffers for test object
	std::vector<Vertex> testObjectVertices;
	std::vector<uint32_t> testObjectIndices;
	VkBuffer vertexBuffer, indexBuffer, sphereBuffer;
	VkDeviceMemory vertexBufferMemory, indexBufferMemory, sphereBufferMemory;
	
	std::vector<Sphere> testObjectSpheres;
	
	struct UBO {
		glm::mat4 viewInverse;
		glm::mat4 projInverse;
		glm::vec4 light;
		glm::vec3 ambient;
		int rtx_reflection_max_recursion;
		bool rtx_shadows;
	};
	
	VkBuffer uniformBuffer;
	VkDeviceMemory uniformBufferMemory;
	VkDescriptorSet descriptorSet;
	
	// Ray tracing geometry instance
	struct GeometryInstance {
		glm::mat3x4 transform;
		uint32_t instanceId : 24;
		uint32_t mask : 8;
		uint32_t instanceOffset : 24;
		uint32_t flags : 8;
		uint64_t accelerationStructureHandle;
	};

	// Ray Tracing
	std::vector<VkGeometryNV> testObjectGeometries;
	std::vector<VkGeometryNV> testSphereGeometries;
	std::vector<GeometryInstance> testObjectGeometryInstances;
	
	std::vector<VkRayTracingShaderGroupCreateInfoNV> rayTracingShaderGroups;
	const static int RAYTRACING_GROUP_INDEX_RGEN = 0;
	const static int RAYTRACING_GROUP_INDEX_RMISS = 1;
	const static int RAYTRACING_GROUP_INDEX_RMISS_SHADOW = 2;
	const static int RAYTRACING_GROUP_INDEX_RCHIT = 3;
	const static int RAYTRACING_GROUP_INDEX_RCHIT_SHADOW = 4;
	const static int RAYTRACING_GROUP_INDEX_RCHIT_SPHERE = 5;
	
	// uint32_t RTX_REFLECTION_MAX_RECURSION = 4;
	
	
	void Init() override {
		clearColor = {0,0,0,1};
		useRayTracing = true;
	}

	void LoadScene() override {

		testObjectVertices = {
			{/*pos*/{-0.5,-0.5, 0.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.1f, /*color*/{1.0, 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 0.5,-0.5, 0.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.1f, /*color*/{0.0, 1.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 0.5, 0.5, 0.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.1f, /*color*/{0.0, 0.0, 1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{-0.5, 0.5, 0.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.1f, /*color*/{0.0, 1.0, 1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			//
			{/*pos*/{-0.5,-0.5,-0.5}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.1f, /*color*/{1.0, 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 0.5,-0.5,-0.5}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.1f, /*color*/{0.0, 1.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 0.5, 0.5,-0.5}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.1f, /*color*/{0.0, 0.0, 1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{-0.5, 0.5,-0.5}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.1f, /*color*/{0.0, 1.0, 1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			//
			{/*pos*/{-8.0,-8.0,-2.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.1f, /*color*/{0.5, 0.5, 0.5, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 8.0,-8.0,-2.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.1f, /*color*/{0.5, 0.5, 0.5, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 8.0, 8.0,-2.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.1f, /*color*/{0.5, 0.5, 0.5, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{-8.0, 8.0,-2.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.1f, /*color*/{0.5, 0.5, 0.5, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
		};
		testObjectIndices = {
			0, 1, 2, 2, 3, 0,
			4, 5, 6, 6, 7, 4,
			8, 9, 10, 10, 11, 8,
		};
		
		testObjectSpheres = {
			{/*scatter*/0.5f, /*roughness*/0.0f, /*pos*/{ 0.0, 0.0, 0.8}, /*radius*/0.4f, /*color*/{0.5, 0.6, 0.6, 1.0}, /*specular*/1.0f, /*metallic*/0.5f, /*refraction*/0.5f, /*density*/0.5f},
			{/*scatter*/0.5f, /*roughness*/0.0f, /*pos*/{-2.0,-1.0, 1.5}, /*radius*/0.4f, /*color*/{0.5, 0.6, 0.6, 1.0}, /*specular*/1.0f, /*metallic*/0.5f, /*refraction*/0.5f, /*density*/0.5f},
			{/*scatter*/0.5f, /*roughness*/0.0f, /*pos*/{ 1.0,-1.0, 1.5}, /*radius*/0.6f, /*color*/{0.5, 0.5, 0.5, 1.0}, /*specular*/1.0f, /*metallic*/0.9f, /*refraction*/0.5f, /*density*/0.5f},
			{/*scatter*/0.5f, /*roughness*/0.0f, /*pos*/{ 2.0,-1.0, 0.5}, /*radius*/0.3f, /*color*/{1.0, 1.0, 1.0, 1.0}, /*specular*/1.0f, /*metallic*/0.1f, /*refraction*/0.5f, /*density*/0.5f},
		};

	}

	void UnloadScene() override {
		
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
			renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
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
			renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
			CopyBuffer(stagingBuffer, indexBuffer, bufferSize);
			renderingDevice->DestroyBuffer(stagingBuffer, nullptr);
			renderingDevice->FreeMemory(stagingBufferMemory, nullptr);
		}

		// Spheres
		{
			void* data;
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			VkDeviceSize bufferSize = sizeof(Sphere) * testObjectSpheres.size();
			renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
			renderingDevice->MapMemory(stagingBufferMemory, 0, bufferSize, 0, &data);
				memcpy(data, testObjectSpheres.data(), bufferSize);
			renderingDevice->UnmapMemory(stagingBufferMemory);
			renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sphereBuffer, sphereBufferMemory);
			CopyBuffer(stagingBuffer, sphereBuffer, bufferSize);
			renderingDevice->DestroyBuffer(stagingBuffer, nullptr);
			renderingDevice->FreeMemory(stagingBufferMemory, nullptr);
		}

		
		if (useRayTracing) {
			
			
			// Geometries for bottom level acceleration structure
			VkGeometryNV triangleGeometry {};
				triangleGeometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
				triangleGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
				triangleGeometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
				triangleGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
				triangleGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_NV;
				triangleGeometry.geometry.triangles.vertexData = vertexBuffer;
				triangleGeometry.geometry.triangles.vertexOffset = 0;
				triangleGeometry.geometry.triangles.vertexCount = (uint)testObjectVertices.size();
				triangleGeometry.geometry.triangles.vertexStride = sizeof(Vertex);
				triangleGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
				triangleGeometry.geometry.triangles.indexData = indexBuffer;
				triangleGeometry.geometry.triangles.indexOffset = 0;
				triangleGeometry.geometry.triangles.indexCount = (uint)testObjectIndices.size();
				triangleGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
				triangleGeometry.geometry.triangles.transformData = VK_NULL_HANDLE;
				triangleGeometry.geometry.triangles.transformOffset = 0;
			testObjectGeometries.push_back(triangleGeometry);
			
			VkGeometryNV sphereGeometry {};
				sphereGeometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
				sphereGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
				sphereGeometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
				sphereGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_NV;
				sphereGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_NV;
				sphereGeometry.geometry.aabbs.aabbData = sphereBuffer;
				sphereGeometry.geometry.aabbs.numAABBs = (uint)testObjectSpheres.size();
				sphereGeometry.geometry.aabbs.stride = sizeof(Sphere);
				sphereGeometry.geometry.aabbs.offset = 0;
			testSphereGeometries.push_back(sphereGeometry);
			
			
			rayTracingBottomLevelAccelerationStructures.resize(2);
			rayTracingBottomLevelAccelerationStructureMemories.resize(2);
			rayTracingBottomLevelAccelerationStructureHandles.resize(2);
			
			{
				// Bottom Level acceleration structure for triangles
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
				if (renderingDevice->CreateAccelerationStructureNV(&accStructCreateInfo, nullptr, &rayTracingBottomLevelAccelerationStructures[0]) != VK_SUCCESS)
					throw std::runtime_error("Failed to create bottom level acceleration structure for triangles");
					
				VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
				memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
				memoryRequirementsInfo.accelerationStructure = rayTracingBottomLevelAccelerationStructures[0];
					
				VkMemoryRequirements2 memoryRequirements2 {};
				renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirements2);
				
				VkMemoryAllocateInfo memoryAllocateInfo {
					VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					nullptr,// pNext
					memoryRequirements2.memoryRequirements.size,// VkDeviceSize allocationSize
					renderingDevice->GetGPU()->FindMemoryType(memoryRequirements2.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)// memoryTypeIndex
				};
				if (renderingDevice->AllocateMemory(&memoryAllocateInfo, nullptr, &rayTracingBottomLevelAccelerationStructureMemories[0]) != VK_SUCCESS)
					throw std::runtime_error("Failed to allocate memory for bottom level acceleration structure for triangles");
				
				VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo {
					VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV,
					nullptr,// pNext
					rayTracingBottomLevelAccelerationStructures[0],// accelerationStructure
					rayTracingBottomLevelAccelerationStructureMemories[0],// memory
					0,// VkDeviceSize memoryOffset
					0,// uint32_t deviceIndexCount
					nullptr// const uint32_t* pDeviceIndices
				};
				if (renderingDevice->BindAccelerationStructureMemoryNV(1, &accelerationStructureMemoryInfo) != VK_SUCCESS)
					throw std::runtime_error("Failed to bind bottom level acceleration structure memory for triangles");
				
				if (renderingDevice->GetAccelerationStructureHandleNV(rayTracingBottomLevelAccelerationStructures[0], sizeof(uint64_t), &rayTracingBottomLevelAccelerationStructureHandles[0]))
					throw std::runtime_error("Failed to get bottom level acceleration structure handle for triangles");
			}
			
			{
				// Bottom Level acceleration structure for spheres
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
						(uint)testSphereGeometries.size(),// geometryCount
						testSphereGeometries.data()// VkGeometryNV pGeometries
					}
				};
				if (renderingDevice->CreateAccelerationStructureNV(&accStructCreateInfo, nullptr, &rayTracingBottomLevelAccelerationStructures[1]) != VK_SUCCESS)
					throw std::runtime_error("Failed to create bottom level acceleration structure for spheres");
					
				VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
				memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
				memoryRequirementsInfo.accelerationStructure = rayTracingBottomLevelAccelerationStructures[1];
					
				VkMemoryRequirements2 memoryRequirements2 {};
				renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirements2);
				
				VkMemoryAllocateInfo memoryAllocateInfo {
					VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					nullptr,// pNext
					memoryRequirements2.memoryRequirements.size,// VkDeviceSize allocationSize
					renderingDevice->GetGPU()->FindMemoryType(memoryRequirements2.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)// memoryTypeIndex
				};
				if (renderingDevice->AllocateMemory(&memoryAllocateInfo, nullptr, &rayTracingBottomLevelAccelerationStructureMemories[1]) != VK_SUCCESS)
					throw std::runtime_error("Failed to allocate memory for bottom level acceleration structure for spheres");
				
				VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo {
					VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV,
					nullptr,// pNext
					rayTracingBottomLevelAccelerationStructures[1],// accelerationStructure
					rayTracingBottomLevelAccelerationStructureMemories[1],// memory
					0,// VkDeviceSize memoryOffset
					0,// uint32_t deviceIndexCount
					nullptr// const uint32_t* pDeviceIndices
				};
				if (renderingDevice->BindAccelerationStructureMemoryNV(1, &accelerationStructureMemoryInfo) != VK_SUCCESS)
					throw std::runtime_error("Failed to bind bottom level acceleration structure memory for spheres");
				
				if (renderingDevice->GetAccelerationStructureHandleNV(rayTracingBottomLevelAccelerationStructures[1], sizeof(uint64_t), &rayTracingBottomLevelAccelerationStructureHandles[1]))
					throw std::runtime_error("Failed to get bottom level acceleration structure handle for spheres");
			}
			
			VkBuffer instanceBuffer;
			VkDeviceMemory instanceBufferMemory;
			
			// Geometry instances for top level acceleration structure
			glm::mat3x4 transform {
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f
			};
			// Triangles instance
			testObjectGeometryInstances.push_back({
				transform,
				0, // instanceId
				0xff, // mask
				0, // instanceOffset
				VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV, // flags
				rayTracingBottomLevelAccelerationStructureHandles[0] // accelerationStructureHandle
			});
			// Spheres instance
			testObjectGeometryInstances.push_back({
				transform,
				0, // instanceId
				0xff, // mask
				2, // instanceOffset
				0, // flags
				rayTracingBottomLevelAccelerationStructureHandles[1] // accelerationStructureHandle
			});
			
			// Ray Tracing Geometry Instances
			renderingDevice->CreateBuffer(sizeof(GeometryInstance) * testObjectGeometryInstances.size(), VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instanceBuffer, instanceBufferMemory, testObjectGeometryInstances.data());
			
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
						(uint)testObjectGeometryInstances.size(),// instanceCount
						0,// geometryCount
						nullptr// VkGeometryNV pGeometries
					}
				};
				if (renderingDevice->CreateAccelerationStructureNV(&accStructCreateInfo, nullptr, &rayTracingTopLevelAccelerationStructure) != VK_SUCCESS)
					throw std::runtime_error("Failed to create top level acceleration structure");
				
				VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
				memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
				memoryRequirementsInfo.accelerationStructure = rayTracingTopLevelAccelerationStructure;
					
				VkMemoryRequirements2 memoryRequirements2 {};
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
			
			// Build Ray Tracing acceleration structures
			{
				// RayTracing Scratch buffer
				VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
				memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
				memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
				VkMemoryRequirements2 memoryRequirementsBottomLevel1, memoryRequirementsBottomLevel2, memoryRequirementsTopLevel;
				memoryRequirementsInfo.accelerationStructure = rayTracingBottomLevelAccelerationStructures[0];
				renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirementsBottomLevel1);
				memoryRequirementsInfo.accelerationStructure = rayTracingBottomLevelAccelerationStructures[1];
				renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirementsBottomLevel2);
				memoryRequirementsInfo.accelerationStructure = rayTracingTopLevelAccelerationStructure;
				renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirementsTopLevel);
				// Send scratch buffer
				const VkDeviceSize scratchBufferSize = std::max(memoryRequirementsBottomLevel1.memoryRequirements.size + memoryRequirementsBottomLevel2.memoryRequirements.size, memoryRequirementsTopLevel.memoryRequirements.size);
				VkBuffer scratchBuffer;
				VkDeviceMemory scratchBufferMemory;
				renderingDevice->CreateBuffer(scratchBufferSize, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchBufferMemory);
				
				auto cmdBuffer = BeginSingleTimeCommands(commandPool);
					
					VkAccelerationStructureInfoNV accelerationStructBuildInfo {};
					accelerationStructBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
				
					VkMemoryBarrier memoryBarrier {
						VK_STRUCTURE_TYPE_MEMORY_BARRIER,
						nullptr,// pNext
						VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,// VkAccessFlags srcAccessMask
						VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,// VkAccessFlags dstAccessMask
					};
					
					accelerationStructBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
					accelerationStructBuildInfo.geometryCount = (uint)testObjectGeometries.size();
					accelerationStructBuildInfo.pGeometries = testObjectGeometries.data();
					accelerationStructBuildInfo.instanceCount = 0;
					
					renderingDevice->CmdBuildAccelerationStructureNV(
						cmdBuffer, 
						&accelerationStructBuildInfo, 
						VK_NULL_HANDLE, 
						0, 
						VK_FALSE, 
						rayTracingBottomLevelAccelerationStructures[0], 
						VK_NULL_HANDLE, 
						scratchBuffer, 
						0
					);
					
					renderingDevice->CmdPipelineBarrier(
						cmdBuffer, 
						VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
						VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
						0, 
						1, &memoryBarrier, 
						0, 0, 
						0, 0
					);
					
					accelerationStructBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
					accelerationStructBuildInfo.geometryCount = (uint)testSphereGeometries.size();
					accelerationStructBuildInfo.pGeometries = testSphereGeometries.data();
					accelerationStructBuildInfo.instanceCount = 0;
					
					renderingDevice->CmdBuildAccelerationStructureNV(
						cmdBuffer, 
						&accelerationStructBuildInfo, 
						VK_NULL_HANDLE, 
						0, 
						VK_FALSE, 
						rayTracingBottomLevelAccelerationStructures[1], 
						VK_NULL_HANDLE, 
						scratchBuffer, 
						memoryRequirementsBottomLevel1.memoryRequirements.size
					);
					
					renderingDevice->CmdPipelineBarrier(
						cmdBuffer, 
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
					accelerationStructBuildInfo.instanceCount = (uint)testObjectGeometryInstances.size();
					
					renderingDevice->CmdBuildAccelerationStructureNV(
						cmdBuffer, 
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
						cmdBuffer, 
						VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
						VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
						0, 
						1, &memoryBarrier, 
						0, 0, 
						0, 0
					);
				
				EndSingleTimeCommands(commandPool, cmdBuffer);
					
				renderingDevice->DestroyBuffer(scratchBuffer, nullptr);
				renderingDevice->FreeMemory(scratchBufferMemory, nullptr);
				
				renderingDevice->DestroyBuffer(instanceBuffer, nullptr);
				renderingDevice->FreeMemory(instanceBufferMemory, nullptr);
			}
			
		}

		// Uniform buffers
		VkDeviceSize bufferSize = sizeof(UBO);
		renderingDevice->CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffer, uniformBufferMemory);
		







		// // Pass recursion depth for reflections to ray generation shader via specialization constant
		// VkSpecializationMapEntry rgenSpecializationMapEntry {};
		// rgenSpecializationMapEntry.constantID = 0;
		// rgenSpecializationMapEntry.offset = 0;
		// rgenSpecializationMapEntry.size = sizeof(RTX_REFLECTION_MAX_RECURSION);
		// VkSpecializationInfo rgenSpecializationInfo {};
		// rgenSpecializationInfo.mapEntryCount = 1;
		// rgenSpecializationInfo.pMapEntries = &rgenSpecializationMapEntry;
		// rgenSpecializationInfo.dataSize = sizeof(RTX_REFLECTION_MAX_RECURSION);
		// rgenSpecializationInfo.pData = &RTX_REFLECTION_MAX_RECURSION;
		

		if (useRayTracing) {
			
			
			// Load shader files
			testShader = new VulkanShaderProgram(renderingDevice, {
				{"incubator_MyVulkan/assets/shaders/rtx.rgen"/*, "main", &rgenSpecializationInfo*/},
				{"incubator_MyVulkan/assets/shaders/rtx.rmiss"},
				{"incubator_MyVulkan/assets/shaders/rtx.shadow.rmiss"},
				{"incubator_MyVulkan/assets/shaders/rtx.rchit"},
				{"incubator_MyVulkan/assets/shaders/rtx.sphere.rchit"},
				{"incubator_MyVulkan/assets/shaders/rtx.sphere.rint"},
			});
			
			rayTracingShaderGroups.resize(6);
			
			rayTracingShaderGroups[RAYTRACING_GROUP_INDEX_RGEN] = {
				VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV,
				nullptr,
				VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV,
				0, // generalShader
				VK_SHADER_UNUSED_NV, // closestHitShader;
				VK_SHADER_UNUSED_NV, // anyHitShader;
				VK_SHADER_UNUSED_NV // intersectionShader;
			};
			rayTracingShaderGroups[RAYTRACING_GROUP_INDEX_RMISS] = {
				VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV,
				nullptr,
				VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV,
				1, // generalShader
				VK_SHADER_UNUSED_NV, // closestHitShader;
				VK_SHADER_UNUSED_NV, // anyHitShader;
				VK_SHADER_UNUSED_NV // intersectionShader;
			};
			rayTracingShaderGroups[RAYTRACING_GROUP_INDEX_RMISS_SHADOW] = {
				VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV,
				nullptr,
				VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV,
				2, // generalShader
				VK_SHADER_UNUSED_NV, // closestHitShader;
				VK_SHADER_UNUSED_NV, // anyHitShader;
				VK_SHADER_UNUSED_NV // intersectionShader;
			};
			rayTracingShaderGroups[RAYTRACING_GROUP_INDEX_RCHIT] = {
				VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV,
				nullptr,
				VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV,
				VK_SHADER_UNUSED_NV, // generalShader
				3, // closestHitShader;
				VK_SHADER_UNUSED_NV, // anyHitShader;
				VK_SHADER_UNUSED_NV // intersectionShader;
			};
			rayTracingShaderGroups[RAYTRACING_GROUP_INDEX_RCHIT_SHADOW] = {
				VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV,
				nullptr,
				VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV,
				VK_SHADER_UNUSED_NV, // generalShader
				2, // closestHitShader;
				VK_SHADER_UNUSED_NV, // anyHitShader;
				VK_SHADER_UNUSED_NV // intersectionShader;
			};
			rayTracingShaderGroups[RAYTRACING_GROUP_INDEX_RCHIT_SPHERE] = {
				VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV,
				nullptr,
				VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV,
				VK_SHADER_UNUSED_NV, // generalShader
				4, // closestHitShader;
				VK_SHADER_UNUSED_NV, // anyHitShader;
				5 // intersectionShader;
			};
			
			
			// Uniforms
			testShader->AddLayoutBindings({
				{// accelerationStructure
					0, // binding
					VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, // descriptorType
					1,
					VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, // stage flags
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
					VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_MISS_BIT_NV, // stage flags
					nullptr // pImmutableSamplers
				},
				{// vertex buffer
					3, // binding
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // descriptorType
					1, // count (for array)
					VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, // stage flags
					nullptr // pImmutableSamplers
				},
				{// index buffer
					4, // binding
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // descriptorType
					1, // count (for array)
					VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, // stage flags
					nullptr // pImmutableSamplers
				},
				{// sphere buffer
					5, // binding
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // descriptorType
					1, // count (for array)
					VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_INTERSECTION_BIT_NV, // stage flags
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
				{4, offsetof(Vertex, Vertex::color), VK_FORMAT_R32G32B32A32_SFLOAT},
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
		
		
		
		// Ray Tracing Pipeline
		
		if (useRayTracing) {
			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
			pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCreateInfo.setLayoutCount = (uint)testShader->GetDescriptorSetLayouts().size();
			pipelineLayoutCreateInfo.pSetLayouts = testShader->GetDescriptorSetLayouts().data();
			//TODO add pipelineLayoutCreateInfo.pPushConstantRanges
			
			if (renderingDevice->CreatePipelineLayout(&pipelineLayoutCreateInfo, nullptr, &rayTracingPipelineLayout) != VK_SUCCESS)
				throw std::runtime_error("Failed to create ray tracing pipeline layout");
				
			
			
		
			VkRayTracingPipelineCreateInfoNV rayTracingPipelineInfo {};
			rayTracingPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
			rayTracingPipelineInfo.stageCount = (uint)testShader->GetStages().size();
			rayTracingPipelineInfo.pStages = testShader->GetStages().data();
			rayTracingPipelineInfo.groupCount = (uint)rayTracingShaderGroups.size();
			rayTracingPipelineInfo.pGroups = rayTracingShaderGroups.data();
			rayTracingPipelineInfo.maxRecursionDepth = 3;
			rayTracingPipelineInfo.layout = rayTracingPipelineLayout;
			
			if (renderingDevice->CreateRayTracingPipelinesNV(VK_NULL_HANDLE, 1, &rayTracingPipelineInfo, nullptr, &rayTracingPipeline) != VK_SUCCESS) //TODO support multiple ray tracing pipelines
				throw std::runtime_error("Failed to create ray tracing pipelines");
			
			
			
			
			
			// Shader Binding Table
			const uint32_t sbtSize = rayTracingProperties.shaderGroupHandleSize * rayTracingShaderGroups.size();
			renderingDevice->CreateBuffer(sbtSize, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, rayTracingShaderBindingTableBuffer, rayTracingShaderBindingTableBufferMemory);
			uint8_t* data;
			renderingDevice->MapMemory(rayTracingShaderBindingTableBufferMemory, 0, sbtSize, 0, (void**)&data);
			auto shaderHandleStorage = new uint8_t[sbtSize];
			if (renderingDevice->GetRayTracingShaderGroupHandlesNV(rayTracingPipeline, 0, (uint)rayTracingShaderGroups.size(), sbtSize, shaderHandleStorage) != VK_SUCCESS)
				throw std::runtime_error("Failed to get ray tracing shader group handles");
			data += CopyShaderIdentifier(data, shaderHandleStorage, RAYTRACING_GROUP_INDEX_RGEN);
			data += CopyShaderIdentifier(data, shaderHandleStorage, RAYTRACING_GROUP_INDEX_RMISS);
			data += CopyShaderIdentifier(data, shaderHandleStorage, RAYTRACING_GROUP_INDEX_RMISS_SHADOW);
			data += CopyShaderIdentifier(data, shaderHandleStorage, RAYTRACING_GROUP_INDEX_RCHIT);
			data += CopyShaderIdentifier(data, shaderHandleStorage, RAYTRACING_GROUP_INDEX_RCHIT_SHADOW);
			data += CopyShaderIdentifier(data, shaderHandleStorage, RAYTRACING_GROUP_INDEX_RCHIT_SPHERE);
			renderingDevice->UnmapMemory(rayTracingShaderBindingTableBufferMemory);
			delete[] shaderHandleStorage;
		}
		
		
		
		
		
		
		
		if (useRayTracing) {
			renderingDevice->CreateDescriptorPool(
				{
					{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1},
					{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
					{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
					{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
				}, 
				descriptorPool, 
				VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
			);
		} else {
			renderingDevice->CreateDescriptorPool(
				{
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				}, 
				1, 
				descriptorPool, 
				VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
			);
		}
		


		
		
		
		// Descriptor sets 
		
		if (useRayTracing) {
			
			// allocate and update descriptor sets
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = (uint)testShader->GetDescriptorSetLayouts().size();
			allocInfo.pSetLayouts = testShader->GetDescriptorSetLayouts().data();
			if (renderingDevice->AllocateDescriptorSets(&allocInfo, &descriptorSet) != VK_SUCCESS) {
				throw std::runtime_error("Failed to allocate descriptor sets");
			}
			
			// Descriptor sets for Ray Tracing Acceleration Structure
			VkWriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo {};
			descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
			descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
			descriptorAccelerationStructureInfo.pAccelerationStructures = &rayTracingTopLevelAccelerationStructure;
			
			// Storage image
			VkDescriptorImageInfo storageImageDescriptor{};
			storageImageDescriptor.imageView = rayTracingStorageImage.view;
			storageImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			
			VkDescriptorBufferInfo uniformBufferInfo = {uniformBuffer, 0, sizeof(UBO)};
			VkDescriptorBufferInfo vertexBufferInfo = {vertexBuffer, 0, sizeof(Vertex) * testObjectVertices.size()};
			VkDescriptorBufferInfo indexBufferInfo = {indexBuffer, 0, sizeof(uint32_t) * testObjectIndices.size()};
			VkDescriptorBufferInfo sphereBufferInfo = {sphereBuffer, 0, sizeof(Sphere) * testObjectSpheres.size()};

			std::vector<VkWriteDescriptorSet> descriptorWrites {
				{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // acceleration structure
					&descriptorAccelerationStructureInfo,// pNext
					descriptorSet,// VkDescriptorSet dstSet
					0,// uint dstBinding
					0,// uint dstArrayElement
					1,// uint descriptorCount
					VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV,// VkDescriptorType descriptorType
					nullptr,// VkDescriptorImageInfo* pImageInfo
					nullptr,// VkDescriptorBufferInfo* pBufferInfo
					nullptr// VkBufferView* pTexelBufferView
				},
				{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // result image
					nullptr,// pNext
					descriptorSet,// VkDescriptorSet dstSet
					1,// uint dstBinding
					0,// uint dstArrayElement
					1,// uint descriptorCount
					VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,// VkDescriptorType descriptorType
					&storageImageDescriptor,// VkDescriptorImageInfo* pImageInfo
					nullptr,// VkDescriptorBufferInfo* pBufferInfo
					nullptr// VkBufferView* pTexelBufferView
				},
				{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // ubo
					nullptr,// pNext
					descriptorSet,// VkDescriptorSet dstSet
					2,// uint dstBinding
					0,// uint dstArrayElement
					1,// uint descriptorCount
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,// VkDescriptorType descriptorType
					nullptr,// VkDescriptorImageInfo* pImageInfo
					&uniformBufferInfo,// VkDescriptorBufferInfo* pBufferInfo
					nullptr// VkBufferView* pTexelBufferView
				},
				{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // vertex buffer
					nullptr,// pNext
					descriptorSet,// VkDescriptorSet dstSet
					3,// uint dstBinding
					0,// uint dstArrayElement
					1,// uint descriptorCount
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,// VkDescriptorType descriptorType
					nullptr,// VkDescriptorImageInfo* pImageInfo
					&vertexBufferInfo,// VkDescriptorBufferInfo* pBufferInfo
					nullptr// VkBufferView* pTexelBufferView
				},
				{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // index buffer
					nullptr,// pNext
					descriptorSet,// VkDescriptorSet dstSet
					4,// uint dstBinding
					0,// uint dstArrayElement
					1,// uint descriptorCount
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,// VkDescriptorType descriptorType
					nullptr,// VkDescriptorImageInfo* pImageInfo
					&indexBufferInfo,// VkDescriptorBufferInfo* pBufferInfo
					nullptr// VkBufferView* pTexelBufferView
				},
				{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // sphere buffer
					nullptr,// pNext
					descriptorSet,// VkDescriptorSet dstSet
					5,// uint dstBinding
					0,// uint dstArrayElement
					1,// uint descriptorCount
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,// VkDescriptorType descriptorType
					nullptr,// VkDescriptorImageInfo* pImageInfo
					&sphereBufferInfo,// VkDescriptorBufferInfo* pBufferInfo
					nullptr// VkBufferView* pTexelBufferView
				},
			};

			// Update Descriptor Sets
			renderingDevice->UpdateDescriptorSets((uint)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
		} else {
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = testShader->GetDescriptorSetLayouts().size();
			allocInfo.pSetLayouts = testShader->GetDescriptorSetLayouts().data();
			if (renderingDevice->AllocateDescriptorSets(&allocInfo, &descriptorSet) != VK_SUCCESS) {
				throw std::runtime_error("Failed to allocate descriptor sets");
			}
			for (size_t i = 0; i < swapChain->imageViews.size(); i++) {
				std::array<VkWriteDescriptorSet, 1> descriptorWrites = {};

				// ubo
				descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[0].dstSet = descriptorSet;
				descriptorWrites[0].dstBinding = 0; // layout(binding = 0) uniform...
				descriptorWrites[0].dstArrayElement = 0; // array
				descriptorWrites[0].descriptorCount = 1; // array
				descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				VkDescriptorBufferInfo bufferInfo = {uniformBuffer, 0, sizeof(UBO)};
				descriptorWrites[0].pBufferInfo = &bufferInfo;

				// Update Descriptor Sets
				renderingDevice->UpdateDescriptorSets(descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
			}
		}
	}

	void DeleteSceneFromDevice() override {
		
		// Descriptor Sets
		renderingDevice->FreeDescriptorSets(descriptorPool, 1, &descriptorSet);
		
		// Descriptor pools
		renderingDevice->DestroyDescriptorPool(descriptorPool, nullptr);
		
		// Uniform buffers
		renderingDevice->DestroyBuffer(uniformBuffer, nullptr);
		renderingDevice->FreeMemory(uniformBufferMemory, nullptr);
		
		if (rayTracingTopLevelAccelerationStructure != VK_NULL_HANDLE) {
			// Shader binding table
			renderingDevice->DestroyBuffer(rayTracingShaderBindingTableBuffer, nullptr);
			renderingDevice->FreeMemory(rayTracingShaderBindingTableBufferMemory, nullptr);
			
			// Ray tracing pipeline
			renderingDevice->DestroyPipeline(rayTracingPipeline, nullptr);
			renderingDevice->DestroyPipelineLayout(rayTracingPipelineLayout, nullptr);
			
			// Acceleration Structure
			renderingDevice->FreeMemory(rayTracingTopLevelAccelerationStructureMemory, nullptr);
			renderingDevice->DestroyAccelerationStructureNV(rayTracingTopLevelAccelerationStructure, nullptr);
			rayTracingTopLevelAccelerationStructure = VK_NULL_HANDLE;
			testObjectGeometryInstances.clear();
			renderingDevice->FreeMemory(rayTracingBottomLevelAccelerationStructureMemories[0], nullptr);
			renderingDevice->DestroyAccelerationStructureNV(rayTracingBottomLevelAccelerationStructures[0], nullptr);
			renderingDevice->FreeMemory(rayTracingBottomLevelAccelerationStructureMemories[1], nullptr);
			renderingDevice->DestroyAccelerationStructureNV(rayTracingBottomLevelAccelerationStructures[1], nullptr);
			rayTracingBottomLevelAccelerationStructureMemories.clear();
			rayTracingBottomLevelAccelerationStructureHandles.clear();
			rayTracingBottomLevelAccelerationStructures.clear();
			testObjectGeometries.clear();
			testSphereGeometries.clear();
		}

		// Shaders
		delete testShader;
		
		// Vertices
		renderingDevice->DestroyBuffer(vertexBuffer, nullptr);
		renderingDevice->FreeMemory(vertexBufferMemory, nullptr);

		// Indices
		renderingDevice->DestroyBuffer(indexBuffer, nullptr);
		renderingDevice->FreeMemory(indexBufferMemory, nullptr);
		
		// Spheres
		renderingDevice->DestroyBuffer(sphereBuffer, nullptr);
		renderingDevice->FreeMemory(sphereBufferMemory, nullptr);
		
	}
	
	void ConfigureRayTracingPipelines() override {
		//
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
		renderingDevice->CmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, rayTracingPipelineLayout, 0, 1, &descriptorSet, 0, 0);
		
		VkDeviceSize bindingStride = rayTracingProperties.shaderGroupHandleSize;
		VkDeviceSize bindingOffsetRayGenShader = bindingStride * RAYTRACING_GROUP_INDEX_RGEN;
		VkDeviceSize bindingOffsetMissShader = bindingStride * RAYTRACING_GROUP_INDEX_RMISS;
		VkDeviceSize bindingOffsetHitShader = bindingStride * RAYTRACING_GROUP_INDEX_RCHIT;
		
		renderingDevice->CmdTraceRaysNV(
			commandBuffer, 
			rayTracingShaderBindingTableBuffer, bindingOffsetRayGenShader,
			rayTracingShaderBindingTableBuffer, bindingOffsetMissShader, bindingStride,
			rayTracingShaderBindingTableBuffer, bindingOffsetHitShader, bindingStride,
			VK_NULL_HANDLE, 0, 0,
			swapChain->extent.width, swapChain->extent.height, 1
		);
		
		TransitionImageLayout(commandBuffer, swapChain->images[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
		TransitionImageLayout(commandBuffer, rayTracingStorageImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1);
		
		VkImageCopy copyRegion {};
		copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copyRegion.srcOffset = {0,0,0};
		copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copyRegion.dstOffset = {0,0,0};
		copyRegion.extent = {swapChain->extent.width, swapChain->extent.height, 1};
		renderingDevice->CmdCopyImage(commandBuffer, rayTracingStorageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapChain->images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
		
		TransitionImageLayout(commandBuffer, swapChain->images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1);
		TransitionImageLayout(commandBuffer, rayTracingStorageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1);
		
	}

	void ConfigureCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override { // This method is not called when using Ray Tracing
	
		// We can now bind the graphics pipeline
		renderingDevice->CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPass->graphicsPipelines[0]->handle); // The second parameter specifies if the pipeline object is a graphics or compute pipeline.
		
		// Bind Descriptor Sets (Uniforms)
		renderingDevice->CmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPass->graphicsPipelines[0]->pipelineLayout, 0/*firstSet*/, 1/*count*/, &descriptorSet, 0, nullptr);


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
			ubo.projInverse = glm::inverse(glm::perspective(glm::radians(80.0f), (float) swapChain->extent.width / (float) swapChain->extent.height, 0.1f, 100.0f));
			ubo.projInverse[1][1] *= -1;
		} else {
			// Current camera position
			ubo.viewInverse = glm::lookAt(glm::vec3(2,2,2), glm::vec3(0,0,0), glm::vec3(0,0,1));
			// Projection
			ubo.projInverse = glm::perspective(glm::radians(80.0f), (float) swapChain->extent.width / (float) swapChain->extent.height, 0.1f, 100.0f);
			ubo.projInverse[1][1] *= -1;
		}
		ubo.light = light;
		ubo.ambient = glm::vec3(0.15f, 0.15f, 0.15f);
		ubo.rtx_reflection_max_recursion = rtx_reflection_max_recursion;
		ubo.rtx_shadows = rtx_shadows;
		// Update memory
		void* data;
		renderingDevice->MapMemory(uniformBufferMemory, 0/*offset*/, sizeof(ubo), 0/*flags*/, &data);
			memcpy(data, &ubo, sizeof(ubo));
		renderingDevice->UnmapMemory(uniformBufferMemory);
	}

public:
	glm::vec4 light {1.0,1.0,3.0, 1.0};
	int rtx_reflection_max_recursion = 4;
	bool rtx_shadows = true;
	
};
