#pragma once

#include <v4d.h>
#include "../incubator_rendering/V4DRenderingPipeline.hh"
#include "../incubator_rendering/Camera.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

class Planet {
	
	PipelineLayout planetPipelineLayout;
	RasterShaderPipeline planetShader {planetPipelineLayout, {
		"incubator_galaxy4d/assets/shaders/planetRaster.vert",
		"incubator_galaxy4d/assets/shaders/planetRaster.geom",
		"incubator_galaxy4d/assets/shaders/planetRaster.frag",
	}};
	
	
	// Static graphics configuration
	static const int chunkSubdivisionsPerFace = 8;
	static const int vertexSubdivisionsPerChunk = 32;
	// static const int chunkLodSizeInPixels = 100;
	// static constexpr float minVertexSeparationInMeters = 0.5f;
	
	// calculated constants
	static const int nbChunksPerFace = chunkSubdivisionsPerFace * chunkSubdivisionsPerFace;
	static const int nbBaseChunksPerPlanet = nbChunksPerFace * 6;
	static const int nbVerticesPerChunk = (vertexSubdivisionsPerChunk+1) * (vertexSubdivisionsPerChunk+1);
	static const int nbIndicesPerChunk = (vertexSubdivisionsPerChunk) * (vertexSubdivisionsPerChunk) * 6;
	
	
	struct Vertex {
		glm::vec4 pos;
		glm::vec4 normal;
	};

	enum FACE : int {
		FRONT,
		BACK,
		RIGHT,
		LEFT,
		TOP,
		BOTTOM
	};
	
	struct PlanetInfo {
		double radius; // top of atmosphere (maximum radius)
		double solidRadius; // standard radius of solid surface (AKA sea level, surface height can go below or above)
		double heightVariation; // half the total variation (surface height is +- heightVariation)
		glm::dvec3 absolutePosition; // position of planet relative to world (star system center)
		
		struct Chunk {
			PlanetInfo* planet;
			FACE face;
			
			// Cube positions (-1.0 to +1.0)
			glm::dvec3 topLeft;
			glm::dvec3 topRight;
			glm::dvec3 bottomLeft;
			glm::dvec3 bottomRight;
			glm::dvec3 center {0};
			// non-normalized (true rounded central position of chunk on planet)
			glm::dvec3 posOnPlanet {0};
			
			std::array<Vertex, nbVerticesPerChunk> vertices;
			std::array<uint32_t, nbIndicesPerChunk> indices;
			std::vector<Chunk> subChunks {};
			bool generated = false;
			bool visible = true;
			Buffer vertexBuffer { VK_BUFFER_USAGE_VERTEX_BUFFER_BIT };
			Buffer indexBuffer { VK_BUFFER_USAGE_INDEX_BUFFER_BIT };
				
			struct PlanetChunkPushConstant { // max 128 bytes
				glm::dvec3 absolutePosition; // 24
				double radius; // 8
			} planetChunkPushConstant {};
			
			Chunk(PlanetInfo* planet, int face, glm::dvec3 topLeft, glm::dvec3 topRight, glm::dvec3 bottomLeft, glm::dvec3 bottomRight)
			: planet(planet), face((FACE)face), topLeft(topLeft), topRight(topRight), bottomLeft(bottomLeft), bottomRight(bottomRight) {
				center = (topLeft + topRight + bottomLeft + bottomRight) / 4.0;
				posOnPlanet = glm::round(glm::normalize(center) * planet->solidRadius);
			}
			
			void Create(Renderer* renderer, Device* renderingDevice) {
				auto [dir, top, right] = GetFaceVectors(face);
				
				int indexIndex = 0;
				for (int row = 0; row <= vertexSubdivisionsPerChunk; ++row) {
					for (int col = 0; col <= vertexSubdivisionsPerChunk; ++col) {
						uint32_t topLeftIndex = (vertexSubdivisionsPerChunk+1) * row + col;
						
						glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(row)/vertexSubdivisionsPerChunk);
						glm::dvec3 leftOffset =	glm::mix(topLeft - center, topRight - center, double(col)/vertexSubdivisionsPerChunk);
						
						glm::dvec3 pos = glm::normalize(center + top*topOffset + right*leftOffset);
						
						vertices[topLeftIndex].pos = glm::vec4(pos * planet->GetHeightMap(pos) - posOnPlanet, 0);
						vertices[topLeftIndex].normal = glm::vec4(glm::normalize(pos), 0);
						
						if (row < vertexSubdivisionsPerChunk && col < vertexSubdivisionsPerChunk) {
							uint32_t topRightIndex = topLeftIndex+1;
							uint32_t bottomLeftIndex = (vertexSubdivisionsPerChunk+1) * (row+1) + col;
							uint32_t bottomRightIndex = bottomLeftIndex+1;
							indices[indexIndex++] = topLeftIndex;
							indices[indexIndex++] = bottomLeftIndex;
							indices[indexIndex++] = bottomRightIndex;
							indices[indexIndex++] = topLeftIndex;
							indices[indexIndex++] = bottomRightIndex;
							indices[indexIndex++] = topRightIndex;
						}
					}
				}
				
