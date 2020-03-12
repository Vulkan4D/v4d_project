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






typedef glm::uvec4	GeometryBuffer_T;
typedef glm::u32	IndexBuffer_T;
typedef glm::vec4	PosBuffer_T;
typedef glm::u32	MaterialBuffer_T;
typedef glm::vec2	NormalBuffer_T;
typedef glm::u32	UVBuffer_T;
typedef glm::u32	ColorBuffer_T;


NormalBuffer_T PackNormal(glm::vec3 normal) {
	float f = glm::sqrt(8.0f * normal.z + 8.0f);
	return glm::vec2(normal) / f + 0.5f;
}

ColorBuffer_T PackColor(glm::vec4 color) {
	color *= 255.0f;
	glm::uvec4 pack {
		glm::clamp(glm::u32(color.r), (glm::u32)0, (glm::u32)255),
		glm::clamp(glm::u32(color.g), (glm::u32)0, (glm::u32)255),
		glm::clamp(glm::u32(color.b), (glm::u32)0, (glm::u32)255),
		glm::clamp(glm::u32(color.a), (glm::u32)0, (glm::u32)255),
	};
	return (pack.r << 24) | (pack.g << 16) | (pack.b << 8) | pack.a;
}

UVBuffer_T PackUV(glm::vec2 uv) {
	uv *= 65535.0f;
	glm::uvec2 pack {
		glm::clamp(glm::u32(uv.s), (glm::u32)0, (glm::u32)65535),
		glm::clamp(glm::u32(uv.t), (glm::u32)0, (glm::u32)65535),
	};
	return (pack.s << 16) | pack.t;
}

glm::vec3 UnpackNormal(NormalBuffer_T norm) {
	glm::vec2 fenc = norm * 4.0f - 2.0f;
	float f = glm::dot(fenc, fenc);
	float g = glm::sqrt(1.0f - f / 4.0f);
	return glm::vec3(fenc * g, 1.0f - f / 2.0f);
}

glm::vec2 UnpackUV(UVBuffer_T uv) {
	return glm::vec2(
		(uv & 0xffff0000) >> 16,
		(uv & 0x0000ffff) >> 0
	) / 65535.0f;
}

glm::vec4 UnpackColor(ColorBuffer_T color) {
	return glm::vec4(
		(color & 0xff000000) >> 24,
		(color & 0x00ff0000) >> 16,
		(color & 0x0000ff00) >> 8,
		(color & 0x000000ff) >> 0
	) / 255.0f;
}

template<class T, VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT>
struct GlobalBuffer : public StagedBuffer {
	GlobalBuffer(uint32_t initialBlocks) : StagedBuffer(usage, sizeof(T) * initialBlocks, false) {}
	
	void Push(Device* device, VkCommandBuffer commandBuffer, int count, int offset = 0) {
		Buffer::Copy(device, commandBuffer, stagingBuffer, deviceLocalBuffer, sizeof(T) * count, sizeof(T) * offset, sizeof(T) * offset);
	}
	
	void Pull(Device* device, VkCommandBuffer commandBuffer, int count, int offset = 0) {
		Buffer::Copy(device, commandBuffer, deviceLocalBuffer, stagingBuffer, sizeof(T) * count, sizeof(T) * offset, sizeof(T) * offset);
	}
};

