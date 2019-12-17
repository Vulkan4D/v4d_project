#pragma once
#include <v4d.h>
#include "../incubator_rendering/helpers/Geometry.hpp"

using namespace v4d::graphics::vulkan::rtx;

#pragma region Scene-specific structs

// Test Object Vertex Data Structure
struct Vertex {
	glm::vec3 pos;
	float roughness;
	glm::vec3 normal;
	float emissive;
	glm::vec4 color;
	glm::vec2 uv;
	float specular;
	float metallic;
};

struct Sphere : public ProceduralGeometryData {
	float emissive;
	float roughness;
	glm::vec3 pos;
	float radius;
	glm::vec4 color;
	float specular;
	float metallic;
	float refraction;
	float density;
	
	Sphere(float emissive, float roughness, glm::vec3 pos, float radius, glm::vec4 color, float specular, float metallic, float refraction, float density)
	: ProceduralGeometryData(pos - radius, pos + radius), emissive(emissive), roughness(roughness), pos(pos), radius(radius), color(color), specular(specular), metallic(metallic), refraction(refraction), density(density) {}
};

struct UBO {
	glm::dmat4 viewInverse;
	glm::dmat4 projInverse;
	glm::dvec4 light;
	glm::vec3 ambient;
	double time;
	int samplesPerPixel;
	int rtx_reflection_max_recursion;
	bool rtx_shadows;
};

#pragma endregion

#pragma region RayTracing structs

struct AccelerationStructure {
	VkAccelerationStructureNV accelerationStructure = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	uint64_t handle = 0;
	VkAccelerationStructureCreateInfoNV createInfo;
};

struct BottomLevelAccelerationStructure : public AccelerationStructure {
	std::vector<Geometry*> geometries {};
	std::vector<VkGeometryNV> rayTracingGeometries {};
	void GenerateRayTracingGeometries() {
		for (auto* geometry : geometries) {
			rayTracingGeometries.push_back(geometry->GetRayTracingGeometry());
		}
	}
};

struct RayTracingGeometryInstance {
	glm::mat3x4 transform;
	uint32_t instanceId : 24;
	uint32_t mask : 8;
	uint32_t instanceOffset : 24;
	uint32_t flags : 8; // VkGeometryInstanceFlagBitsNV
	uint64_t accelerationStructureHandle;
};

struct TopLevelAccelerationStructure : public AccelerationStructure {
	std::vector<RayTracingGeometryInstance> instances {};
};

#pragma endregion

struct GeometryInstance {
	glm::mat3x4 transform;
	uint32_t instanceId;
	// Ray tracing
	uint32_t rayTracingMask;
	uint32_t rayTracingShaderHitOffset;
	uint32_t rayTracingFlags;
	BottomLevelAccelerationStructure* rayTracingBlas;
	int rayTracingGeometryInstanceIndex = -1;
};

class RayTracingRenderer : public v4d::graphics::Renderer {
	using v4d::graphics::Renderer::Renderer;
	
	PipelineLayout rayTracingPipelineLayout;
	
private: // Buffers	
	Buffer uniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(UBO)};
	std::vector<Buffer*> stagedBuffers {};
	
private: // Scene objects
	std::vector<Geometry*> geometries {};
	std::vector<GeometryInstance> geometryInstances {};
	
