#pragma once

// class Geometry {
// public:
// 	VkGeometryFlagBitsNV rayTracingGeometryFlags = VK_GEOMETRY_OPAQUE_BIT_NV;

// 	virtual void* GetData() = 0;
// 	virtual void* GetIndexData() = 0;
// 	virtual VkGeometryNV GetRayTracingGeometry() const = 0;
	
// 	virtual ~Geometry(){}
// };

// template<class V, class I = uint32_t, VkFormat f = VK_FORMAT_R32G32B32_SFLOAT>
// class TriangleGeometry : public Geometry {
// public:
	
// 	std::vector<V, std::allocator<V>> vertexData {};
// 	std::vector<I, std::allocator<I>> indexData {};
	
// 	Buffer* vertexBuffer = nullptr;
// 	VkDeviceSize vertexOffset = 0;
// 	VkDeviceSize vertexCount = 0;
	
// 	Buffer* indexBuffer = nullptr;
// 	VkDeviceSize indexOffset = 0;
// 	VkDeviceSize indexCount = 0;

// 	Buffer* transformBuffer = nullptr;
// 	VkDeviceSize transformOffset = 0;
	
// 	void* GetData() override {
// 		return vertexData.data();
// 	}
	
// 	void* GetIndexData() override {
// 		return indexData.data();
// 	}
	
// 	VkGeometryNV GetRayTracingGeometry() const override {
// 		if (vertexBuffer->buffer == VK_NULL_HANDLE)
// 			throw std::runtime_error("Vertex Buffer for aabb geometry has not been created");
// 		if (indexBuffer->buffer == VK_NULL_HANDLE)
// 			throw std::runtime_error("Index Buffer for aabb geometry has not been created");
		
// 		VkGeometryNV triangleGeometry {};
// 		triangleGeometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
// 		triangleGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
// 		triangleGeometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
// 		triangleGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
// 		triangleGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_NV;
		
// 		triangleGeometry.geometry.triangles.vertexOffset = vertexOffset;
// 		triangleGeometry.geometry.triangles.vertexCount = (uint)vertexData.size();
// 		triangleGeometry.geometry.triangles.vertexStride = sizeof(V);
// 		triangleGeometry.geometry.triangles.vertexFormat = f;
// 		triangleGeometry.geometry.triangles.indexOffset = indexOffset;
// 		triangleGeometry.geometry.triangles.indexCount = (uint)indexData.size();
// 		triangleGeometry.geometry.triangles.indexType = sizeof(I) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
		
// 		triangleGeometry.geometry.triangles.vertexData = vertexBuffer->buffer;
// 		triangleGeometry.geometry.triangles.indexData = indexBuffer->buffer;
		
// 		triangleGeometry.geometry.triangles.transformData = transformBuffer? transformBuffer->buffer : VK_NULL_HANDLE;
// 		triangleGeometry.geometry.triangles.transformOffset = transformOffset;
		
// 		return triangleGeometry;
// 	}
	
// 	TriangleGeometry(){}
// 	TriangleGeometry(std::vector<V>&& vertexData, std::vector<I>&& indexData, Buffer* vertexBuffer, VkDeviceSize vertexOffset, Buffer* indexBuffer, VkDeviceSize indexOffset, Buffer* transformBuffer = nullptr, VkDeviceSize transformOffset = 0)
// 	 : vertexData(std::forward<std::vector<V>>(vertexData)), indexData(std::forward<std::vector<I>>(indexData)) {
// 		this->vertexBuffer = vertexBuffer;
// 		this->indexBuffer = indexBuffer;
// 		this->transformBuffer = transformBuffer;
// 		this->vertexOffset = vertexOffset;
// 		this->indexOffset = indexOffset;
// 		this->transformOffset = transformOffset;
// 		this->vertexCount = vertexData.size();
// 		this->indexCount = indexData.size();
// 	}
// };

// struct ProceduralGeometryData {
// 	std::pair<glm::vec3, glm::vec3> boundingBox;
// 	ProceduralGeometryData(glm::vec3 min, glm::vec3 max) : boundingBox(min, max) {}
// };

// template<class V>
// class ProceduralGeometry : public Geometry {
// public:
// 	std::vector<V, std::allocator<V>> aabbData {};
	