				vertexBuffer.AddSrcDataPtr<Vertex, nbVerticesPerChunk>(&vertices);
				indexBuffer.AddSrcDataPtr<uint32_t, nbIndicesPerChunk>(&indices);
				
				renderer->AllocateBufferStaged(vertexBuffer);
				renderer->AllocateBufferStaged(indexBuffer);
				
				generated = true;
			}
			
			void Destroy(Device* renderingDevice) {
				if (generated) {
					generated = false;
					
					vertexBuffer.ResetSrcData();
					indexBuffer.ResetSrcData();
					
					// for (auto& subChunk : subChunks) {
					// 	subChunk.Free(renderingDevice);
					// }
					vertexBuffer.Free(renderingDevice);
					indexBuffer.Free(renderingDevice);
				}
			}
			
			void Render(Device* renderingDevice, VkCommandBuffer commandBuffer, RasterShaderPipeline* shader) {
				if (generated && visible) {
					planetChunkPushConstant.absolutePosition = planet->absolutePosition + posOnPlanet;
					planetChunkPushConstant.radius = planet->radius;
					shader->SetData(&vertexBuffer, &indexBuffer);
					shader->Execute(renderingDevice, commandBuffer, &planetChunkPushConstant);
				}
			}
		};
		
		std::vector<Chunk> chunks {};
		
		double GetHeightMap(glm::dvec3 normalizedPos) {
			double height = 0;
			return solidRadius + height * heightVariation;
		}
		
		static constexpr std::tuple<glm::dvec3, glm::dvec3, glm::dvec3> GetFaceVectors(int face) {
			glm::dvec3 dir {0};
			glm::dvec3 top {0};
			glm::dvec3 right {0};
			switch (face) {
				case FRONT:
					dir = glm::dvec3(0, 0, 1);
					top = glm::dvec3(0, 1, 0);
					right = glm::dvec3(1, 0, 0);
					break;
				case BACK:
					dir = glm::dvec3(0, 0, -1);
					top = glm::dvec3(0, 1, 0);
					right = glm::dvec3(-1, 0, 0);
					break;
				case RIGHT:
					dir = glm::dvec3(1, 0, 0);
					top = glm::dvec3(0, 1, 0);
					right = glm::dvec3(0, 0, -1);
					break;
				case LEFT:
					dir = glm::dvec3(-1, 0, 0);
					top = glm::dvec3(0, -1, 0);
					right = glm::dvec3(0, 0, -1);
					break;
				case TOP:
					dir = glm::dvec3(0, 1, 0);
					top = glm::dvec3(0, 0, 1);
					right = glm::dvec3(-1, 0, 0);
					break;
				case BOTTOM:
					dir = glm::dvec3(0, -1, 0);
					top = glm::dvec3(0, 0, -1);
					right = glm::dvec3(-1, 0, 0);
					break;
			}
			return {dir, top, right};
		}
		
		void Create(Renderer* renderer, Device* renderingDevice) {
			chunks.reserve(nbBaseChunksPerPlanet);
			for (int face = 0; face < 6; ++face) {
				auto [dir, top, right] = GetFaceVectors(face);
				
				top /= chunkSubdivisionsPerFace;
				right /= chunkSubdivisionsPerFace;
				
				for (int row = 0; row < chunkSubdivisionsPerFace; ++row) {
					for (int col = 0; col < chunkSubdivisionsPerFace; ++col) {
						double bottomOffset =	row*2 - chunkSubdivisionsPerFace;
						double topOffset =		row*2 - chunkSubdivisionsPerFace + 2;
						double leftOffset =		col*2 - chunkSubdivisionsPerFace;
						double rightOffset =	col*2 - chunkSubdivisionsPerFace + 2;
						chunks.push_back({
							this,
							face,
							dir + top*topOffset 	+ right*leftOffset,
							dir + top*topOffset 	+ right*rightOffset,
							dir + top*bottomOffset 	+ right*leftOffset,
							dir + top*bottomOffset 	+ right*rightOffset
						});
					}
				}
			}
			
			// Generate chunks
			for (auto& chunk : chunks) {
				chunk.Create(renderer, renderingDevice);
			}
		}
		