private: // Ray Tracing stuff
	std::vector<BottomLevelAccelerationStructure> rayTracingBottomLevelAccelerationStructures {};
	TopLevelAccelerationStructure rayTracingTopLevelAccelerationStructure {};
	ShaderBindingTable* shaderBindingTable = nullptr;
	Buffer rayTracingShaderBindingTableBuffer {VK_BUFFER_USAGE_RAY_TRACING_BIT_NV};
	VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties{};
	
	Image rayTracingStorageImage {
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
	};
	float rayTracingImageScale = 2;
	
	void RayTracingInit() {
		RequiredDeviceExtension(VK_NV_RAY_TRACING_EXTENSION_NAME); // NVidia's RayTracing extension
		RequiredDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME); // Needed for RayTracing extension
	}
	void RayTracingInfo() {
		// Query the ray tracing properties of the current implementation
		rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
		VkPhysicalDeviceProperties2 deviceProps2{};
		deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProps2.pNext = &rayTracingProperties;
		GetPhysicalDeviceProperties2(renderingDevice->GetPhysicalDevice()->GetHandle(), &deviceProps2);
	}
	void CreateRayTracingResources() {
		rayTracingStorageImage.Create(
			renderingDevice,
			(uint)((float)swapChain->extent.width * rayTracingImageScale),
			(uint)((float)swapChain->extent.height * rayTracingImageScale),
			{VK_FORMAT_R32G32B32A32_SFLOAT}
		);
		TransitionImageLayout(rayTracingStorageImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	}
	void DestroyRayTracingResources() {
		rayTracingStorageImage.Destroy(renderingDevice);
	}
	void CreateRayTracingAccelerationStructures() {
		
		// Bottom level Acceleration structures
		for (auto& blas : rayTracingBottomLevelAccelerationStructures) {
			blas.GenerateRayTracingGeometries();
			
			blas.createInfo = {
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV,
				nullptr,// pNext
				0,// VkDeviceSize compactedSize
				{// VkAccelerationStructureInfoNV info
					VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV,
					nullptr,// pNext
					VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV,// type
					VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV /*| VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV*/,// flags
					0,// instanceCount
					(uint)blas.rayTracingGeometries.size(),// geometryCount
					blas.rayTracingGeometries.data()// VkGeometryNV pGeometries
				}
			};
			
			if (renderingDevice->CreateAccelerationStructureNV(&blas.createInfo, nullptr, &blas.accelerationStructure) != VK_SUCCESS)
				throw std::runtime_error("Failed to create bottom level acceleration structure");
				
			VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
			memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
			memoryRequirementsInfo.accelerationStructure = blas.accelerationStructure;
			memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;
				
			VkMemoryRequirements2 memoryRequirements2 {};
			renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirements2);
			
			VkMemoryAllocateInfo memoryAllocateInfo {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				nullptr,// pNext
				memoryRequirements2.memoryRequirements.size,// VkDeviceSize allocationSize
				renderingDevice->GetPhysicalDevice()->FindMemoryType(memoryRequirements2.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)// memoryTypeIndex
			};
			if (renderingDevice->AllocateMemory(&memoryAllocateInfo, nullptr, &blas.memory) != VK_SUCCESS)
				throw std::runtime_error("Failed to allocate memory for bottom level acceleration structure");
			
			VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo {
				VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV,
				nullptr,// pNext
				blas.accelerationStructure,// accelerationStructure
				blas.memory,// memory
				0,// VkDeviceSize memoryOffset
				0,// uint32_t deviceIndexCount
				nullptr// pDeviceIndices
			};
			if (renderingDevice->BindAccelerationStructureMemoryNV(1, &accelerationStructureMemoryInfo) != VK_SUCCESS)
				throw std::runtime_error("Failed to bind bottom level acceleration structure memory");
			
			if (renderingDevice->GetAccelerationStructureHandleNV(blas.accelerationStructure, sizeof(uint64_t), &blas.handle))
				throw std::runtime_error("Failed to get bottom level acceleration structure handle");
		}
		
		// Geometry Instances
		rayTracingTopLevelAccelerationStructure.instances.reserve(geometryInstances.size());
		for (auto& instance : geometryInstances) if (instance.rayTracingBlas) {
			instance.rayTracingGeometryInstanceIndex = (int)rayTracingTopLevelAccelerationStructure.instances.size();
			rayTracingTopLevelAccelerationStructure.instances.push_back({
				instance.transform,
				instance.instanceId,
				instance.rayTracingMask,
				instance.rayTracingShaderHitOffset,
				instance.rayTracingFlags,
				instance.rayTracingBlas->handle
			});
		}
		
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
					VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV /*| VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV*/, // VkGeometryInstanceFlagBitsNV flags
					(uint)rayTracingTopLevelAccelerationStructure.instances.size(),// instanceCount
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
				renderingDevice->GetPhysicalDevice()->FindMemoryType(memoryRequirements2.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)// memoryTypeIndex
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
			// Instance buffer
			Buffer instanceBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV);
			instanceBuffer.AddSrcDataPtr(&rayTracingTopLevelAccelerationStructure.instances);
			AllocateBufferStaged(graphicsQueue, instanceBuffer);
			
			VkDeviceSize allBlasReqSize = 0;
			// RayTracing Scratch buffer
			VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
			memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
			memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
			for (auto& blas : rayTracingBottomLevelAccelerationStructures) {
				VkMemoryRequirements2 req;
				memoryRequirementsInfo.accelerationStructure = blas.accelerationStructure;
				renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &req);
				allBlasReqSize += req.memoryRequirements.size;
			}
			VkMemoryRequirements2 memoryRequirementsTopLevel;
			memoryRequirementsInfo.accelerationStructure = rayTracingTopLevelAccelerationStructure.accelerationStructure;
			renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirementsTopLevel);
			// Send scratch buffer
			const VkDeviceSize scratchBufferSize = std::max(allBlasReqSize, memoryRequirementsTopLevel.memoryRequirements.size);
			Buffer scratchBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, scratchBufferSize);
			scratchBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			
			auto cmdBuffer = BeginSingleTimeCommands(graphicsQueue);
				
				VkAccelerationStructureInfoNV accelerationStructBuildInfo {};
				accelerationStructBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
				accelerationStructBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV /*| VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV*/;
			
				VkMemoryBarrier memoryBarrier {
					VK_STRUCTURE_TYPE_MEMORY_BARRIER,
					nullptr,// pNext
					VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,// VkAccessFlags srcAccessMask
					VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,// VkAccessFlags dstAccessMask
				};
				
				for (auto& blas : rayTracingBottomLevelAccelerationStructures) {
					accelerationStructBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
					accelerationStructBuildInfo.geometryCount = (uint)blas.rayTracingGeometries.size();
					accelerationStructBuildInfo.pGeometries = blas.rayTracingGeometries.data();
					accelerationStructBuildInfo.instanceCount = 0;
					
					renderingDevice->CmdBuildAccelerationStructureNV(
						cmdBuffer, 
						&accelerationStructBuildInfo, 
						VK_NULL_HANDLE, 
						0, 
						VK_FALSE, 
						blas.accelerationStructure, 
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
				}
				
				accelerationStructBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
				accelerationStructBuildInfo.geometryCount = 0;
				accelerationStructBuildInfo.pGeometries = nullptr;
				accelerationStructBuildInfo.instanceCount = (uint)rayTracingTopLevelAccelerationStructure.instances.size();
				
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
			
			EndSingleTimeCommands(graphicsQueue, cmdBuffer);
			
			scratchBuffer.Free(renderingDevice);
			instanceBuffer.Free(renderingDevice);
		}
	}
	void DestroyRayTracingAccelerationStructures() {
		// Geometry instances
		for (auto& instance : geometryInstances) if (instance.rayTracingBlas) {
			instance.rayTracingGeometryInstanceIndex = -1;
		}
		// Acceleration Structures
		if (rayTracingTopLevelAccelerationStructure.accelerationStructure != VK_NULL_HANDLE) {
			renderingDevice->FreeMemory(rayTracingTopLevelAccelerationStructure.memory, nullptr);
			renderingDevice->DestroyAccelerationStructureNV(rayTracingTopLevelAccelerationStructure.accelerationStructure, nullptr);
			rayTracingTopLevelAccelerationStructure.accelerationStructure = VK_NULL_HANDLE;
			rayTracingTopLevelAccelerationStructure.instances.clear();
			for (auto& blas : rayTracingBottomLevelAccelerationStructures) {
				renderingDevice->FreeMemory(blas.memory, nullptr);
				renderingDevice->DestroyAccelerationStructureNV(blas.accelerationStructure, nullptr);
				blas.rayTracingGeometries.clear();
			}
		}
	}
	void CreateRayTracingPipeline() {
		shaderBindingTable->CreateRayTracingPipeline(renderingDevice);
		
		// Shader Binding Table
		rayTracingShaderBindingTableBuffer.size = rayTracingProperties.shaderGroupHandleSize * shaderBindingTable->GetGroups().size();
		rayTracingShaderBindingTableBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		shaderBindingTable->WriteShaderBindingTableToBuffer(renderingDevice, &rayTracingShaderBindingTableBuffer, rayTracingProperties.shaderGroupHandleSize);
	}
	void DestroyRayTracingPipeline() {
		// Shader binding table
		rayTracingShaderBindingTableBuffer.Free(renderingDevice);
		// Ray tracing pipeline
		shaderBindingTable->DestroyRayTracingPipeline(renderingDevice);
	}
	