// 	Buffer* aabbBuffer = nullptr;
// 	VkDeviceSize aabbOffset = 0;
// 	VkDeviceSize aabbCount = 0;
	
// 	void* GetData() override {
// 		return aabbData.data();
// 	}
	
// 	void* GetIndexData() override {
// 		return nullptr;
// 	}
	
// 	VkGeometryNV GetRayTracingGeometry() const override {
// 		if (aabbBuffer->buffer == VK_NULL_HANDLE)
// 			throw std::runtime_error("Buffer for aabb geometry has not been created");
		
// 		VkGeometryNV geometry {};
// 		geometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
// 		geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
// 		geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
// 		geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_NV;
// 		geometry.flags = VK_GEOMETRY_OPAQUE_BIT_NV;
		
// 		geometry.geometry.aabbs.numAABBs = (uint)aabbData.size();
// 		geometry.geometry.aabbs.stride = sizeof(V);
// 		geometry.geometry.aabbs.offset = aabbOffset;
		
// 		geometry.geometry.aabbs.aabbData = aabbBuffer->buffer;
		
// 		return geometry;
// 	}
	
// 	ProceduralGeometry(){}
// 	ProceduralGeometry(std::vector<V>&& aabbData, Buffer* aabbBuffer, VkDeviceSize aabbOffset = 0)
// 	 : aabbData(std::forward<std::vector<V>>(aabbData)) {
// 		this->aabbBuffer = aabbBuffer;
// 		this->aabbOffset = aabbOffset;
// 		this->aabbCount = aabbData.size();
// 	}
// 	ProceduralGeometry(std::vector<V>& aabbData, Buffer* aabbBuffer, VkDeviceSize aabbOffset = 0)
// 	 : aabbData(std::forward<std::vector<V>>(aabbData)) {
// 		this->aabbBuffer = aabbBuffer;
// 		this->aabbOffset = aabbOffset;
// 		this->aabbCount = aabbData.size();
// 	}
// };







glm::vec2 PackNormal(glm::vec3 normal) {
	float f = glm::sqrt(8.0f * normal.z + 8.0f);
	return glm::vec2(normal) / f + 0.5f;
}

glm::u32 PackColor(glm::vec4 color) {
	color *= 255.0f;
	glm::uvec4 pack {
		glm::clamp(glm::u32(color.r), (glm::u32)0, (glm::u32)255),
		glm::clamp(glm::u32(color.g), (glm::u32)0, (glm::u32)255),
		glm::clamp(glm::u32(color.b), (glm::u32)0, (glm::u32)255),
		glm::clamp(glm::u32(color.a), (glm::u32)0, (glm::u32)255),
	};
	return (pack.r << 24) | (pack.g << 16) | (pack.b << 8) | pack.a;
}

glm::u32 PackUV(glm::vec2 uv) {
	uv *= 65535.0f;
	glm::uvec2 pack {
		glm::clamp(glm::u32(uv.s), (glm::u32)0, (glm::u32)65535),
		glm::clamp(glm::u32(uv.t), (glm::u32)0, (glm::u32)65535),
	};
	return (pack.s << 16) | pack.t;
}

glm::vec3 UnpackNormal(glm::vec2 norm) {
	glm::vec2 fenc = norm * 4.0f - 2.0f;
	float f = glm::dot(fenc, fenc);
	float g = glm::sqrt(1.0f - f / 4.0f);
	return glm::vec3(fenc * g, 1.0f - f / 2.0f);
}

glm::vec2 UnpackUV(glm::uint uv) {
	return glm::vec2(
		(uv & 0xffff0000) >> 16,
		(uv & 0x0000ffff) >> 0
	) / 65535.0f;
}

glm::vec4 UnpackColor(glm::uint color) {
	return glm::vec4(
		(color & 0xff000000) >> 24,
		(color & 0x00ff0000) >> 16,
		(color & 0x0000ff00) >> 8,
		(color & 0x000000ff) >> 0
	) / 255.0f;
}

template<class T, VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT>
struct GlobalBuffer : StagedBuffer {
	GlobalBuffer(uint32_t initialBlocks) : StagedBuffer(usage, sizeof(T) * initialBlocks, false) {}
	
