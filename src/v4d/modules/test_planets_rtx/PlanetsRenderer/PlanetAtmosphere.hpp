#pragma once
#include <v4d.h>

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

#define PLANETARY_ATMOSPHERE_MESH_USE_TRIANGLE_STRIPS

struct PlanetAtmosphere {
	static const int vertexSubdivisions = 64;
	
	// Constructor arguments
	double radius = 0; // top of atmosphere (maximum radius)
	float densityFactor = 0.5f;
	glm::vec3 color = glm::vec3(0.85, 0.8, 1.0);

	#pragma region Calculated constants
	static const int nbVertices = (vertexSubdivisions+1) * (vertexSubdivisions+1) * 6;
	#ifdef PLANETARY_ATMOSPHERE_MESH_USE_TRIANGLE_STRIPS
		static const int nbIndices = (vertexSubdivisions*vertexSubdivisions*2 + vertexSubdivisions*3) * 6;
	#else
		static const int nbIndices = vertexSubdivisions*vertexSubdivisions*6 * 6;
	#endif
	#pragma endregion

	struct Vertex {
		glm::vec4 pos; // pos.a = altitude
		
		static std::vector<VertexInputAttributeDescription> GetInputAttributes() {
			return {
				{0, offsetof(Vertex, pos), VK_FORMAT_R32G32B32A32_SFLOAT},
			};
		}
	};

	StagedBuffer vertexBuffer {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT};
	StagedBuffer indexBuffer {VK_BUFFER_USAGE_INDEX_BUFFER_BIT};
	
	std::array<Vertex, nbVertices> vertices;
	std::array<uint32_t, nbIndices> indices;
	bool allocated = false;
	
	void Allocate(Device* device, VkCommandBuffer commandBuffer) {
		if (!allocated) {
			allocated = true;
			vertexBuffer.Allocate(device);
			indexBuffer.Allocate(device);
			vertexBuffer.Update(device, commandBuffer);
			indexBuffer.Update(device, commandBuffer);
		}
	}
	
	void Free(Device* device) {
		if (allocated) {
			vertexBuffer.Free(device);
			indexBuffer.Free(device);
			allocated = false;
		}
	}
	
	PlanetAtmosphere(double radius) : radius(radius) {
	
		int genVertexIndex = 0;
		int genIndexIndex = 0;
		
		for (int face = 0; face < 6; ++face) {
			auto [faceDir, topDir, rightDir] = CubeToSphere::GetFaceVectors(face);
			
			glm::dvec3 topLeft = faceDir + topDir - rightDir;
			glm::dvec3 topRight = faceDir + topDir + rightDir;
			glm::dvec3 bottomLeft = faceDir - topDir - rightDir;
			glm::dvec3 bottomRight = faceDir - topDir + rightDir;
			glm::dvec3 center = (topLeft + topRight + bottomLeft + bottomRight) / 4.0;
			
			double topSign = topDir.x + topDir.y + topDir.z;
			double rightSign = rightDir.x + rightDir.y + rightDir.z;
			
			for (int genRow = 0; genRow <= vertexSubdivisions; ++genRow) {
				for (int genCol = 0; genCol <= vertexSubdivisions; ++genCol) {
					uint32_t currentIndex = genVertexIndex++;
					
					glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow)/vertexSubdivisions);
					glm::dvec3 rightOffset = glm::mix(topLeft - center, topRight - center, double(genCol)/vertexSubdivisions);
					
					// glm::dvec3 pos = CubeToSphere::Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
					glm::dvec3 pos = glm::normalize(center + topDir*topOffset + rightDir*rightOffset);
					
					vertices[currentIndex].pos = glm::vec4(pos * radius, 0/* potential additional data */);
					// vertices[currentIndex].normal = glm::dvec4(pos, 0/* potential additional data */);
					// vertices[currentIndex].uv = glm::vec4(glm::vec2(genCol, genRow) / float(vertexSubdivisions), 0, 0);
					
					if (genRow < vertexSubdivisions) {
						uint32_t topLeftIndex = currentIndex;
						uint32_t topRightIndex = topLeftIndex+1;
						uint32_t bottomLeftIndex = currentIndex + vertexSubdivisions+1;
						uint32_t bottomRightIndex = bottomLeftIndex+1;
						
						#ifdef PLANETARY_ATMOSPHERE_MESH_USE_TRIANGLE_STRIPS
							if (topSign == rightSign) {
								if (genCol < vertexSubdivisions) {
									if (genCol == 0) {
										indices[genIndexIndex++] = topLeftIndex;
									}
									indices[genIndexIndex++] = bottomLeftIndex;
									indices[genIndexIndex++] = topRightIndex;
								} else {
									indices[genIndexIndex++] = bottomLeftIndex; // bottom right-most
									if (genRow < vertexSubdivisions) {
										indices[genIndexIndex++] = 0xFFFFFFFF; // restart primitive
									}
								}
							} else {
								if (genCol < vertexSubdivisions) {
									if (genCol == 0) {
										indices[genIndexIndex++] = bottomLeftIndex;
									}
									indices[genIndexIndex++] = topLeftIndex;
									indices[genIndexIndex++] = bottomRightIndex;
								} else {
									indices[genIndexIndex++] = topLeftIndex; // top right-most
									if (genRow < vertexSubdivisions) {
										indices[genIndexIndex++] = 0xFFFFFFFF; // restart primitive
									}
								}
							}
						#else
							if (genCol < vertexSubdivisions) {
								if (topSign == rightSign) {
									indices[genIndexIndex++] = topLeftIndex;
									indices[genIndexIndex++] = bottomLeftIndex;
									indices[genIndexIndex++] = bottomRightIndex;
									indices[genIndexIndex++] = topLeftIndex;
									indices[genIndexIndex++] = bottomRightIndex;
									indices[genIndexIndex++] = topRightIndex;
								} else {
									indices[genIndexIndex++] = topLeftIndex;
									indices[genIndexIndex++] = bottomRightIndex;
									indices[genIndexIndex++] = bottomLeftIndex;
									indices[genIndexIndex++] = topLeftIndex;
									indices[genIndexIndex++] = topRightIndex;
									indices[genIndexIndex++] = bottomRightIndex;
								}
							}
						#endif
					}
				}
			}
		}
	
		// Check for errors
		if (genVertexIndex != nbVertices) {
			INVALIDCODE("Problem with atmosphere mesh generation, generated vertices do not match array size " << genVertexIndex << " != " << nbVertices)
			return;
		}
		if (genIndexIndex != nbIndices) {
			INVALIDCODE("Problem with atmosphere mesh generation, generated indices do not match array size " << genIndexIndex << " != " << nbIndices)
			return;
		}
		
		vertexBuffer.AddSrcDataPtr<Vertex, nbVertices>(&vertices);
		indexBuffer.AddSrcDataPtr<uint32_t, nbIndices>(&indices);
	}
	
};