		void Destroy(Device* renderingDevice) {
			for (auto& chunk : chunks) {
				chunk.Destroy(renderingDevice);
			}
			chunks.clear();
		}
		
		void Render(Device* renderingDevice, VkCommandBuffer commandBuffer, RasterShaderPipeline* shader) {
			for (auto& chunk : chunks) {
				chunk.Render(renderingDevice, commandBuffer, shader);
			}
		}
	};
	
	// std::array<PlanetInfo, 255> planets {};
	// Buffer planetsBuffer { VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(PlanetInfo) * 255 };
	
	PlanetInfo planetInfo {
		24000000,
		23900000,
		10000,
		{0, 21800000, -10000000}
	};
	
public:
	
	void Init(Renderer* renderer) {
		// planetsBuffer.AddSrcDataPtr<PlanetInfo, 255>(&planets);
		
		
	}
	
	void Info(Renderer* renderer, Device* renderingDevice) {
		
	}
	
	void InitLayouts(Renderer* renderer, std::vector<DescriptorSet*>& descriptorSets, DescriptorSet* baseDescriptorSet_0) {
		auto* planetDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		// planetDescriptorSet_1->AddBinding_uniformBuffer(0, &planetsBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		
		planetPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		planetPipelineLayout.AddDescriptorSet(planetDescriptorSet_1);
		planetPipelineLayout.AddPushConstant<PlanetInfo::Chunk::PlanetChunkPushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	
	void ConfigureShaders() {
		planetShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
		planetShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		planetShader.depthStencilState.depthWriteEnable = VK_FALSE;
		planetShader.depthStencilState.depthTestEnable = VK_FALSE;
		planetShader.AddVertexInputBinding(sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX, {
			{0, offsetof(Vertex, Vertex::pos), VK_FORMAT_R32G32B32A32_SFLOAT},
			{1, offsetof(Vertex, Vertex::normal), VK_FORMAT_R32G32B32A32_SFLOAT},
		});
	}
	
	void LoadScene() {
		
	}
	
	void ReadShaders() {
		planetShader.ReadShaders();
	}
	
	void CreateResources(Renderer* renderer, Device* renderingDevice, float screenWidth, float screenHeight) {
		
	}
	
	void DestroyResources(Device* renderingDevice) {
		
	}
	
	void AllocateBuffers(Renderer* renderer, Device* renderingDevice, Queue& transferQueue) {
		// renderer->AllocateBufferStaged(transferQueue, planetsBuffer);
		
		//TODO For each planet, and put it in dynamic update
		planetInfo.Create(renderer, renderingDevice);
	}
	
	void FreeBuffers(Renderer* renderer, Device* renderingDevice) {
		// planetsBuffer.Free(renderingDevice);
		
		//TODO For each planet, and put it in dynamic update
		planetInfo.Destroy(renderingDevice);
	}
	
	void CreatePipelines(Renderer* renderer, Device* renderingDevice, std::vector<RasterShaderPipeline*>& opaqueLightingShaders) {
		planetPipelineLayout.Create(renderingDevice);
		opaqueLightingShaders.push_back(&planetShader);
	}
	
	void DestroyPipelines(Renderer* renderer, Device* renderingDevice) {
		planetShader.DestroyPipeline(renderingDevice);
		planetPipelineLayout.Destroy(renderingDevice);
	}
	
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) {
		
	}
	
	void RunDynamic(Device* renderingDevice, VkCommandBuffer commandBuffer) {
		
	}
	
	void RunInOpaqueLightingPass(Device* renderingDevice, VkCommandBuffer commandBuffer) {
		//TODO For each planet
		planetInfo.Render(renderingDevice, commandBuffer, &planetShader);
	}
	
	void RunLowPriorityGraphics(Device* renderingDevice, VkCommandBuffer commandBuffer) {
		
	}
	
	void FrameUpdate(Renderer* renderer, Device* renderingDevice, Camera& mainCamera) {
		
	}
	
	void LowPriorityFrameUpdate(Renderer* renderer, Device* renderingDevice, Camera& mainCamera, Queue& lowPriorityGraphicsQueue) {
		
	}
	
	
	
	
};