	void Push(Device* device, VkCommandBuffer commandBuffer, int offset = 0, int n = 0) {
		if (offset != 0 && n == 0) n = 1;
		Buffer::Copy(device, commandBuffer, stagingBuffer, deviceLocalBuffer, sizeof(T) * n, sizeof(T) * offset, sizeof(T) * offset);
	}
	
	void Pull(Device* device, VkCommandBuffer commandBuffer, int offset = 0, int n = 0) {
		if (offset != 0 && n == 0) n = 1;
		Buffer::Copy(device, commandBuffer, deviceLocalBuffer, stagingBuffer, sizeof(T) * n, sizeof(T) * offset, sizeof(T) * offset);
	}
};

typedef glm::uvec4	GeometryBuffer_T;
typedef glm::u32	IndexBuffer_T;
typedef glm::vec4	VertexBuffer_T;
typedef glm::u32	MaterialBuffer_T;
typedef glm::vec2	NormalBuffer_T;
typedef glm::u32	UVBuffer_T;
typedef glm::u32	ColorBuffer_T;

typedef GlobalBuffer<GeometryBuffer_T> GeometryBuffer;
typedef GlobalBuffer<IndexBuffer_T, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT> IndexBuffer;
typedef GlobalBuffer<VertexBuffer_T, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT> VertexBuffer;
typedef GlobalBuffer<MaterialBuffer_T> MaterialBuffer;
typedef GlobalBuffer<NormalBuffer_T> NormalBuffer;
typedef GlobalBuffer<UVBuffer_T> UVBuffer;
typedef GlobalBuffer<ColorBuffer_T> ColorBuffer;

struct BufferAllocation {
	uint32_t n;
	void* data;
};

class Geometry {
public:
	uint32_t vertexCount;
	uint32_t indexCount;
	
	int geometryOffset = -1;
	int vertexOffset = -1;
	int indexOffset = -1;

	IndexBuffer_T* indices = nullptr;
	VertexBuffer_T* vertexPositions = nullptr;
	MaterialBuffer_T* vertexMaterials = nullptr;
	NormalBuffer_T* vertexNormals = nullptr;
	UVBuffer_T* vertexUVs = nullptr;
	ColorBuffer_T* vertexColors = nullptr;
	
	Buffer* transformBuffer = nullptr;
	VkDeviceSize transformOffset = 0;
	
	class GlobalGeometryBuffers {
		std::mutex geometryBufferMutex, vertexBufferMutex, indexBufferMutex;
		std::map<int, Geometry*> geometryAllocations {};
		std::map<int, BufferAllocation> indexAllocations {};
		std::map<int, BufferAllocation> vertexAllocations {};
		
		static const int nbInitialGeometries = 128;
		static const int nbInitialVertices = nbInitialGeometries * 256;
		static const int nbInitialIndices = nbInitialVertices * 6;
		
	public:
		
		GeometryBuffer geometryBuffer {nbInitialGeometries};
		IndexBuffer indexBuffer {nbInitialIndices};
		VertexBuffer vertexBuffer {nbInitialVertices};
		MaterialBuffer materialBuffer {nbInitialVertices};
		NormalBuffer normalBuffer {nbInitialVertices};
		UVBuffer uvBuffer {nbInitialVertices};
		ColorBuffer colorBuffer {nbInitialVertices};
		
