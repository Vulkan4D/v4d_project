#pragma once

#include "VulkanRenderer.hpp"
#include "VulkanShaderBindingTable.hpp"

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

struct UBO {
	glm::mat4 viewInverse;
	glm::mat4 projInverse;
	glm::vec4 light;
	glm::vec3 ambient;
	int rtx_reflection_max_recursion;
	bool rtx_shadows;
};

// Ray tracing geometry instance
struct GeometryInstance {
	glm::mat3x4 transform;
	uint32_t instanceId : 24;
	uint32_t mask : 8;
	uint32_t instanceOffset : 24;
	uint32_t flags : 8;
	uint64_t accelerationStructureHandle;
};

struct AccelerationStructure {
	VkAccelerationStructureNV accelerationStructure = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	uint64_t handle = 0;
};

class VulkanRayTracingRenderer : public VulkanRenderer {
	using VulkanRenderer::VulkanRenderer;
	
	// Vertex Buffers for test object
	std::vector<Vertex> testObjectVertices;
	std::vector<uint32_t> testObjectIndices;
	VulkanBuffer vertexBuffer, indexBuffer, sphereBuffer;
	
	std::vector<Sphere> testObjectSpheres;
	
	VulkanBuffer uniformBuffer;
	VkDescriptorSet descriptorSet;
	
	// Ray Tracing stuff
	VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties{};
	std::vector<AccelerationStructure> rayTracingBottomLevelAccelerationStructures;
	AccelerationStructure rayTracingTopLevelAccelerationStructure;
	VulkanBuffer rayTracingShaderBindingTableBuffer;
	struct RayTracingStorageImage {
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImage image = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkFormat format;
	} rayTracingStorageImage;

	// Scene-specific
	std::vector<VkGeometryNV> testObjectGeometries;
	std::vector<VkGeometryNV> testSphereGeometries;
	std::vector<GeometryInstance> testObjectGeometryInstances;
	VulkanShaderBindingTable* shaderBindingTable = nullptr;
	
	void Init() override {
		RequiredDeviceExtension(VK_NV_RAY_TRACING_EXTENSION_NAME); // NVidia's RayTracing extension
		RequiredDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME); // Needed for RayTracing extension
		
		// Set all device features that you may want to use, then the unsupported features will be disabled, you may check via this object later.
		// deviceFeatures.geometryShader = VK_TRUE;
		// deviceFeatures.samplerAnisotropy = VK_TRUE;
		// deviceFeatures.sampleRateShading = VK_TRUE;
	}
	
	void ScoreGPUSelection(int& score, VulkanGPU* gpu) {
		// Build up a score here and the GPU with the highest score will be selected.
		// Add to the score optional specs, then multiply with mandatory specs.
		
		// Optional specs  -->  score += points * CONDITION
		score += 10 * (gpu->GetProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU); // Is a Dedicated GPU
		// score += 20 * gpu->GetFeatures().tessellationShader; // Supports Tessellation
		// score += gpu->GetProperties().limits.framebufferColorSampleCounts; // Add sample counts to the score (1-64)

		// Mandatory specs  -->  score *= CONDITION
		// score *= gpu->GetFeatures().geometryShader; // Supports Geometry Shaders
		// score *= gpu->GetFeatures().samplerAnisotropy; // Supports Anisotropic filtering
		// score *= gpu->GetFeatures().sampleRateShading; // Supports Sample Shading
	}
	
	void Info() override {
		// Query the ray tracing properties of the current implementation
		rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
		VkPhysicalDeviceProperties2 deviceProps2{};
		deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProps2.pNext = &rayTracingProperties;
		GetPhysicalDeviceProperties2(renderingDevice->GetGPU()->GetHandle(), &deviceProps2);
		// // MultiSampling
		// msaaSamples = std::min(VK_SAMPLE_COUNT_8_BIT, renderingGPU->GetMaxUsableSampleCount());
	}

	void CreateResources() override {
		VkFormat colorFormat = swapChain->format.format;
		renderingDevice->CreateImage(
			swapChain->extent.width, 
			swapChain->extent.height, 
			1, VK_SAMPLE_COUNT_1_BIT,
			colorFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			rayTracingStorageImage.image,
			rayTracingStorageImage.memory
		);

		VkImageViewCreateInfo viewInfo = {};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = rayTracingStorageImage.image;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = colorFormat;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
		if (renderingDevice->CreateImageView(&viewInfo, nullptr, &rayTracingStorageImage.view) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create texture image view");
		}
		
		TransitionImageLayout(rayTracingStorageImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);
	}
	
	void DestroyResources() override {
		if (rayTracingStorageImage.image != VK_NULL_HANDLE) {
			renderingDevice->DestroyImageView(rayTracingStorageImage.view, nullptr);
			renderingDevice->DestroyImage(rayTracingStorageImage.image, nullptr);
			renderingDevice->FreeMemory(rayTracingStorageImage.memory, nullptr);
			rayTracingStorageImage.image = VK_NULL_HANDLE;
		}
	}
	
