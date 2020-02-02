#pragma once
#include <v4d.h>

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

#define PLANETARY_ATMOSPHERE_MESH_USE_TRIANGLE_STRIPS

struct PlanetAtmosphere {
	static const int vertexSubdivisions = 128;
	
	// Constructor arguments
	double radius; // top of atmosphere (maximum radius)
	double solidRadius; // standard radius of solid surface (AKA sea level, surface height can go below or above)

	#pragma region Calculated constants
	static const int nbVertices = (vertexSubdivisions+1) * (vertexSubdivisions+1) * 6;
	#ifdef PLANETARY_ATMOSPHERE_MESH_USE_TRIANGLE_STRIPS
		static const int nbIndices = (vertexSubdivisions*vertexSubdivisions*2 + vertexSubdivisions*3 - 1) * 6;
	#else
		static const int nbIndices = vertexSubdivisions*vertexSubdivisions*6 * 6;
	#endif
	#pragma endregion

	struct Vertex {
		glm::vec4 pos; // pos.a = altitude
		glm::vec4 normal; // normal.w = slope
		
		static std::vector<VertexInputAttributeDescription> GetInputAttributes() {
			return {
				{0, offsetof(Vertex, pos), VK_FORMAT_R32G32B32A32_SFLOAT},
				{1, offsetof(Vertex, normal), VK_FORMAT_R32G32B32A32_SFLOAT},
			};
		}
	};

	StagedBuffer vertexBuffer {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT};
	StagedBuffer indexBuffer {VK_BUFFER_USAGE_INDEX_BUFFER_BIT};
	
	std::array<Vertex, nbVertices> vertices;
	std::array<uint32_t, nbIndices> indices;
	
	PlanetAtmosphere(double radius, double solidRadius) : radius(radius), solidRadius(solidRadius) {
		vertexBuffer.AddSrcDataPtr<Vertex, nbVertices>(&vertices);
		indexBuffer.AddSrcDataPtr<uint32_t, nbIndices>(&indices);
	}
	
};