		int/*geometryOffset*/ AddGeometry(Geometry* geometry) {
			std::scoped_lock lock(geometryBufferMutex, vertexBufferMutex, indexBufferMutex);
			
			// Geometry
			geometry->geometryOffset = -1;
			for (auto [i, g] : geometryAllocations) {
				if (!g) {
					geometry->geometryOffset = i;
					break;
				}
			}
			if (geometry->geometryOffset == -1) geometry->geometryOffset = geometryAllocations.size();
			geometryAllocations[geometry->geometryOffset] = geometry;
			
			// Index
			geometry->indexOffset = -1;
			int currentIndexOffset = 0;
			for (auto [i, alloc] : indexAllocations) {
				currentIndexOffset = i;
				if (!alloc.data) {
					if (alloc.n >= geometry->indexCount) {
						geometry->indexOffset = i;
						if (alloc.n > geometry->indexCount) {
							indexAllocations[geometry->indexOffset+geometry->indexCount] = {alloc.n - geometry->indexCount, nullptr};
						}
					}
				}
				currentIndexOffset += alloc.n;
			}
			if (geometry->indexOffset == -1) geometry->indexOffset = currentIndexOffset;
			indexAllocations[geometry->indexOffset] = {geometry->indexCount, geometry};
			
			// Vertex
			geometry->vertexOffset = -1;
			int currentVertexOffset = 0;
			for (auto [i, alloc] : vertexAllocations) {
				currentVertexOffset = i;
				if (!alloc.data) {
					if (alloc.n >= geometry->vertexCount) {
						geometry->vertexOffset = i;
						if (alloc.n > geometry->vertexCount) {
							vertexAllocations[geometry->vertexOffset+geometry->vertexCount] = {alloc.n - geometry->vertexCount, nullptr};
						}
					}
				}
				currentVertexOffset += alloc.n;
			}
			if (geometry->vertexOffset == -1) geometry->vertexOffset = currentVertexOffset;
			vertexAllocations[geometry->vertexOffset] = {geometry->vertexCount, geometry};
			
			// Check for overflow
			if (geometryBuffer.stagingBuffer.size < geometry->geometryOffset * sizeof(GeometryBuffer_T)) 
				FATAL("Global Geometry buffer overflow" )
			if (indexBuffer.stagingBuffer.size < geometry->indexOffset * sizeof(IndexBuffer_T)) 
				FATAL("Global Index buffer overflow" )
			if (vertexBuffer.stagingBuffer.size < geometry->vertexOffset * sizeof(VertexBuffer_T)) 
				FATAL("Global Vertex buffer overflow" )
			if (materialBuffer.stagingBuffer.size < geometry->vertexOffset * sizeof(MaterialBuffer_T)) 
				FATAL("Global Material buffer overflow" )
			if (normalBuffer.stagingBuffer.size < geometry->vertexOffset * sizeof(NormalBuffer_T)) 
				FATAL("Global Normal buffer overflow" )
			if (uvBuffer.stagingBuffer.size < geometry->vertexOffset * sizeof(UVBuffer_T)) 
				FATAL("Global UV buffer overflow" )
			if (colorBuffer.stagingBuffer.size < geometry->vertexOffset * sizeof(ColorBuffer_T)) 
				FATAL("Global Color buffer overflow" )
			
			//TODO dynamically allocate more memory instead of crashing
			
			//
			return geometry->geometryOffset;
		}
		
		void RemoveGeometry(Geometry* geometry) {
			std::scoped_lock lock(geometryBufferMutex, vertexBufferMutex, indexBufferMutex);
			if (geometry->geometryOffset != -1) {
				geometryAllocations[geometry->geometryOffset] = nullptr;
				indexAllocations[geometry->indexOffset].data = nullptr;
				vertexAllocations[geometry->vertexOffset].data = nullptr;
			}
		}
		
		void Allocate(Device* device) {
			geometryBuffer.Allocate(device);
			indexBuffer.Allocate(device);
			vertexBuffer.Allocate(device);
			materialBuffer.Allocate(device);
			normalBuffer.Allocate(device);
			uvBuffer.Allocate(device);
			colorBuffer.Allocate(device);
		}
		
		void Free(Device* device) {
			geometryBuffer.Free(device);
			indexBuffer.Free(device);
			vertexBuffer.Free(device);
			materialBuffer.Free(device);
			normalBuffer.Free(device);
			uvBuffer.Free(device);
			colorBuffer.Free(device);
		}
		
		void PushAllGeometries(Device* device, VkCommandBuffer commandBuffer) {
			geometryBuffer.Push(device, commandBuffer);
			indexBuffer.Push(device, commandBuffer);
			vertexBuffer.Push(device, commandBuffer);
			materialBuffer.Push(device, commandBuffer);
			normalBuffer.Push(device, commandBuffer);
			uvBuffer.Push(device, commandBuffer);
			colorBuffer.Push(device, commandBuffer);
		}
		
		void PullAllGeometries(Device* device, VkCommandBuffer commandBuffer) {
			geometryBuffer.Pull(device, commandBuffer);
			indexBuffer.Pull(device, commandBuffer);
			vertexBuffer.Pull(device, commandBuffer);
			materialBuffer.Pull(device, commandBuffer);
			normalBuffer.Pull(device, commandBuffer);
			uvBuffer.Pull(device, commandBuffer);
			colorBuffer.Pull(device, commandBuffer);
		}
		