private: // Renderer Configuration methods

	void Init() override {
		RayTracingInit();
		
		// Set all device features that you may want to use, then the unsupported features will be disabled, you may check via this object later.
		// deviceFeatures.geometryShader = VK_TRUE;
		// deviceFeatures.samplerAnisotropy = VK_TRUE;
		// deviceFeatures.sampleRateShading = VK_TRUE;
		
		// Preferences
		preferredPresentModes = {
			// VK_PRESENT_MODE_MAILBOX_KHR,	// TripleBuffering (No Tearing, low latency)
			// VK_PRESENT_MODE_FIFO_KHR,		// VSync ON (No Tearing, more latency)
			VK_PRESENT_MODE_IMMEDIATE_KHR,	// VSync OFF (With Tearing, no latency)
		};
	}
	
	void ScorePhysicalDeviceSelection(int& score, PhysicalDevice* physicalDevice) {
		// Build up a score here and the PhysicalDevice with the highest score will be selected.
		// Add to the score optional specs, then multiply with mandatory specs.
		
		// Optional specs  -->  score += points * CONDITION
		score += 10 * (physicalDevice->GetProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU); // Is a Dedicated GPU
		// score += 20 * physicalDevice->GetFeatures().tessellationShader; // Supports Tessellation
		// score += physicalDevice->GetProperties().limits.framebufferColorSampleCounts; // Add sample counts to the score (1-64)

		// Mandatory specs  -->  score *= CONDITION
		// score *= physicalDevice->GetFeatures().geometryShader; // Supports Geometry Shaders
		// score *= physicalDevice->GetFeatures().samplerAnisotropy; // Supports Anisotropic filtering
		// score *= physicalDevice->GetFeatures().sampleRateShading; // Supports Sample Shading
	}
	
	void Info() override {
		RayTracingInfo();
	}
	
	void InitLayouts() override {}
	void ConfigureShaders() override {}

	void CreateResources() override {
		CreateRayTracingResources();
	}
	
	void DestroyResources() override {
		DestroyRayTracingResources();
	}
	
