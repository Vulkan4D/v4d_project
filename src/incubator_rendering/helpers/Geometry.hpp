#pragma once

class Geometry {
public:
	VkGeometryFlagBitsNV rayTracingGeometryFlags = VK_GEOMETRY_OPAQUE_BIT_NV;

	virtual void* GetData() = 0;
	virtual void* GetIndexData() = 0;
	virtual VkGeometryNV GetRayTracingGeometry() const = 0;
	
	virtual ~Geometry(){}
};

template<class V, class I = uint, VkFormat f = VK_FORMAT_R32G32B32_SFLOAT>
class TriangleGeometry : public Geometry {
public:
	
	std::vector<V, std::allocator<V>> vertexData {};
	std::vector<I, std::allocator<I>> indexData {};
	
	Buffer* vertexBuffer = nullptr;
	VkDeviceSize vertexOffset = 0;
	VkDeviceSize vertexCount = 0;
	
	Buffer* indexBuffer = nullptr;
	VkDeviceSize indexOffset = 0;
	VkDeviceSize indexCount = 0;

	Buffer* transformBuffer = nullptr;
	VkDeviceSize transformOffset = 0;
	
	void* GetData() override {
		return vertexData.data();
	}
	
	void* GetIndexData() override {
		return indexData.data();
	}
	
	VkGeometryNV GetRayTracingGeometry() const override {
		if (vertexBuffer->buffer == VK_NULL_HANDLE)
			throw std::runtime_error("Vertex Buffer for aabb geometry has not been created");
		if (indexBuffer->buffer == VK_NULL_HANDLE)
			throw std::runtime_error("Index Buffer for aabb geometry has not been created");
		
		VkGeometryNV triangleGeometry {};
		triangleGeometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
		triangleGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
		triangleGeometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
		triangleGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
		triangleGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_NV;
		
		triangleGeometry.geometry.triangles.vertexOffset = vertexOffset;
		triangleGeometry.geometry.triangles.vertexCount = (uint)vertexData.size();
		triangleGeometry.geometry.triangles.vertexStride = sizeof(V);
		triangleGeometry.geometry.triangles.vertexFormat = f;
		triangleGeometry.geometry.triangles.indexOffset = indexOffset;
		triangleGeometry.geometry.triangles.indexCount = (uint)indexData.size();
		triangleGeometry.geometry.triangles.indexType = sizeof(I) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
		
		triangleGeometry.geometry.triangles.vertexData = vertexBuffer->buffer;
		triangleGeometry.geometry.triangles.indexData = indexBuffer->buffer;
		
		triangleGeometry.geometry.triangles.transformData = transformBuffer? transformBuffer->buffer : VK_NULL_HANDLE;
		triangleGeometry.geometry.triangles.transformOffset = transformOffset;
		
		return triangleGeometry;
	}
	
	TriangleGeometry(){}
	TriangleGeometry(std::vector<V>&& vertexData, std::vector<I>&& indexData, Buffer* vertexBuffer, VkDeviceSize vertexOffset, Buffer* indexBuffer, VkDeviceSize indexOffset, Buffer* transformBuffer = nullptr, VkDeviceSize transformOffset = 0)
	 : vertexData(std::forward<std::vector<V>>(vertexData)), indexData(std::forward<std::vector<I>>(indexData)) {
		this->vertexBuffer = vertexBuffer;
		this->indexBuffer = indexBuffer;
		this->transformBuffer = transformBuffer;
		this->vertexOffset = vertexOffset;
		this->indexOffset = indexOffset;
		this->transformOffset = transformOffset;
		this->vertexCount = vertexData.size();
		this->indexCount = indexData.size();
	}
};

struct ProceduralGeometryData {
	std::pair<glm::vec3, glm::vec3> boundingBox;
	ProceduralGeometryData(glm::vec3 min, glm::vec3 max) : boundingBox(min, max) {}
};

template<class V>
class ProceduralGeometry : public Geometry {
public:
	std::vector<V, std::allocator<V>> aabbData {};
	
	Buffer* aabbBuffer = nullptr;
	VkDeviceSize aabbOffset = 0;
	VkDeviceSize aabbCount = 0;
	
	void* GetData() override {
		return aabbData.data();
	}
	
	void* GetIndexData() override {
		return nullptr;
	}
	
	VkGeometryNV GetRayTracingGeometry() const override {
		if (aabbBuffer->buffer == VK_NULL_HANDLE)
			throw std::runtime_error("Buffer for aabb geometry has not been created");
		
		VkGeometryNV geometry {};
		geometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
		geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
		geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
		geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_NV;
		geometry.flags = VK_GEOMETRY_OPAQUE_BIT_NV;
		
		geometry.geometry.aabbs.numAABBs = (uint)aabbData.size();
		geometry.geometry.aabbs.stride = sizeof(V);
		geometry.geometry.aabbs.offset = aabbOffset;
		
		geometry.geometry.aabbs.aabbData = aabbBuffer->buffer;
		
		return geometry;
	}
	
	ProceduralGeometry(){}
	ProceduralGeometry(std::vector<V>&& aabbData, Buffer* aabbBuffer, VkDeviceSize aabbOffset = 0)
	 : aabbData(std::forward<std::vector<V>>(aabbData)) {
		this->aabbBuffer = aabbBuffer;
		this->aabbOffset = aabbOffset;
		this->aabbCount = aabbData.size();
	}
	ProceduralGeometry(std::vector<V>& aabbData, Buffer* aabbBuffer, VkDeviceSize aabbOffset = 0)
	 : aabbData(std::forward<std::vector<V>>(aabbData)) {
		this->aabbBuffer = aabbBuffer;
		this->aabbOffset = aabbOffset;
		this->aabbCount = aabbData.size();
	}
};