typedef GlobalBuffer<GeometryBuffer_T> GeometryBuffer;
typedef GlobalBuffer<IndexBuffer_T, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT> IndexBuffer;
typedef GlobalBuffer<PosBuffer_T, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT> PosBuffer;
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
	
	uint32_t geometryOffset = 0;
	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;

	GeometryBuffer_T* geometryInfo = nullptr;
	IndexBuffer_T* indices = nullptr;
	PosBuffer_T* vertexPositions = nullptr;
	MaterialBuffer_T* vertexMaterials = nullptr;
	NormalBuffer_T* vertexNormals = nullptr;
	UVBuffer_T* vertexUVs = nullptr;
	ColorBuffer_T* vertexColors = nullptr;
	
	Buffer* transformBuffer = nullptr;
	VkDeviceSize transformOffset = 0;
	
	bool geometryInfoInitialized = false;
	
	class GlobalGeometryBuffers {
		std::mutex geometryBufferMutex, vertexBufferMutex, indexBufferMutex;
		std::map<int, Geometry*> geometryAllocations {};
		std::map<int, BufferAllocation> indexAllocations {};
		std::map<int, BufferAllocation> vertexAllocations {};
		
		static const int nbInitialGeometries = 1024;
		static const int nbInitialVertices = 1000000;
		static const int nbInitialIndices = nbInitialVertices * 2;
		
	public:
		
		enum GeometryBuffersMask : uint8_t {
			BUFFER_GEOMETRY_INFO = 0x01,
			BUFFER_INDEX = 0x02,
			BUFFER_POS = 0x04,
			BUFFER_MAT = 0x08,
			BUFFER_NORM = 0x10,
			BUFFER_UV = 0x20,
			BUFFER_COLOR = 0x40,
			// BUFFER_*** = 0x80,
			BUFFER_ALL = 0xff,
			BUFFER_VERTEX_DATA = BUFFER_POS | BUFFER_MAT | BUFFER_NORM | BUFFER_UV | BUFFER_COLOR,
		};
		
		GeometryBuffer geometryBuffer {nbInitialGeometries};
		IndexBuffer indexBuffer {nbInitialIndices};
		PosBuffer posBuffer {nbInitialVertices};
		MaterialBuffer materialBuffer {nbInitialVertices};
		NormalBuffer normalBuffer {nbInitialVertices};
		UVBuffer uvBuffer {nbInitialVertices};
		ColorBuffer colorBuffer {nbInitialVertices};
		
		uint32_t nbAllocatedGeometries = 0;
		uint32_t nbAllocatedIndices = 0;
		uint32_t nbAllocatedVertices = 0;
		
		int/*geometryOffset*/ AddGeometry(Geometry* geometry) {
			std::scoped_lock lock(geometryBufferMutex, vertexBufferMutex, indexBufferMutex);
			
			// Geometry
			geometry->geometryOffset = nbAllocatedGeometries;
			for (auto [i, g] : geometryAllocations) {
				if (!g) {
					geometry->geometryOffset = i;
					break;
				}
			}
			nbAllocatedGeometries = std::max(nbAllocatedGeometries, geometry->geometryOffset);
			geometryAllocations[geometry->geometryOffset] = geometry;
			
			// Index
			geometry->indexOffset = nbAllocatedIndices;
			for (auto [i, alloc] : indexAllocations) {
				if (!alloc.data) {
					if (alloc.n >= geometry->indexCount) {
						geometry->indexOffset = i;
						if (alloc.n > geometry->indexCount) {
							indexAllocations[geometry->indexOffset+geometry->indexCount] = {alloc.n - geometry->indexCount, nullptr};
						}
					}
				}
			}
			nbAllocatedIndices = std::max(nbAllocatedIndices, geometry->indexOffset + geometry->indexCount);
			indexAllocations[geometry->indexOffset] = {geometry->indexCount, geometry};
			
			// Vertex
			geometry->vertexOffset = nbAllocatedVertices;
			for (auto [i, alloc] : vertexAllocations) {
				if (!alloc.data) {
					if (alloc.n >= geometry->vertexCount) {
						geometry->vertexOffset = i;
						if (alloc.n > geometry->vertexCount) {
							vertexAllocations[geometry->vertexOffset+geometry->vertexCount] = {alloc.n - geometry->vertexCount, nullptr};
						}
					}
				}
			}
			nbAllocatedVertices = std::max(nbAllocatedVertices, geometry->vertexOffset + geometry->vertexCount);
			vertexAllocations[geometry->vertexOffset] = {geometry->vertexCount, geometry};
			
			// Check for overflow
			if (geometryBuffer.stagingBuffer.size < nbAllocatedGeometries * sizeof(GeometryBuffer_T)) 
				FATAL("Global Geometry buffer overflow" )
			if (indexBuffer.stagingBuffer.size < nbAllocatedIndices * sizeof(IndexBuffer_T)) 
				FATAL("Global Index buffer overflow" )
			if (posBuffer.stagingBuffer.size < nbAllocatedVertices * sizeof(PosBuffer_T)) 
				FATAL("Global Pos buffer overflow" )
			if (materialBuffer.stagingBuffer.size < nbAllocatedVertices * sizeof(MaterialBuffer_T)) 
				FATAL("Global Material buffer overflow" )
			if (normalBuffer.stagingBuffer.size < nbAllocatedVertices * sizeof(NormalBuffer_T)) 
				FATAL("Global Normal buffer overflow" )
			if (uvBuffer.stagingBuffer.size < nbAllocatedVertices * sizeof(UVBuffer_T)) 
				FATAL("Global UV buffer overflow" )
			if (colorBuffer.stagingBuffer.size < nbAllocatedVertices * sizeof(ColorBuffer_T)) 
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
			posBuffer.Allocate(device);
			materialBuffer.Allocate(device);
			normalBuffer.Allocate(device);
			uvBuffer.Allocate(device);
			colorBuffer.Allocate(device);
		}
		
		void Free(Device* device) {
			geometryBuffer.Free(device);
			indexBuffer.Free(device);
			posBuffer.Free(device);
			materialBuffer.Free(device);
			normalBuffer.Free(device);
			uvBuffer.Free(device);
			colorBuffer.Free(device);
		}
		
		void PushAllGeometries(Device* device, VkCommandBuffer commandBuffer) {
			if (nbAllocatedGeometries == 0) return;
			geometryBuffer.Push(device, commandBuffer, nbAllocatedGeometries);
			indexBuffer.Push(device, commandBuffer, nbAllocatedIndices);
			posBuffer.Push(device, commandBuffer, nbAllocatedVertices);
			materialBuffer.Push(device, commandBuffer, nbAllocatedVertices);
			normalBuffer.Push(device, commandBuffer, nbAllocatedVertices);
			uvBuffer.Push(device, commandBuffer, nbAllocatedVertices);
			colorBuffer.Push(device, commandBuffer, nbAllocatedVertices);
		}
		
		void PullAllGeometries(Device* device, VkCommandBuffer commandBuffer) {
			if (nbAllocatedGeometries == 0) return;
			geometryBuffer.Pull(device, commandBuffer, nbAllocatedGeometries);
			indexBuffer.Pull(device, commandBuffer, nbAllocatedIndices);
			posBuffer.Pull(device, commandBuffer, nbAllocatedVertices);
			materialBuffer.Pull(device, commandBuffer, nbAllocatedVertices);
			normalBuffer.Pull(device, commandBuffer, nbAllocatedVertices);
			uvBuffer.Pull(device, commandBuffer, nbAllocatedVertices);
			colorBuffer.Pull(device, commandBuffer, nbAllocatedVertices);
		}
		
		void PushGeometry(Device* device, VkCommandBuffer commandBuffer, Geometry* geometry, 
							GeometryBuffersMask geometryBuffersMask = BUFFER_ALL, 
							uint32_t vertexCount = 0, uint32_t vertexOffset = 0,
							uint32_t indexCount = 0, uint32_t indexOffset = 0
		) {
			if (vertexCount == 0) vertexCount = geometry->vertexCount;
			else vertexCount = std::min(vertexCount, geometry->vertexCount);
			
			if (indexCount == 0) indexCount = geometry->indexCount;
			else indexCount = std::min(indexCount, geometry->indexCount);
			
			if (geometryBuffersMask & BUFFER_GEOMETRY_INFO) geometryBuffer.Push(device, commandBuffer, 1, geometry->geometryOffset);
			if (geometryBuffersMask & BUFFER_INDEX) indexBuffer.Push(device, commandBuffer, indexCount, geometry->indexOffset + indexOffset);
			if (geometryBuffersMask & BUFFER_POS) posBuffer.Push(device, commandBuffer, vertexCount, geometry->vertexOffset + vertexOffset);
			if (geometryBuffersMask & BUFFER_MAT) materialBuffer.Push(device, commandBuffer, vertexCount, geometry->vertexOffset + vertexOffset);
			if (geometryBuffersMask & BUFFER_NORM) normalBuffer.Push(device, commandBuffer, vertexCount, geometry->vertexOffset + vertexOffset);
			if (geometryBuffersMask & BUFFER_UV) uvBuffer.Push(device, commandBuffer, vertexCount, geometry->vertexOffset + vertexOffset);
			if (geometryBuffersMask & BUFFER_COLOR) colorBuffer.Push(device, commandBuffer, vertexCount, geometry->vertexOffset + vertexOffset);
		}
		
		void PullGeometry(Device* device, VkCommandBuffer commandBuffer, Geometry* geometry, 
							GeometryBuffersMask geometryBuffersMask = BUFFER_ALL, 
							uint32_t vertexCount = 0, uint32_t vertexOffset = 0,
							uint32_t indexCount = 0, uint32_t indexOffset = 0
		) {
			if (vertexCount == 0) vertexCount = geometry->vertexCount;
			else vertexCount = std::min(vertexCount, geometry->vertexCount);
			
			if (indexCount == 0) indexCount = geometry->indexCount;
			else indexCount = std::min(indexCount, geometry->indexCount);
			
			if (geometryBuffersMask & BUFFER_GEOMETRY_INFO) geometryBuffer.Pull(device, commandBuffer, 1, geometry->geometryOffset);
			if (geometryBuffersMask & BUFFER_INDEX) indexBuffer.Pull(device, commandBuffer, indexCount, geometry->indexOffset + indexOffset);
			if (geometryBuffersMask & BUFFER_POS) posBuffer.Pull(device, commandBuffer, vertexCount, geometry->vertexOffset + vertexOffset);
			if (geometryBuffersMask & BUFFER_MAT) materialBuffer.Pull(device, commandBuffer, vertexCount, geometry->vertexOffset + vertexOffset);
			if (geometryBuffersMask & BUFFER_NORM) normalBuffer.Pull(device, commandBuffer, vertexCount, geometry->vertexOffset + vertexOffset);
			if (geometryBuffersMask & BUFFER_UV) uvBuffer.Pull(device, commandBuffer, vertexCount, geometry->vertexOffset + vertexOffset);
			if (geometryBuffersMask & BUFFER_COLOR) colorBuffer.Pull(device, commandBuffer, vertexCount, geometry->vertexOffset + vertexOffset);
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
		
		triangleGeometry.geometry.triangles.vertexOffset = (VkDeviceSize)(vertexOffset*sizeof(PosBuffer_T));
		triangleGeometry.geometry.triangles.vertexCount = vertexCount;
		triangleGeometry.geometry.triangles.vertexStride = sizeof(PosBuffer_T);
		triangleGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		triangleGeometry.geometry.triangles.indexOffset = (VkDeviceSize)(indexOffset*sizeof(IndexBuffer_T));
		triangleGeometry.geometry.triangles.indexCount = indexCount;
		triangleGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
		
		triangleGeometry.geometry.triangles.vertexData = globalBuffers.posBuffer.deviceLocalBuffer.buffer;
		triangleGeometry.geometry.triangles.indexData = globalBuffers.indexBuffer.deviceLocalBuffer.buffer;
		
		triangleGeometry.geometry.triangles.transformData = transformBuffer? transformBuffer->buffer : VK_NULL_HANDLE;
		triangleGeometry.geometry.triangles.transformOffset = transformOffset;
		
		return triangleGeometry;
	}
	
	Geometry(uint32_t vertexCount, uint32_t indexCount) : vertexCount(vertexCount), indexCount(indexCount) {
		globalBuffers.AddGeometry(this);
	}
	
	~Geometry() {
		globalBuffers.RemoveGeometry(this);
	}
	
	void MapStagingBuffers() {
		if (!globalBuffers.geometryBuffer.stagingBuffer.data) {
			FATAL("global buffers must be allocated before mapping staging buffers")
		}
		geometryInfo =		&((GeometryBuffer_T*)	(globalBuffers.geometryBuffer.stagingBuffer.data))	[geometryOffset];
		indices =			&((IndexBuffer_T*)		(globalBuffers.indexBuffer.stagingBuffer.data))		[indexOffset];
		vertexPositions =	&((PosBuffer_T*)		(globalBuffers.posBuffer.stagingBuffer.data))		[vertexOffset];
		vertexMaterials =	&((MaterialBuffer_T*)	(globalBuffers.materialBuffer.stagingBuffer.data))	[vertexOffset];
		vertexNormals =		&((NormalBuffer_T*)		(globalBuffers.normalBuffer.stagingBuffer.data))	[vertexOffset];
		vertexUVs =			&((UVBuffer_T*)			(globalBuffers.uvBuffer.stagingBuffer.data))		[vertexOffset];
		vertexColors =		&((ColorBuffer_T*)		(globalBuffers.colorBuffer.stagingBuffer.data))		[vertexOffset];
		
		// LOG("Global Geometry Staging Buffers mapped")
	}
	
	void UnmapStagingBuffers() {
		geometryInfo = nullptr;
		indices = nullptr;
		vertexPositions = nullptr;
		vertexMaterials = nullptr;
		vertexNormals = nullptr;
		vertexUVs = nullptr;
		vertexColors = nullptr;
		
		geometryInfoInitialized = false;
	}
	
	void SetGeometryInfo(uint32_t objectIndex = 0, uint32_t otherIndex = 0) {
		if (!geometryInfo) MapStagingBuffers();
		
		GeometryBuffer_T data {
			indexOffset,
			vertexOffset,
			objectIndex,
			otherIndex
		};
		
		memcpy(geometryInfo, &data, sizeof(GeometryBuffer_T));
		
		geometryInfoInitialized = true;
	}
	
	void SetVertex(uint32_t i, const glm::vec4& pos, MaterialBuffer_T material, const glm::vec3& normal, const glm::vec2& uv, const glm::vec4& color) {
		if (!geometryInfoInitialized) SetGeometryInfo();
		
		NormalBuffer_T packedNormal = PackNormal(normal);
		UVBuffer_T packedUV = PackUV(uv);
		ColorBuffer_T packedColor = PackColor(color);
		memcpy(&vertexPositions[i], &pos, sizeof(PosBuffer_T));
		memcpy(&vertexMaterials[i], &material, sizeof(MaterialBuffer_T));
		memcpy(&vertexNormals[i], &packedNormal, sizeof(NormalBuffer_T));
		memcpy(&vertexUVs[i], &packedUV, sizeof(UVBuffer_T));
		memcpy(&vertexColors[i], &packedColor, sizeof(ColorBuffer_T));
	}
	
	void SetIndex(uint32_t i, IndexBuffer_T vertexIndex) {
		if (!geometryInfoInitialized) SetGeometryInfo();
		
		memcpy(&indices[i], &vertexIndex, sizeof(IndexBuffer_T));
	}
	
	void SetIndices(const std::vector<IndexBuffer_T>& vertexIndices, uint32_t count = 0, uint32_t startAt = 0) {
		if (!geometryInfoInitialized) SetGeometryInfo();
		
		if (vertexIndices.size() == 0) return;
		
		if (count == 0) count = std::min(indexCount, (uint32_t)vertexIndices.size());
		else count = std::min(count, (uint32_t)vertexIndices.size());
		
		memcpy(&indices[startAt], vertexIndices.data(), sizeof(IndexBuffer_T)*count);
	}
	
	//TODO void SetTriangle(uint32_t i, IndexBuffer_T v0, IndexBuffer_T v1, IndexBuffer_T v2) {}
	//TODO void GetTriangle(uint32_t i, IndexBuffer_T* v0, IndexBuffer_T* v1, IndexBuffer_T* v2) {}
	
	void SetIndices(const IndexBuffer_T* vertexIndices, uint32_t count = 0, uint32_t startAt = 0) {
		if (!geometryInfoInitialized) SetGeometryInfo();
		
		if (count == 0) count = indexCount;
		else count = std::min(count, indexCount);
	
		memcpy(&indices[startAt], vertexIndices, sizeof(IndexBuffer_T)*count);
	}
	
	void GetGeometryInfo(uint32_t* objectIndex, uint32_t* otherIndex) {
		if (!geometryInfo) MapStagingBuffers();
		
		GeometryBuffer_T data;
		memcpy(&data, &geometryInfo, sizeof(GeometryBuffer_T));
		indexOffset = data.x;
		vertexOffset = data.y;
		*objectIndex = data.z;
		*otherIndex = data.w;
	}
	
	void GetVertex(uint32_t i, glm::vec4* pos, glm::u32* material, glm::vec3* normal, glm::vec2* uv, glm::vec4* color) {
		if (!geometryInfo) MapStagingBuffers();
		
		NormalBuffer_T packedNormal;
		UVBuffer_T packedUV;
		ColorBuffer_T packedColor;
		memcpy(pos, &vertexPositions[i], sizeof(PosBuffer_T));
		memcpy(material, &vertexMaterials[i], sizeof(MaterialBuffer_T));
		memcpy(&packedNormal, &vertexNormals[i], sizeof(NormalBuffer_T));
		memcpy(&packedUV, &vertexUVs[i], sizeof(UVBuffer_T));
		memcpy(&packedColor, &vertexColors[i], sizeof(ColorBuffer_T));
		*normal = UnpackNormal(packedNormal);
		*uv = UnpackUV(packedUV);
		*color = UnpackColor(packedColor);
	}
	
	void GetIndex(uint32_t i, IndexBuffer_T* vertexIndex) {
		if (!geometryInfo) MapStagingBuffers();
		
		memcpy(vertexIndex, &indices[i], sizeof(IndexBuffer_T));
	}
	
	void GetIndices(std::vector<IndexBuffer_T>* vertexIndices, uint32_t count = 0, uint32_t startAt = 0) {
		if (!geometryInfo) MapStagingBuffers();
	
		if (vertexIndices) {
			if (count == 0) count = indexCount;
			if (vertexIndices->capacity() < count) vertexIndices->resize(count);
			memcpy(vertexIndices->data(), &indices[startAt], sizeof(IndexBuffer_T)*count);
		} else {
			LOG_ERROR("vertexIndices vector pointer arg must be allocated")
		}
	}
	
	void Push(Device* device, VkCommandBuffer commandBuffer, 
			GlobalGeometryBuffers::GeometryBuffersMask geometryBuffersMask = GlobalGeometryBuffers::BUFFER_ALL, 
			uint32_t vertexCount = 0, uint32_t vertexOffset = 0,
			uint32_t indexCount = 0, uint32_t indexOffset = 0
		) {
		globalBuffers.PushGeometry(device, commandBuffer, this, geometryBuffersMask, vertexCount, vertexOffset, indexCount, indexOffset);
	}

	void Pull(Device* device, VkCommandBuffer commandBuffer, 
			GlobalGeometryBuffers::GeometryBuffersMask geometryBuffersMask = GlobalGeometryBuffers::BUFFER_ALL, 
			uint32_t vertexCount = 0, uint32_t vertexOffset = 0,
			uint32_t indexCount = 0, uint32_t indexOffset = 0
		) {
		globalBuffers.PullGeometry(device, commandBuffer, this, geometryBuffersMask, vertexCount, vertexOffset, indexCount, indexOffset);
	}

};

Geometry::GlobalGeometryBuffers Geometry::globalBuffers {};