public: // Scene configuration methods
	void LoadScene() override {
		
		// Buffers
		Buffer* vertexBuffer = stagedBuffers.emplace_back(new Buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
		Buffer* indexBuffer = stagedBuffers.emplace_back(new Buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
		Buffer* sphereBuffer = stagedBuffers.emplace_back(new Buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
		
		// Descriptor sets
		auto* descriptorSet = descriptorSets.emplace_back(new DescriptorSet(0));
		descriptorSet->AddBinding_accelerationStructure(0, &rayTracingTopLevelAccelerationStructure.accelerationStructure, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
		descriptorSet->AddBinding_imageView(1, &rayTracingStorageImage.view, VK_SHADER_STAGE_RAYGEN_BIT_NV);
		descriptorSet->AddBinding_uniformBuffer(2, &uniformBuffer, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_MISS_BIT_NV);
		descriptorSet->AddBinding_storageBuffer(3, stagedBuffers[0], VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
		descriptorSet->AddBinding_storageBuffer(4, stagedBuffers[1], VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
		descriptorSet->AddBinding_storageBuffer(5, stagedBuffers[2], VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_INTERSECTION_BIT_NV);
		
		// Pipeline layouts
		rayTracingPipelineLayout.AddDescriptorSet(descriptorSet);

		// Add triangle geometries
		auto* trianglesGeometry1 = new TriangleGeometry<Vertex>({
			{/*pos*/{-0.5,-0.5, 0.0}, /*roughness*/0.0f, /*normal*/{ 0.0, 0.0, 1.0}, /*emissive*/0.0f, /*color*/{1.0, 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 0.5,-0.5, 0.0}, /*roughness*/0.0f, /*normal*/{ 0.0, 0.0, 1.0}, /*emissive*/0.0f, /*color*/{0.0, 1.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 0.5, 0.5, 0.0}, /*roughness*/0.0f, /*normal*/{ 0.0, 0.0, 1.0}, /*emissive*/0.0f, /*color*/{0.0, 0.0, 1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{-0.5, 0.5, 0.0}, /*roughness*/0.0f, /*normal*/{ 0.0, 0.0, 1.0}, /*emissive*/0.0f, /*color*/{0.0, 1.0, 1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			//
			{/*pos*/{-0.5,-0.5,-0.5}, /*roughness*/0.0f, /*normal*/{ 0.0, 0.0, 1.0}, /*emissive*/0.0f, /*color*/{1.0, 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 0.5,-0.5,-0.5}, /*roughness*/0.0f, /*normal*/{ 0.0, 0.0, 1.0}, /*emissive*/0.0f, /*color*/{0.0, 1.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{ 0.5, 0.5,-0.5}, /*roughness*/0.0f, /*normal*/{ 0.0, 0.0, 1.0}, /*emissive*/0.0f, /*color*/{0.0, 0.0, 1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			{/*pos*/{-0.5, 0.5,-0.5}, /*roughness*/0.0f, /*normal*/{ 0.0, 0.0, 1.0}, /*emissive*/0.0f, /*color*/{0.0, 1.0, 1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.2f},
			
			// bottom white
			/*  8 */{/*pos*/{-8.0,-8.0,-2.0}, /*roughness*/0.5f, /*normal*/{ 0.0, 0.0, 1.0}, /*emissive*/0.0f, /*color*/{1.0,1.0,1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.1f},
			/*  9 */{/*pos*/{ 8.0,-8.0,-2.0}, /*roughness*/0.5f, /*normal*/{ 0.0, 0.0, 1.0}, /*emissive*/0.0f, /*color*/{1.0,1.0,1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.1f},
			/* 10 */{/*pos*/{ 8.0, 8.0,-2.0}, /*roughness*/0.5f, /*normal*/{ 0.0, 0.0, 1.0}, /*emissive*/0.0f, /*color*/{1.0,1.0,1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.1f},
			/* 11 */{/*pos*/{-8.0, 8.0,-2.0}, /*roughness*/0.5f, /*normal*/{ 0.0, 0.0, 1.0}, /*emissive*/0.0f, /*color*/{1.0,1.0,1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.1f},
			
			// top gray
			/* 12 */{/*pos*/{-8.0,-8.0, 4.0}, /*roughness*/0.0f, /*normal*/{ 0.0, 0.0,-1.0}, /*emissive*/0.0f, /*color*/{0.5,0.5,0.5, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.4f},
			/* 13 */{/*pos*/{ 8.0,-8.0, 4.0}, /*roughness*/0.0f, /*normal*/{ 0.0, 0.0,-1.0}, /*emissive*/0.0f, /*color*/{0.5,0.5,0.5, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.4f},
			/* 14 */{/*pos*/{ 8.0, 8.0, 4.0}, /*roughness*/0.0f, /*normal*/{ 0.0, 0.0,-1.0}, /*emissive*/0.0f, /*color*/{0.5,0.5,0.5, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.4f},
			/* 15 */{/*pos*/{-8.0, 8.0, 4.0}, /*roughness*/0.0f, /*normal*/{ 0.0, 0.0,-1.0}, /*emissive*/0.0f, /*color*/{0.5,0.5,0.5, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.4f},
			
			// left red
			/* 16 */{/*pos*/{ 8.0, 8.0,-2.0}, /*roughness*/0.0f, /*normal*/{-1.0, 0.0, 0.0}, /*emissive*/0.0f, /*color*/{1.0,0.0,0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.0f},
			/* 17 */{/*pos*/{ 8.0,-8.0,-2.0}, /*roughness*/0.0f, /*normal*/{-1.0, 0.0, 0.0}, /*emissive*/0.0f, /*color*/{1.0,0.0,0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.0f},
			/* 18 */{/*pos*/{ 8.0, 8.0, 4.0}, /*roughness*/0.0f, /*normal*/{-1.0, 0.0, 0.0}, /*emissive*/0.0f, /*color*/{1.0,0.0,0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.0f},
			/* 19 */{/*pos*/{ 8.0,-8.0, 4.0}, /*roughness*/0.0f, /*normal*/{-1.0, 0.0, 0.0}, /*emissive*/0.0f, /*color*/{1.0,0.0,0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.0f},
			
			// back blue
			/* 20 */{/*pos*/{ 8.0,-8.0, 4.0}, /*roughness*/0.0f, /*normal*/{ 0.0, 1.0, 0.0}, /*emissive*/0.0f, /*color*/{0.0,0.0,1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.0f},
			/* 21 */{/*pos*/{ 8.0,-8.0,-2.0}, /*roughness*/0.0f, /*normal*/{ 0.0, 1.0, 0.0}, /*emissive*/0.0f, /*color*/{0.0,0.0,1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.0f},
			/* 22 */{/*pos*/{-8.0,-8.0, 4.0}, /*roughness*/0.0f, /*normal*/{ 0.0, 1.0, 0.0}, /*emissive*/0.0f, /*color*/{0.0,0.0,1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.0f},
			/* 23 */{/*pos*/{-8.0,-8.0,-2.0}, /*roughness*/0.0f, /*normal*/{ 0.0, 1.0, 0.0}, /*emissive*/0.0f, /*color*/{0.0,0.0,1.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.0f},
			
			// right green
			/* 24 */{/*pos*/{-8.0, 8.0,-2.0}, /*roughness*/0.0f, /*normal*/{ 1.0, 0.0, 0.0}, /*emissive*/0.0f, /*color*/{0.0,1.0,0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.0f},
			/* 25 */{/*pos*/{-8.0,-8.0,-2.0}, /*roughness*/0.0f, /*normal*/{ 1.0, 0.0, 0.0}, /*emissive*/0.0f, /*color*/{0.0,1.0,0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.0f},
			/* 26 */{/*pos*/{-8.0, 8.0, 4.0}, /*roughness*/0.0f, /*normal*/{ 1.0, 0.0, 0.0}, /*emissive*/0.0f, /*color*/{0.0,1.0,0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.0f},
			/* 27 */{/*pos*/{-8.0,-8.0, 4.0}, /*roughness*/0.0f, /*normal*/{ 1.0, 0.0, 0.0}, /*emissive*/0.0f, /*color*/{0.0,1.0,0.0, 1.0}, /*uv*/{0.0, 0.0}, /*specular*/1.0f, /*metallic*/0.0f},
		}, {
			0, 1, 2, 2, 3, 0,
			4, 5, 6, 6, 7, 4,
			8, 9, 10, 10, 11, 8,
			//
			13, 12, 14, 14, 12, 15,
			16, 17, 18, 18, 17, 19,
			20, 21, 22, 22, 21, 23,
			25, 24, 26, 26, 27, 25,
		}, vertexBuffer, 0, indexBuffer, 0);
		geometries.push_back(trianglesGeometry1);
		
		// Add procedural geometries
		auto* spheresGeometry1 = new ProceduralGeometry<Sphere>({
			{/*emissive*/0.0f, /*roughness*/0.0f, /*pos*/{ 0.0, 0.0, 0.8}, /*radius*/0.4f, /*color*/{1.0, 1.0, 1.0, 1.0}, /*specular*/1.0f, /*metallic*/1.0f, /*refraction*/0.5f, /*density*/0.5f},
			{/*emissive*/0.0f, /*roughness*/0.99f, /*pos*/{-2.0,-1.0, 1.5}, /*radius*/0.4f, /*color*/{1.0, 0.8, 0.0, 1.0}, /*specular*/1.0f, /*metallic*/0.4f, /*refraction*/0.5f, /*density*/0.5f},
			{/*emissive*/0.0f, /*roughness*/0.5f, /*pos*/{ 1.0,-1.0, 1.5}, /*radius*/0.6f, /*color*/{0.5, 0.5, 0.5, 1.0}, /*specular*/1.0f, /*metallic*/0.9f, /*refraction*/0.5f, /*density*/0.5f},
			{/*emissive*/0.0f, /*roughness*/0.5f, /*pos*/{ 2.0,-1.0, 0.5}, /*radius*/0.3f, /*color*/{1.0, 1.0, 1.0, 1.0}, /*specular*/1.0f, /*metallic*/0.1f, /*refraction*/0.5f, /*density*/0.5f},
		}, sphereBuffer);
		geometries.push_back(spheresGeometry1);

		// Assign geometries to acceleration structures
		rayTracingBottomLevelAccelerationStructures.resize(2);
		rayTracingBottomLevelAccelerationStructures[0].geometries.push_back(trianglesGeometry1);
		rayTracingBottomLevelAccelerationStructures[1].geometries.push_back(spheresGeometry1);
		
		// Assign buffer data
		vertexBuffer->AddSrcDataPtr(&trianglesGeometry1->vertexData);
		indexBuffer->AddSrcDataPtr(&trianglesGeometry1->indexData);
		sphereBuffer->AddSrcDataPtr(&spheresGeometry1->aabbData);

		// Ray tracing shaders
		shaderBindingTable = new ShaderBindingTable(rayTracingPipelineLayout, "incubator_rendering/assets/shaders/rtx.rgen");
		shaderBindingTable->AddMissShader("incubator_rendering/assets/shaders/rtx.rmiss");
		shaderBindingTable->AddMissShader("incubator_rendering/assets/shaders/rtx.shadow.rmiss");
		uint32_t trianglesShaderOffset = shaderBindingTable->AddHitShader("incubator_rendering/assets/shaders/rtx.rchit");
		uint32_t spheresShaderOffset = shaderBindingTable->AddHitShader("incubator_rendering/assets/shaders/rtx.sphere.rchit", "", "incubator_rendering/assets/shaders/rtx.sphere.rint");
		
		// Assign instances
		glm::mat3x4 transform {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f
		};
		geometryInstances.reserve(2);
		// Triangles instance
		geometryInstances.push_back({
			transform,
			0, // instanceId
			0x1, // mask
			trianglesShaderOffset, // instanceOffset
			VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV, // VkGeometryInstanceFlagBitsNV flags
			&rayTracingBottomLevelAccelerationStructures[0]
		});
		// Spheres instance
		geometryInstances.push_back({
			transform,
			0, // instanceId
			0x2, // mask
			spheresShaderOffset, // instanceOffset
			0, // flags
			&rayTracingBottomLevelAccelerationStructures[1]
		});
		
	}

	void UnloadScene() override {
		// Shaders
		delete shaderBindingTable;
		
		// Geometries
		geometryInstances.clear();
		rayTracingBottomLevelAccelerationStructures.clear();
		for (auto* geometry : geometries) {
			delete geometry;
		}
		geometries.clear();
		
		// Staged buffers
		for (auto* buffer : stagedBuffers) {
			delete buffer;
		}
		stagedBuffers.clear();
		
		// Pipeline Layouts
		rayTracingPipelineLayout.Reset();
		
		// Descriptor sets
		for (auto* set : descriptorSets) {
			delete set;
		}
		descriptorSets.clear();
	}
	
	void ReadShaders() override {
		shaderBindingTable->ReadShaders();
	}

protected: // Graphics configuration
	void AllocateBuffers() override {
		// Staged Buffers
		AllocateBuffersStaged(graphicsQueue, stagedBuffers);
		// Uniform buffer
		uniformBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		uniformBuffer.MapMemory(renderingDevice);
		
		CreateRayTracingAccelerationStructures();
	}

	void FreeBuffers() override {
		// Ray Tracing
		DestroyRayTracingAccelerationStructures();
		
		for (auto& buffer : stagedBuffers) {
			buffer->Free(renderingDevice);
		}

		// Uniform buffers
		uniformBuffer.UnmapMemory(renderingDevice);
		uniformBuffer.Free(renderingDevice);
	}
	
	void CreatePipelines() override {
		rayTracingPipelineLayout.Create(renderingDevice);
		CreateRayTracingPipeline();
	}
	
	void DestroyPipelines() override {
		DestroyRayTracingPipeline();
		rayTracingPipelineLayout.Destroy(renderingDevice);
	}
	
private: // Commands
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		
		renderingDevice->CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, shaderBindingTable->GetPipeline());
		shaderBindingTable->GetPipelineLayout()->Bind(renderingDevice, commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV);
		
		VkDeviceSize bindingStride = rayTracingProperties.shaderGroupHandleSize;
		VkDeviceSize bindingOffsetMissShader = bindingStride * shaderBindingTable->GetMissGroupOffset();
		VkDeviceSize bindingOffsetHitShader = bindingStride * shaderBindingTable->GetHitGroupOffset();
		
		int width = (int)((float)swapChain->extent.width * rayTracingImageScale);
		int height = (int)((float)swapChain->extent.height * rayTracingImageScale);
		
		renderingDevice->CmdTraceRaysNV(
			commandBuffer, 
			rayTracingShaderBindingTableBuffer.buffer, 0,
			rayTracingShaderBindingTableBuffer.buffer, bindingOffsetMissShader, bindingStride,
			rayTracingShaderBindingTableBuffer.buffer, bindingOffsetHitShader, bindingStride,
			VK_NULL_HANDLE, 0, 0,
			width, height, 1
		);
		
		TransitionImageLayout(commandBuffer, swapChain->images[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 1);
		TransitionImageLayout(commandBuffer, rayTracingStorageImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		
	
		VkImageBlit blit = {};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { width, height, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = 0;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { (int)swapChain->extent.width, (int)swapChain->extent.height, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = 0;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;
		renderingDevice->CmdBlitImage(
			commandBuffer,
			rayTracingStorageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			swapChain->images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR
		);
		
		// VkImageCopy copyRegion {};
		// copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		// copyRegion.srcOffset = {0,0,0};
		// copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		// copyRegion.dstOffset = {0,0,0};
		// copyRegion.extent = {swapChain->extent.width, swapChain->extent.height, 1};
		// renderingDevice->CmdCopyImage(commandBuffer, rayTracingStorageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapChain->images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
		
		TransitionImageLayout(commandBuffer, swapChain->images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1, 1);
		TransitionImageLayout(commandBuffer, rayTracingStorageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	}
	
	void RunDynamicGraphics(VkCommandBuffer) override {}
	void RunDynamicLowPriorityCompute(VkCommandBuffer) override {}
	void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {}
	
protected: // Methods executed on every frame

	void FrameUpdate(uint imageIndex) override {
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		double time = std::chrono::duration<double, std::chrono::seconds::period>(currentTime - startTime).count();
		UBO ubo = {};
		// Slowly rotate the test object
		// ubo.model = glm::rotate(glm::dmat4(1), time * glm::radians(10.0), glm::dvec3(0,0,1));
		
		// Current camera position
		ubo.viewInverse = glm::inverse(glm::lookAt(camPosition, camPosition + camDirection, glm::dvec3(0,0,1)));
		// Projection
		ubo.projInverse = glm::inverse(glm::perspective(glm::radians(80.0), (double) swapChain->extent.width / (double) swapChain->extent.height, 0.01, 1.5e17)); // 1cm - 1 000 000 UA
		ubo.projInverse[1][1] *= -1;

		ubo.light = light;
		ubo.ambient = glm::vec3(0.0f, 0.0f, 0.0f);
		ubo.samplesPerPixel = samplesPerPixel;
		ubo.rtx_reflection_max_recursion = rtx_reflection_max_recursion;
		ubo.rtx_shadows = rtx_shadows;
		
		ubo.time = time;
		
		// Update memory
		uniformBuffer.WriteToMappedData(renderingDevice, &ubo);
	}
	
	void LowPriorityFrameUpdate() override {}

public: // user-defined state variables
	glm::dvec3 camPosition = glm::dvec3(2,2,2);
	glm::dvec3 camDirection = glm::dvec3(-2,-2,-2);
	glm::dvec4 light {1.0,1.0,3.0, 1.0};
	int samplesPerPixel = 2;
	int rtx_reflection_max_recursion = 4;
	bool rtx_shadows = true;
	
	// Just for compatibility with the other test renderer...
	glm::dvec3 velocity = glm::dvec3(0);
	bool toggleTest = false;
	bool continuousGalaxyGen = true;
	float speed = 0;
	
};