		void PushGeometry(Device* device, VkCommandBuffer commandBuffer, Geometry* geometry) {
			geometryBuffer.Push(device, commandBuffer, geometry->geometryOffset);
			indexBuffer.Push(device, commandBuffer, geometry->indexOffset);
			vertexBuffer.Push(device, commandBuffer, geometry->vertexOffset);
			materialBuffer.Push(device, commandBuffer, geometry->vertexOffset);
			normalBuffer.Push(device, commandBuffer, geometry->vertexOffset);
			uvBuffer.Push(device, commandBuffer, geometry->vertexOffset);
			colorBuffer.Push(device, commandBuffer, geometry->vertexOffset);
		}
		
		void PullGeometry(Device* device, VkCommandBuffer commandBuffer, Geometry* geometry) {
			geometryBuffer.Pull(device, commandBuffer, geometry->geometryOffset);
			indexBuffer.Pull(device, commandBuffer, geometry->indexOffset);
			vertexBuffer.Pull(device, commandBuffer, geometry->vertexOffset);
			materialBuffer.Pull(device, commandBuffer, geometry->vertexOffset);
			normalBuffer.Pull(device, commandBuffer, geometry->vertexOffset);
			uvBuffer.Pull(device, commandBuffer, geometry->vertexOffset);
			colorBuffer.Pull(device, commandBuffer, geometry->vertexOffset);
		}
		
		void DefragmentMemory() {
			std::scoped_lock lock(geometryBufferMutex, vertexBufferMutex, indexBufferMutex);
			//TODO merge free space, and maybe trim the end
		}

	};

	static GlobalGeometryBuffers globalBuffers;
	
	VkGeometryNV GetRayTracingGeometry() const {
		VkGeometryNV triangleGeometry {};
		triangleGeometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
		triangleGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
		triangleGeometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
		triangleGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
		triangleGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_NV;
		
		triangleGeometry.geometry.triangles.vertexOffset = (VkDeviceSize)(vertexOffset*sizeof(VertexBuffer_T));
		triangleGeometry.geometry.triangles.vertexCount = vertexCount;
		triangleGeometry.geometry.triangles.vertexStride = sizeof(glm::vec4);
		triangleGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		triangleGeometry.geometry.triangles.indexOffset = (VkDeviceSize)(indexOffset*sizeof(IndexBuffer_T));
		triangleGeometry.geometry.triangles.indexCount = indexCount;
		triangleGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
		
		triangleGeometry.geometry.triangles.vertexData = globalBuffers.vertexBuffer.deviceLocalBuffer.buffer;
		triangleGeometry.geometry.triangles.indexData = globalBuffers.indexBuffer.deviceLocalBuffer.buffer;
		
		triangleGeometry.geometry.triangles.transformData = transformBuffer? transformBuffer->buffer : VK_NULL_HANDLE;
		triangleGeometry.geometry.triangles.transformOffset = transformOffset;
		
		return triangleGeometry;
	}
	
	Geometry(uint32_t vertexCount, uint32_t indexCount) : vertexCount(vertexCount), indexCount(indexCount) {
		globalBuffers.AddGeometry(this);
	}
	
	void MapStagingBuffers() {
		if (!globalBuffers.geometryBuffer.stagingBuffer.data) {
			LOG_ERROR("global buffers must be allocated before mapping staging buffers")
			return;
		}
		indices = &((IndexBuffer_T*)(globalBuffers.geometryBuffer.stagingBuffer.data))[indexOffset];
		vertexPositions = &((VertexBuffer_T*)(globalBuffers.geometryBuffer.stagingBuffer.data))[vertexOffset];
		vertexMaterials = &((MaterialBuffer_T*)(globalBuffers.geometryBuffer.stagingBuffer.data))[vertexOffset];
		vertexNormals = &((NormalBuffer_T*)(globalBuffers.geometryBuffer.stagingBuffer.data))[vertexOffset];
		vertexUVs = &((UVBuffer_T*)(globalBuffers.geometryBuffer.stagingBuffer.data))[vertexOffset];
		vertexColors = &((ColorBuffer_T*)(globalBuffers.geometryBuffer.stagingBuffer.data))[vertexOffset];
	}
	