public:
	void LoadScene() override {

		testObjectVertices = {
			{/*pos*/{-0.5,-0.5, 0.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.0f, /*color*/{1.0, 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 0.5,-0.5, 0.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.0f, /*color*/{0.0, 1.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 0.5, 0.5, 0.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.0f, /*color*/{0.0, 0.0, 1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{-0.5, 0.5, 0.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.0f, /*color*/{0.0, 1.0, 1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			//
			{/*pos*/{-0.5,-0.5,-0.5}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.0f, /*color*/{1.0, 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 0.5,-0.5,-0.5}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.0f, /*color*/{0.0, 1.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 0.5, 0.5,-0.5}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.0f, /*color*/{0.0, 0.0, 1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{-0.5, 0.5,-0.5}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.0f, /*color*/{0.0, 1.0, 1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			//
			{/*pos*/{-8.0,-8.0,-2.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.0f, /*color*/{0.5, 0.5, 0.5, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 8.0,-8.0,-2.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.0f, /*color*/{0.5, 0.5, 0.5, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 8.0, 8.0,-2.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.0f, /*color*/{0.5, 0.5, 0.5, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{-8.0, 8.0,-2.0}, /*roughness*/0.0f, /*normal*/{0.0, 0.0, 1.0}, /*scatter*/0.0f, /*color*/{0.5, 0.5, 0.5, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
		};
		testObjectIndices = {
			0, 1, 2, 2, 3, 0,
			4, 5, 6, 6, 7, 4,
			8, 9, 10, 10, 11, 8,
		};
		
		testObjectSpheres = {
			{/*scatter*/0.0f, /*roughness*/0.0f, /*pos*/{ 0.0, 0.0, 0.8}, /*radius*/0.4f, /*color*/{0.5, 0.6, 0.6, 1.0}, /*specular*/1.0f, /*metallic*/0.5f, /*refraction*/0.5f, /*density*/0.5f},
			{/*scatter*/0.0f, /*roughness*/0.0f, /*pos*/{-2.0,-1.0, 1.5}, /*radius*/0.4f, /*color*/{0.5, 0.6, 0.6, 1.0}, /*specular*/1.0f, /*metallic*/0.5f, /*refraction*/0.5f, /*density*/0.5f},
			{/*scatter*/0.0f, /*roughness*/0.5f, /*pos*/{ 1.0,-1.0, 1.5}, /*radius*/0.6f, /*color*/{0.5, 0.5, 0.5, 1.0}, /*specular*/1.0f, /*metallic*/0.9f, /*refraction*/0.5f, /*density*/0.5f},
			{/*scatter*/0.0f, /*roughness*/0.5f, /*pos*/{ 2.0,-1.0, 0.5}, /*radius*/0.3f, /*color*/{1.0, 1.0, 1.0, 1.0}, /*specular*/1.0f, /*metallic*/0.1f, /*refraction*/0.5f, /*density*/0.5f},
		};

		shaderBindingTable = new VulkanShaderBindingTable("incubator_MyVulkan/assets/shaders/rtx.rgen");
		shaderBindingTable->AddMissShader("incubator_MyVulkan/assets/shaders/rtx.rmiss");
		shaderBindingTable->AddMissShader("incubator_MyVulkan/assets/shaders/rtx.shadow.rmiss");
		shaderBindingTable->AddHitShader("incubator_MyVulkan/assets/shaders/rtx.rchit");
		shaderBindingTable->AddHitShader("incubator_MyVulkan/assets/shaders/rtx.sphere.rchit", "", "incubator_MyVulkan/assets/shaders/rtx.sphere.rint");
		
		// Uniforms
		shaderBindingTable->AddLayoutBinding(// accelerationStructure
			0, // binding
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, // descriptorType
			1, // count (for array)
			VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV // stage flags
		);
		shaderBindingTable->AddLayoutBinding(// resultImage
			1, // binding
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // descriptorType
			1, // count (for array)
			VK_SHADER_STAGE_RAYGEN_BIT_NV // stage flags
		);
		shaderBindingTable->AddLayoutBinding(// ubo
			2, // binding
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // descriptorType
			1, // count (for array)
			VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_MISS_BIT_NV // stage flags
		);
		shaderBindingTable->AddLayoutBinding(// vertex buffer
			3, // binding
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // descriptorType
			1, // count (for array)
			VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV // stage flags
		);
		shaderBindingTable->AddLayoutBinding(// index buffer
			4, // binding
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // descriptorType
			1, // count (for array)
			VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV // stage flags
		);
		shaderBindingTable->AddLayoutBinding(// sphere buffer
			5, // binding
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // descriptorType
			1, // count (for array)
			VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_INTERSECTION_BIT_NV // stage flags
		);
		
		shaderBindingTable->LoadShaders();
	}

	void UnloadScene() override {
		delete shaderBindingTable;
	}

protected:
	void CreateSceneGraphics() override {
		
		// Uniform buffers
		CreateBuffer(sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffer);

		// Vertices
		CreateBufferStaged(sizeof(Vertex) * testObjectVertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vertexBuffer, testObjectVertices.data());

		// Indices
		CreateBufferStaged(sizeof(uint32_t) * testObjectIndices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, indexBuffer, testObjectIndices.data());

		// Spheres
		CreateBufferStaged(sizeof(Sphere) * testObjectSpheres.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sphereBuffer, testObjectSpheres.data());
		
		// Geometries for bottom level acceleration structure
		VkGeometryNV triangleGeometry {};
			triangleGeometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
			triangleGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
			triangleGeometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
			triangleGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
			triangleGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_NV;
			triangleGeometry.geometry.triangles.vertexData = vertexBuffer.buffer;
			triangleGeometry.geometry.triangles.vertexOffset = 0;
			triangleGeometry.geometry.triangles.vertexCount = (uint)testObjectVertices.size();
			triangleGeometry.geometry.triangles.vertexStride = sizeof(Vertex);
			triangleGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
			triangleGeometry.geometry.triangles.indexData = indexBuffer.buffer;
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
			sphereGeometry.geometry.aabbs.aabbData = sphereBuffer.buffer;
			sphereGeometry.geometry.aabbs.numAABBs = (uint)testObjectSpheres.size();
			sphereGeometry.geometry.aabbs.stride = sizeof(Sphere);
			sphereGeometry.geometry.aabbs.offset = 0;
		testSphereGeometries.push_back(sphereGeometry);
		
		rayTracingBottomLevelAccelerationStructures.resize(2);
		
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
			if (renderingDevice->CreateAccelerationStructureNV(&accStructCreateInfo, nullptr, &rayTracingBottomLevelAccelerationStructures[0].accelerationStructure) != VK_SUCCESS)
				throw std::runtime_error("Failed to create bottom level acceleration structure for triangles");
				
			VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
			memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
			memoryRequirementsInfo.accelerationStructure = rayTracingBottomLevelAccelerationStructures[0].accelerationStructure;
				
			VkMemoryRequirements2 memoryRequirements2 {};
			renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirements2);
			
			VkMemoryAllocateInfo memoryAllocateInfo {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				nullptr,// pNext
				memoryRequirements2.memoryRequirements.size,// VkDeviceSize allocationSize
				renderingDevice->GetGPU()->FindMemoryType(memoryRequirements2.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)// memoryTypeIndex
			};
			if (renderingDevice->AllocateMemory(&memoryAllocateInfo, nullptr, &rayTracingBottomLevelAccelerationStructures[0].memory) != VK_SUCCESS)
				throw std::runtime_error("Failed to allocate memory for bottom level acceleration structure for triangles");
			
			VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo {
				VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV,
				nullptr,// pNext
				rayTracingBottomLevelAccelerationStructures[0].accelerationStructure,// accelerationStructure
				rayTracingBottomLevelAccelerationStructures[0].memory,// memory
				0,// VkDeviceSize memoryOffset
				0,// uint32_t deviceIndexCount
				nullptr// const uint32_t* pDeviceIndices
			};
			if (renderingDevice->BindAccelerationStructureMemoryNV(1, &accelerationStructureMemoryInfo) != VK_SUCCESS)
				throw std::runtime_error("Failed to bind bottom level acceleration structure memory for triangles");
			
			if (renderingDevice->GetAccelerationStructureHandleNV(rayTracingBottomLevelAccelerationStructures[0].accelerationStructure, sizeof(uint64_t), &rayTracingBottomLevelAccelerationStructures[0].handle))
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
			if (renderingDevice->CreateAccelerationStructureNV(&accStructCreateInfo, nullptr, &rayTracingBottomLevelAccelerationStructures[1].accelerationStructure) != VK_SUCCESS)
				throw std::runtime_error("Failed to create bottom level acceleration structure for spheres");
				
			VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
			memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
			memoryRequirementsInfo.accelerationStructure = rayTracingBottomLevelAccelerationStructures[1].accelerationStructure;
				
			VkMemoryRequirements2 memoryRequirements2 {};
			renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirements2);
			
			VkMemoryAllocateInfo memoryAllocateInfo {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				nullptr,// pNext
				memoryRequirements2.memoryRequirements.size,// VkDeviceSize allocationSize
				renderingDevice->GetGPU()->FindMemoryType(memoryRequirements2.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)// memoryTypeIndex
			};
			if (renderingDevice->AllocateMemory(&memoryAllocateInfo, nullptr, &rayTracingBottomLevelAccelerationStructures[1].memory) != VK_SUCCESS)
				throw std::runtime_error("Failed to allocate memory for bottom level acceleration structure for spheres");
			
			VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo {
				VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV,
				nullptr,// pNext
				rayTracingBottomLevelAccelerationStructures[1].accelerationStructure,// accelerationStructure
				rayTracingBottomLevelAccelerationStructures[1].memory,// memory
				0,// VkDeviceSize memoryOffset
				0,// uint32_t deviceIndexCount
				nullptr// const uint32_t* pDeviceIndices
			};
			if (renderingDevice->BindAccelerationStructureMemoryNV(1, &accelerationStructureMemoryInfo) != VK_SUCCESS)
				throw std::runtime_error("Failed to bind bottom level acceleration structure memory for spheres");
			
			if (renderingDevice->GetAccelerationStructureHandleNV(rayTracingBottomLevelAccelerationStructures[1].accelerationStructure, sizeof(uint64_t), &rayTracingBottomLevelAccelerationStructures[1].handle))
				throw std::runtime_error("Failed to get bottom level acceleration structure handle for spheres");
		}
		
		VulkanBuffer instanceBuffer;
		
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
			0x1, // mask
			0, // instanceOffset
			VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV, // flags
			rayTracingBottomLevelAccelerationStructures[0].handle // accelerationStructureHandle
		});
		// Spheres instance
		testObjectGeometryInstances.push_back({
			transform,
			0, // instanceId
			0x2, // mask
			1, // instanceOffset
			0, // flags
			rayTracingBottomLevelAccelerationStructures[1].handle // accelerationStructureHandle
		});
		
		// Ray Tracing Geometry Instances
		CreateBufferStaged(sizeof(GeometryInstance) * testObjectGeometryInstances.size(), VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, instanceBuffer, testObjectGeometryInstances.data());
		
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
			if (renderingDevice->CreateAccelerationStructureNV(&accStructCreateInfo, nullptr, &rayTracingTopLevelAccelerationStructure.accelerationStructure) != VK_SUCCESS)
				throw std::runtime_error("Failed to create top level acceleration structure");
			
			VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
			memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
			memoryRequirementsInfo.accelerationStructure = rayTracingTopLevelAccelerationStructure.accelerationStructure;
				
			VkMemoryRequirements2 memoryRequirements2 {};
			renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirements2);
			
			VkMemoryAllocateInfo memoryAllocateInfo {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				nullptr,// pNext
				memoryRequirements2.memoryRequirements.size,// VkDeviceSize allocationSize
				renderingDevice->GetGPU()->FindMemoryType(memoryRequirements2.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)// memoryTypeIndex
			};
			if (renderingDevice->AllocateMemory(&memoryAllocateInfo, nullptr, &rayTracingTopLevelAccelerationStructure.memory) != VK_SUCCESS)
				throw std::runtime_error("Failed to allocate memory for top level acceleration structure");
			
			VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo {
				VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV,
				nullptr,// pNext
				rayTracingTopLevelAccelerationStructure.accelerationStructure,// accelerationStructure
				rayTracingTopLevelAccelerationStructure.memory,// memory
				0,// VkDeviceSize memoryOffset
				0,// uint32_t deviceIndexCount
				nullptr// const uint32_t* pDeviceIndices
			};
			if (renderingDevice->BindAccelerationStructureMemoryNV(1, &accelerationStructureMemoryInfo) != VK_SUCCESS)
				throw std::runtime_error("Failed to bind top level acceleration structure memory");
			
			if (renderingDevice->GetAccelerationStructureHandleNV(rayTracingTopLevelAccelerationStructure.accelerationStructure, sizeof(uint64_t), &rayTracingTopLevelAccelerationStructure.handle))
				throw std::runtime_error("Failed to get top level acceleration structure handle");
		}
		
		// Build Ray Tracing acceleration structures
		{
			// RayTracing Scratch buffer
			VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
			memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
			memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
			VkMemoryRequirements2 memoryRequirementsBottomLevel1, memoryRequirementsBottomLevel2, memoryRequirementsTopLevel;
			memoryRequirementsInfo.accelerationStructure = rayTracingBottomLevelAccelerationStructures[0].accelerationStructure;
			renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirementsBottomLevel1);
			memoryRequirementsInfo.accelerationStructure = rayTracingBottomLevelAccelerationStructures[1].accelerationStructure;
			renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirementsBottomLevel2);
			memoryRequirementsInfo.accelerationStructure = rayTracingTopLevelAccelerationStructure.accelerationStructure;
			renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirementsTopLevel);
			// Send scratch buffer
			const VkDeviceSize scratchBufferSize = std::max(memoryRequirementsBottomLevel1.memoryRequirements.size + memoryRequirementsBottomLevel2.memoryRequirements.size, memoryRequirementsTopLevel.memoryRequirements.size);
			VulkanBuffer scratchBuffer;
			CreateBuffer(scratchBufferSize, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer);
			
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
					rayTracingBottomLevelAccelerationStructures[0].accelerationStructure, 
					VK_NULL_HANDLE, 
					scratchBuffer.buffer, 
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
					rayTracingBottomLevelAccelerationStructures[1].accelerationStructure, 
					VK_NULL_HANDLE, 
					scratchBuffer.buffer, 
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
					instanceBuffer.buffer, 
					0, 
					VK_FALSE, 
					rayTracingTopLevelAccelerationStructure.accelerationStructure, 
					VK_NULL_HANDLE, 
					scratchBuffer.buffer, 
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
				
			renderingDevice->DestroyBuffer(scratchBuffer);
			renderingDevice->DestroyBuffer(instanceBuffer);
		}
		

		
	}

	void DestroySceneGraphics() override {
		
		if (rayTracingTopLevelAccelerationStructure.accelerationStructure != VK_NULL_HANDLE) {
			// Acceleration Structure
			renderingDevice->FreeMemory(rayTracingTopLevelAccelerationStructure.memory, nullptr);
			renderingDevice->DestroyAccelerationStructureNV(rayTracingTopLevelAccelerationStructure.accelerationStructure, nullptr);
			rayTracingTopLevelAccelerationStructure.accelerationStructure = VK_NULL_HANDLE;
			testObjectGeometryInstances.clear();
			renderingDevice->FreeMemory(rayTracingBottomLevelAccelerationStructures[0].memory, nullptr);
			renderingDevice->DestroyAccelerationStructureNV(rayTracingBottomLevelAccelerationStructures[0].accelerationStructure, nullptr);
			renderingDevice->FreeMemory(rayTracingBottomLevelAccelerationStructures[1].memory, nullptr);
			renderingDevice->DestroyAccelerationStructureNV(rayTracingBottomLevelAccelerationStructures[1].accelerationStructure, nullptr);
			rayTracingBottomLevelAccelerationStructures.clear();
		}
		
		testObjectGeometries.clear();
		testSphereGeometries.clear();

		// Vertices
		renderingDevice->DestroyBuffer(vertexBuffer);

		// Indices
		renderingDevice->DestroyBuffer(indexBuffer);
		
		// Spheres
		renderingDevice->DestroyBuffer(sphereBuffer);
		
		// Uniform buffers
		renderingDevice->DestroyBuffer(uniformBuffer);
		
	}
	
	void CreateGraphicsPipelines() override {
		shaderBindingTable->CreateRayTracingPipeline(renderingDevice);
		
		// Shader Binding Table
		const uint32_t sbtSize = rayTracingProperties.shaderGroupHandleSize * shaderBindingTable->GetGroups().size();
		CreateBuffer(sbtSize, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, rayTracingShaderBindingTableBuffer);
		shaderBindingTable->WriteShaderBindingTableToBuffer(renderingDevice, &rayTracingShaderBindingTableBuffer, rayTracingProperties.shaderGroupHandleSize);
		
		// Descriptor sets 
		
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
		
		// allocate and update descriptor sets
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = shaderBindingTable->GetDescriptorSetLayout();
		if (renderingDevice->AllocateDescriptorSets(&allocInfo, &descriptorSet) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate descriptor sets");
		}
		
		// Descriptor sets for Ray Tracing Acceleration Structure
		VkWriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo {};
		descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
		descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
		descriptorAccelerationStructureInfo.pAccelerationStructures = &rayTracingTopLevelAccelerationStructure.accelerationStructure;
		
		// Storage image
		VkDescriptorImageInfo storageImageDescriptor{};
		storageImageDescriptor.imageView = rayTracingStorageImage.view;
		storageImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		
		VkDescriptorBufferInfo uniformBufferInfo = {uniformBuffer.buffer, 0, sizeof(UBO)};
		VkDescriptorBufferInfo vertexBufferInfo = {vertexBuffer.buffer, 0, sizeof(Vertex) * testObjectVertices.size()};
		VkDescriptorBufferInfo indexBufferInfo = {indexBuffer.buffer, 0, sizeof(uint32_t) * testObjectIndices.size()};
		VkDescriptorBufferInfo sphereBufferInfo = {sphereBuffer.buffer, 0, sizeof(Sphere) * testObjectSpheres.size()};

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

	}
	
	void DestroyGraphicsPipelines() override {
		// Shader binding table
		renderingDevice->DestroyBuffer(rayTracingShaderBindingTableBuffer);
		// Ray tracing pipeline
		shaderBindingTable->DestroyRayTracingPipeline(renderingDevice);
		// Descriptor Sets
		renderingDevice->FreeDescriptorSets(descriptorPool, 1, &descriptorSet);
		// Descriptor pools
		renderingDevice->DestroyDescriptorPool(descriptorPool, nullptr);
	}
	
	void RenderingCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		
		renderingDevice->CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, shaderBindingTable->GetPipeline());
		renderingDevice->CmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, shaderBindingTable->GetPipelineLayout(), 0, 1, &descriptorSet, 0, 0);
		
		VkDeviceSize bindingStride = rayTracingProperties.shaderGroupHandleSize;
		VkDeviceSize bindingOffsetMissShader = bindingStride * shaderBindingTable->GetMissGroupOffset();
		VkDeviceSize bindingOffsetHitShader = bindingStride * shaderBindingTable->GetHitGroupOffset();
		
		renderingDevice->CmdTraceRaysNV(
			commandBuffer, 
			rayTracingShaderBindingTableBuffer.buffer, 0,
			rayTracingShaderBindingTableBuffer.buffer, bindingOffsetMissShader, bindingStride,
			rayTracingShaderBindingTableBuffer.buffer, bindingOffsetHitShader, bindingStride,
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

	void FrameUpdate(uint imageIndex) override {
		// static auto startTime = std::chrono::high_resolution_clock::now();
		// auto currentTime = std::chrono::high_resolution_clock::now();
		// float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		UBO ubo = {};
		// Slowly rotate the test object
		// ubo.model = glm::rotate(glm::mat4(1), time * glm::radians(10.0f), glm::vec3(0,0,1));
		
		// Current camera position
		ubo.viewInverse = glm::inverse(glm::lookAt(camPosition, camPosition + camDirection, glm::vec3(0,0,1)));
		// Projection
		ubo.projInverse = glm::inverse(glm::perspective(glm::radians(80.0f), (float) swapChain->extent.width / (float) swapChain->extent.height, 0.1f, 100.0f));
		ubo.projInverse[1][1] *= -1;

		ubo.light = light;
		ubo.ambient = glm::vec3(0.15f, 0.15f, 0.15f);
		ubo.rtx_reflection_max_recursion = rtx_reflection_max_recursion;
		ubo.rtx_shadows = rtx_shadows;
		// Update memory
		renderingDevice->CopyDataToBuffer(&ubo, uniformBuffer);
	}

public:
	glm::vec3 camPosition = glm::vec3(2,2,2);
	glm::vec3 camDirection = glm::vec3(-2,-2,-2);
	glm::vec4 light {1.0,1.0,3.0, 1.0};
	int rtx_reflection_max_recursion = 4;
	bool rtx_shadows = true;
	
};
