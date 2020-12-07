#pragma once

#include <v4d.h>
// #include "utilities/data/EntityComponentSystem.hpp"

namespace Mesh {
	typedef uint32_t Index;
	struct VertexPosition {
		float x, y, z;
		VertexPosition(float x, float y, float z) : x(x), y(y), z(z) {}
		VertexPosition() {static_assert(sizeof(VertexPosition) == 12);}
		static std::vector<VertexInputAttributeDescription> GetInputAttributes() {
			return {
				{0, offsetof(VertexPosition, x), VK_FORMAT_R32G32B32_SFLOAT},
			};
		}
	};
	struct VertexNormal {
		float x;
		float y;
		float z;
		VertexNormal(float x, float y, float z) : x(x), y(y), z(z) {}
		VertexNormal() {static_assert(sizeof(VertexNormal) == 12);}
	};
	struct VertexColor {
		float r;
		float g;
		float b;
		float a;
		VertexColor(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}
		VertexColor() {static_assert(sizeof(VertexColor) == 16);}
	};
	struct VertexUV {
		float s;
		float t;
		VertexUV(float s, float t) : s(s), t(t) {}
		VertexUV() {static_assert(sizeof(VertexUV) == 8);}
	};
	struct ProceduralVertexAABB {
		glm::vec3 aabbMin, aabbMax;
		ProceduralVertexAABB(glm::vec3 aabbMin, glm::vec3 aabbMax) : aabbMin(aabbMin), aabbMax(aabbMax) {}
		ProceduralVertexAABB() {static_assert(sizeof(ProceduralVertexAABB) == 24);}
		static std::vector<VertexInputAttributeDescription> GetInputAttributes() {
			return {
				{0, offsetof(ProceduralVertexAABB, aabbMin), VK_FORMAT_R32G32B32_SFLOAT},
				{1, offsetof(ProceduralVertexAABB, aabbMax), VK_FORMAT_R32G32B32_SFLOAT},
			};
		}
	};
	struct ModelInfo {
		uint64_t moduleVen;
		uint64_t moduleId;
		uint64_t objId;
		uint64_t customData;
		VkDeviceAddress indices {};
		VkDeviceAddress vertexPositions {};
		VkDeviceAddress vertexNormals {};
		VkDeviceAddress vertexColors {};
		VkDeviceAddress vertexUVs {};
		VkDeviceAddress transform;
		ModelInfo() {static_assert(sizeof(ModelInfo) == 80);}
	};
	struct ModelTransform {
		alignas(128) glm::dmat4 worldTransform;
		alignas(64) glm::mat4 modelView;
		alignas(64) glm::mat4 normalView;
		ModelTransform(const glm::dmat4& m) : worldTransform(m) {}
		ModelTransform() {static_assert(sizeof(ModelTransform) == 256);}
	};
	
	template<typename T>
	struct DataBuffer {
		T* data = nullptr;
		size_t count = 0;
		VkBuffer hostBuffer = VK_NULL_HANDLE;
		VkBuffer deviceBuffer = VK_NULL_HANDLE;
		MemoryAllocation hostBufferAllocation = VK_NULL_HANDLE;
		MemoryAllocation deviceBufferAllocation = VK_NULL_HANDLE;
		bool dirtyOnDevice = true;
		
		DataBuffer() {}
		
		void AllocateBuffers(Device* device, const std::initializer_list<T>& list) {
			AllocateBuffers(device, list.size());
			memcpy(data, list.begin(), list.size() * sizeof(T));
		}
		void AllocateBuffers(Device* device, const std::vector<T>& list) {
			AllocateBuffers(device, list.size());
			memcpy(data, list.data(), list.size() * sizeof(T));
		}
		void AllocateBuffers(Device* device, T* inputDataOrArray, size_t elementCount = 1) {
			AllocateBuffers(device, elementCount);
			memcpy(data, inputDataOrArray, elementCount * sizeof(T));
		}
		template<typename _T>
		void AllocateBuffers(Device* device, _T&& value) {
			AllocateBuffers(device, 1);
			*data = std::forward<_T>(value);
		}
		void AllocateBuffers(Device* device, size_t elementCount = 1) {
			count = elementCount;
			
			// Host buffer
			{VkBufferCreateInfo createInfo {};
				createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				createInfo.size = count * sizeof(T);
				createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
				device->CreateAndAllocateBuffer(createInfo, MemoryUsage::MEMORY_USAGE_CPU_ONLY, hostBuffer, &hostBufferAllocation);
			}
			
			// Map data pointer to host buffer
			device->MapMemoryAllocation(hostBufferAllocation, (void**)&data);

			// Device buffer
			{VkBufferCreateInfo createInfo {};
				createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				createInfo.size = count * sizeof(T);
				createInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
				if constexpr (std::is_same_v<T, Mesh::VertexPosition>) {
					createInfo.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
				}
				if constexpr (std::is_same_v<T, Mesh::Index>) {
					createInfo.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
				}
				if constexpr (std::is_same_v<T, Mesh::ProceduralVertexAABB>) {
					createInfo.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
				}
				device->CreateAndAllocateBuffer(createInfo, MemoryUsage::MEMORY_USAGE_GPU_ONLY, deviceBuffer, &deviceBufferAllocation);
			}
		}
		void Upload(Device* device, VkCommandBuffer commandBuffer) {
			if (deviceBuffer && dirtyOnDevice) {
				VkBufferCopy region = {};{
					region.srcOffset = 0;
					region.dstOffset = 0;
					region.size = count * sizeof(T);
				}
				device->CmdCopyBuffer(commandBuffer, hostBuffer, deviceBuffer, 1, &region);
				dirtyOnDevice = false;
			}
		}
		void Download(Device* device, VkCommandBuffer commandBuffer) {
			VkBufferCopy region = {};{
				region.srcOffset = 0;
				region.dstOffset = 0;
				region.size = count * sizeof(T);
			}
			device->CmdCopyBuffer(commandBuffer, deviceBuffer, hostBuffer, 1, &region);
		}
		void FreeBuffers(Device* device) {
			if (data) {
				device->UnmapMemoryAllocation(hostBufferAllocation);
				data = nullptr;
			}
			if (hostBuffer && hostBufferAllocation) {
				device->FreeAndDestroyBuffer(hostBuffer, hostBufferAllocation);
			}
			if (deviceBuffer && deviceBufferAllocation) {
				device->FreeAndDestroyBuffer(deviceBuffer, deviceBufferAllocation);
			}
			dirtyOnDevice = true;
		}
		T* operator->() {
			return data;
		}
		template<typename _T>
		void Set(uint32_t index, _T&& value) {
			assert(data != nullptr && index < count);
			data[index] = std::forward<_T>(value);
		}
		template<typename _T>
		void Set(_T&& value) {
			assert(data != nullptr && count > 0);
			*data = std::forward<_T>(value);
		}
	};

	namespace Material {
		
	}
}