	void UnmapStagingBuffers() {
		indices = nullptr;
		vertexPositions = nullptr;
		vertexMaterials = nullptr;
		vertexNormals = nullptr;
		vertexUVs = nullptr;
		vertexColors = nullptr;
	}
	
	void SetVertex(ulong i, glm::vec3 pos, float info, MaterialBuffer_T material, glm::vec3 normal, glm::vec2 uv, glm::vec4 color) {
		if (vertexPositions && vertexMaterials && vertexNormals && vertexUVs && vertexColors) {
			glm::vec2 packedNormal = PackNormal(normal);
			glm::u32 packedUV = PackUV(uv);
			glm::u32 packedColor = PackColor(color);
			memcpy(&vertexPositions[i], &pos, sizeof(VertexBuffer_T));
			memcpy(&vertexMaterials[i], &material, sizeof(MaterialBuffer_T));
			memcpy(&vertexNormals[i], &packedNormal, sizeof(NormalBuffer_T));
			memcpy(&vertexUVs[i], &packedUV, sizeof(UVBuffer_T));
			memcpy(&vertexColors[i], &packedColor, sizeof(ColorBuffer_T));
		} else {
			LOG_ERROR("staging buffers buffer must be mapped before writing vertices")
		}
	}
	
	void SetIndex(ulong i, IndexBuffer_T vertexIndex) {
		if (indices) {
			memcpy(&indices[i], &vertexIndex, sizeof(IndexBuffer_T));
		} else {
			LOG_ERROR("staging buffers buffer must be mapped before writing indices")
		}
	}
	
	void SetIndices(std::vector<IndexBuffer_T>&& vertexIndices) {
		if (indices) {
			memcpy(indices, vertexIndices.data(), sizeof(IndexBuffer_T)*vertexIndices.size());
		} else {
			LOG_ERROR("staging buffers buffer must be mapped before writing indices")
		}
	}
	
	void SetIndices(IndexBuffer_T* vertexIndices) {
		if (indices) {
			memcpy(indices, vertexIndices, sizeof(IndexBuffer_T)*indexCount);
		} else {
			LOG_ERROR("staging buffers buffer must be mapped before writing indices")
		}
	}
	
	void GetVertex(ulong i, glm::vec3* pos, float* info, glm::u32* material, glm::vec3* normal, glm::vec2* uv, glm::vec4* color) {
		if (vertexPositions && vertexMaterials && vertexNormals && vertexUVs && vertexColors) {
			glm::vec2 packedNormal;
			glm::u32 packedUV;
			glm::u32 packedColor;
			memcpy(pos, &vertexPositions[i], sizeof(VertexBuffer_T));
			memcpy(material, &vertexMaterials[i], sizeof(MaterialBuffer_T));
			memcpy(&packedNormal, &vertexNormals[i], sizeof(NormalBuffer_T));
			memcpy(&packedUV, &vertexUVs[i], sizeof(UVBuffer_T));
			memcpy(&packedColor, &vertexColors[i], sizeof(ColorBuffer_T));
			*normal = UnpackNormal(packedNormal);
			*uv = UnpackUV(packedUV);
			*color = UnpackColor(packedColor);
		} else {
			LOG_ERROR("staging buffers must be mapped before reading vertices")
		}
	}
	
	void GetIndex(ulong i, IndexBuffer_T* vertexIndex) {
		if (indices) {
			memcpy(vertexIndex, indices+(i*sizeof(IndexBuffer_T)), sizeof(IndexBuffer_T));
		} else {
			LOG_ERROR("staging buffers must be mapped before reading indices")
		}
	}
	
	void GetIndices(std::vector<IndexBuffer_T>& vertexIndices) {
		if (indices) {
			if (vertexIndices.capacity() < indexCount) vertexIndices.resize(indexCount);
			memcpy(vertexIndices.data(), indices, sizeof(IndexBuffer_T)*indexCount);
		} else {
			LOG_ERROR("staging buffers buffer must be mapped before reading indices")
		}
	}
	
	void Push(Device* device, VkCommandBuffer commandBuffer) {
		globalBuffers.PushGeometry(device, commandBuffer, this);
	}

	void Pull(Device* device, VkCommandBuffer commandBuffer) {
		globalBuffers.PullGeometry(device, commandBuffer, this);
	}

};

Geometry::GlobalGeometryBuffers Geometry::globalBuffers {};
