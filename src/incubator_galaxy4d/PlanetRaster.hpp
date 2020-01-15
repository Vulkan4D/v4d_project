#pragma once

#include <v4d.h>
#include "../incubator_rendering/V4DRenderingPipeline.hh"
#include "../incubator_rendering/Camera.hpp"
#include "Noise.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

// #define SPHERIFY_CUBE_BY_NORMALIZE // otherwise use technique shown here : http://mathproofs.blogspot.com/2005/07/mapping-cube-to-sphere.html

class Planet {
	
	#pragma region Graphics
	PipelineLayout planetPipelineLayout;
	RasterShaderPipeline planetShader {planetPipelineLayout, {
		"incubator_galaxy4d/assets/shaders/planetRaster.vert",
		"incubator_galaxy4d/assets/shaders/planetRaster.geom",
		"incubator_galaxy4d/assets/shaders/planetRaster.frag",
	}};
	#pragma endregion
	
	#pragma region Static graphics configuration
	static const int chunkSubdivisionsPerFace = 8;
	static const int vertexSubdivisionsPerChunk = 32;
	static constexpr float targetVertexSeparationInMeters = 1.0; // approximative vertex separation in meters for the most precise level of detail
	// static constexpr float chunkLodMaxSizeInScreen = 0.04f; // level of detail ratio of screen size
	#pragma endregion
	
	#pragma region Calculated constants
	static const int nbChunksPerFace = chunkSubdivisionsPerFace * chunkSubdivisionsPerFace;
	static const int nbBaseChunksPerPlanet = nbChunksPerFace * 6;
	static const int nbVerticesPerChunk = (vertexSubdivisionsPerChunk+1) * (vertexSubdivisionsPerChunk+1);
	static const int nbIndicesPerChunk = (vertexSubdivisionsPerChunk) * (vertexSubdivisionsPerChunk) * 6;
	#pragma endregion
	
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
			
			// static std::mutex chunkGeneratorMutex;
			// static std::thread chunkGeneratorThreads;
			// static std::priority_queue<Chunk*> chunksToGenerate;
			// static std::queue<Chunk*> chunksToUnload;
			
			#pragma region Constructor arguments
			PlanetInfo* planet;
			FACE face;
			int level;
			// Cube positions (-1.0 to +1.0)
			glm::dvec3 topLeft;
			glm::dvec3 topRight;
			glm::dvec3 bottomLeft;
			glm::dvec3 bottomRight;
			#pragma endregion
			
			#pragma region Caching
			// Cube positions (-1.0 to +1.0)
			glm::dvec3 top {0};
			glm::dvec3 left {0};
			glm::dvec3 right {0};
			glm::dvec3 bottom {0};
			glm::dvec3 center {0};
			// true positions on planet
			glm::dvec3 centerPos {0};
			glm::dvec3 topLeftPos {0};
			glm::dvec3 topRightPos {0};
			glm::dvec3 bottomLeftPos {0};
			glm::dvec3 bottomRightPos {0};
			glm::dvec3 centerPosLowestPoint {0};
			glm::dvec3 topLeftPosLowestPoint {0};
			glm::dvec3 topRightPosLowestPoint {0};
			glm::dvec3 bottomLeftPosLowestPoint {0};
			glm::dvec3 bottomRightPosLowestPoint {0};
			double chunkSize = 0;
			double heightAtCenter = 0;
			double distanceFromCamera = 0;
			#pragma endregion
			
			#pragma region Data
			std::array<Vertex, nbVerticesPerChunk> vertices;
			std::array<uint32_t, nbIndicesPerChunk> indices;
			std::vector<Chunk*> subChunks {};
			Buffer vertexBuffer { VK_BUFFER_USAGE_VERTEX_BUFFER_BIT };
			Buffer indexBuffer { VK_BUFFER_USAGE_INDEX_BUFFER_BIT };
			// std::recursive_mutex chunkMutex;
			struct PlanetChunkPushConstant { // max 128 bytes
				// glm::dvec3 absolutePosition; // 24
				glm::mat4 mvp; // 16
				glm::vec4 testColor; // 16
				// double radius; // 8
			} planetChunkPushConstant {};
			#pragma endregion
			
			#pragma region States
			volatile bool meshActive = false;
			volatile bool meshGenerated = false;
			volatile bool meshAllocated = false;
			volatile bool chunkVisibleByAngle = false;
			volatile bool hasSubChunks = false;
			volatile bool subChunksReadyToRender = false;
			volatile bool destroyOnNextFrame = false;
			#pragma endregion
			
			Chunk(PlanetInfo* planet, int face, int level, glm::dvec3 topLeft, glm::dvec3 topRight, glm::dvec3 bottomLeft, glm::dvec3 bottomRight)
			: planet(planet), face((FACE)face), level(level), topLeft(topLeft), topRight(topRight), bottomLeft(bottomLeft), bottomRight(bottomRight) {
				
				chunkSize = planet->GetSolidCirconference() / 4.0 / chunkSubdivisionsPerFace / glm::pow(2, level);
				center = (topLeft + topRight + bottomLeft + bottomRight) / 4.0;
				top = (topLeft + topRight) / 2.0;
				left = (topLeft + bottomLeft) / 2.0;
				right = (topRight + bottomRight) / 2.0;
				bottom = (bottomLeft + bottomRight) / 2.0;
				centerPos = Spherify(center);
				heightAtCenter = planet->GetHeightMap(centerPos);
				centerPos = glm::round(centerPos * heightAtCenter);
				topLeftPos = Spherify(topLeft);
				topLeftPos *= planet->GetHeightMap(topLeftPos);
				topRightPos = Spherify(topRight);
				topRightPos *= planet->GetHeightMap(topRightPos);
				bottomLeftPos = Spherify(bottomLeft);
				bottomLeftPos *= planet->GetHeightMap(bottomLeftPos);
				bottomRightPos = Spherify(bottomRight);
				bottomRightPos *= planet->GetHeightMap(bottomRightPos);
				centerPosLowestPoint = Spherify(center) * (planet->solidRadius - glm::max(chunkSize/2.0, planet->heightVariation*2.0));
				topLeftPosLowestPoint = Spherify(topLeft) * (planet->solidRadius - glm::max(chunkSize/2.0, planet->heightVariation*2.0));
				topRightPosLowestPoint = Spherify(topRight) * (planet->solidRadius - glm::max(chunkSize/2.0, planet->heightVariation*2.0));
				bottomLeftPosLowestPoint = Spherify(bottomLeft) * (planet->solidRadius - glm::max(chunkSize/2.0, planet->heightVariation*2.0));
				bottomRightPosLowestPoint = Spherify(bottomRight) * (planet->solidRadius - glm::max(chunkSize/2.0, planet->heightVariation*2.0));
				RefreshDistanceFromCamera();
			
				// planetChunkPushConstant.radius = planet->radius;
				
			}
			
			~Chunk() {
				meshActive = false;
				// std::lock_guard lock(chunkMutex);
			}
			
			static glm::dvec3 Spherify(glm::dvec3 point) {
				#ifdef SPHERIFY_CUBE_BY_NORMALIZE
					point = glm::normalize(point);
				#else
					// http://mathproofs.blogspot.com/2005/07/mapping-cube-to-sphere.html
					double	x2 = point.x * point.x,
							y2 = point.y * point.y,
							z2 = point.z * point.z;
					point.x *= glm::sqrt(1.0 - y2 / 2.0 - z2 / 2.0 + y2 * z2 / 3.0);
					point.y *= glm::sqrt(1.0 - z2 / 2.0 - x2 / 2.0 + z2 * x2 / 3.0);
					point.z *= glm::sqrt(1.0 - x2 / 2.0 - y2 / 2.0 + x2 * y2 / 3.0);
				#endif
				return point;
			}
			
			void GenerateAsync() {
				static bool chunkGeneratorInitialized = false;
				static v4d::processing::ThreadPoolPriorityQueue<Chunk*> chunkGenerator ([](Chunk* chunk){
					chunk->Generate();
				}, [](Chunk* a, Chunk* b) {
					return a->distanceFromCamera > b->distanceFromCamera;
				});
				if (!chunkGeneratorInitialized) {
					chunkGeneratorInitialized = true;
					chunkGenerator.RunThreads(3);
				}
				chunkGenerator.Enqueue(this);
			}
			
			int genRow = 0;
			int genCol = 0;
			int genIndexIndex = 0;
			void Generate() {
				// std::lock_guard lock(chunkMutex);
				
				if (!chunkVisibleByAngle || !meshActive) return;
				
				auto [faceDir, topDir, rightDir] = GetFaceVectors(face);
				
				for (; genRow <= vertexSubdivisionsPerChunk; ++genRow) {
					for (; genCol <= vertexSubdivisionsPerChunk; ++genCol) {
						uint32_t topLeftIndex = (vertexSubdivisionsPerChunk+1) * genRow + genCol;
						
						glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow)/vertexSubdivisionsPerChunk);
						glm::dvec3 leftOffset =	glm::mix(topLeft - center, topRight - center, double(genCol)/vertexSubdivisionsPerChunk);
						
						glm::dvec3 pos = Spherify(center + topDir*topOffset + rightDir*leftOffset);
						
						vertices[topLeftIndex].pos = glm::vec4(pos * planet->GetHeightMap(pos) - centerPos, 0);
						vertices[topLeftIndex].normal = glm::vec4(Spherify(pos), 0);
						
						if (genRow < vertexSubdivisionsPerChunk && genCol < vertexSubdivisionsPerChunk) {
							uint32_t topRightIndex = topLeftIndex+1;
							uint32_t bottomLeftIndex = (vertexSubdivisionsPerChunk+1) * (genRow+1) + genCol;
							uint32_t bottomRightIndex = bottomLeftIndex+1;
							switch (face) {
								case FRONT:
								case LEFT:
								case BOTTOM:
									indices[genIndexIndex++] = topLeftIndex;
									indices[genIndexIndex++] = bottomLeftIndex;
									indices[genIndexIndex++] = bottomRightIndex;
									indices[genIndexIndex++] = topLeftIndex;
									indices[genIndexIndex++] = bottomRightIndex;
									indices[genIndexIndex++] = topRightIndex;
									break;
								case BACK:
								case RIGHT:
								case TOP:
									indices[genIndexIndex++] = topLeftIndex;
									indices[genIndexIndex++] = bottomRightIndex;
									indices[genIndexIndex++] = bottomLeftIndex;
									indices[genIndexIndex++] = topLeftIndex;
									indices[genIndexIndex++] = topRightIndex;
									indices[genIndexIndex++] = bottomRightIndex;
									break;
							}
						}
						
						if (!chunkVisibleByAngle || !meshActive) return;
					}
					genCol = 0;
				}
				meshGenerated = true;
			}
			
			void Allocate(Renderer* renderer, Device* renderingDevice) {
				if (meshGenerated && !meshAllocated) {
					
					vertexBuffer.AddSrcDataPtr<Vertex, nbVerticesPerChunk>(&vertices);
					indexBuffer.AddSrcDataPtr<uint32_t, nbIndicesPerChunk>(&indices);
					
					renderer->AllocateBufferStaged(vertexBuffer);
					renderer->AllocateBufferStaged(indexBuffer);
					
					meshAllocated = true;
				}
			}
			
			void AutoAddSubChunks() {
				// std::lock_guard lock(chunkMutex);
				if (!hasSubChunks) {
					subChunks.reserve(4);
					
					subChunks.push_back(new Chunk{
						planet,
						face,
						level+1,
						topLeft,
						top,
						left,
						center,
					});
					
					subChunks.push_back(new Chunk{
						planet,
						face,
						level+1,
						top,
						topRight,
						center,
						right,
					});
					
					subChunks.push_back(new Chunk{
						planet,
						face,
						level+1,
						left,
						center,
						bottomLeft,
						bottom,
					});
					
					subChunks.push_back(new Chunk{
						planet,
						face,
						level+1,
						center,
						right,
						bottom,
						bottomRight,
					});
					
					// Sort();
					
					hasSubChunks = true;
				}
			}
			
			void AutoRemoveSubChunks(Device* renderingDevice) {
				// std::lock_guard lock(chunkMutex);
				if (hasSubChunks) {
					subChunksReadyToRender = false;
					hasSubChunks = false;
					for (auto* subChunk : subChunks) {
						subChunk->meshActive = false;
						subChunk->AutoRemoveSubChunks(renderingDevice);
						subChunk->Free(renderingDevice);
						delete subChunk;
					}
					subChunks.clear();
				}
			}
			
			void Free(Device* renderingDevice) {
				if (meshAllocated) {
					meshAllocated = false;
					
					vertexBuffer.ResetSrcData();
					indexBuffer.ResetSrcData();
					
					vertexBuffer.Free(renderingDevice);
					indexBuffer.Free(renderingDevice);
				}
			}
			
			void Render(Renderer* renderer, Device* renderingDevice, VkCommandBuffer commandBuffer, RasterShaderPipeline* shader, Camera& camera) {
				// if (!meshActive && meshAllocated) {
				// 	if (destroyOnNextFrame) {
				// 		AutoRemoveSubChunks(renderingDevice);
				// 		Free(renderingDevice);
				// 		destroyOnNextFrame = false;
				// 	} else {
				// 		destroyOnNextFrame = true;
				// 	}
				// }
				if (hasSubChunks && subChunksReadyToRender) {
					for (auto* subChunk : subChunks) if (subChunk) {
						subChunk->Render(renderer, renderingDevice, commandBuffer, shader, camera);
					}
				} else {
					if (chunkVisibleByAngle && meshActive && meshGenerated && meshAllocated && !destroyOnNextFrame) {
						planetChunkPushConstant.mvp = camera.GetProjectionViewMatrix() * glm::translate(glm::dmat4(1), planet->absolutePosition + centerPos);
						shader->SetData(&vertexBuffer, &indexBuffer);
						shader->Execute(renderingDevice, commandBuffer, &planetChunkPushConstant);
					}
				}
			}
			
			void RefreshDistanceFromCamera() {
				distanceFromCamera = glm::distance(planet->cameraPos, centerPos);
				if (distanceFromCamera > chunkSize/2.0)
					distanceFromCamera = glm::min(distanceFromCamera, glm::distance(planet->cameraPos, topLeftPos));
				if (distanceFromCamera > chunkSize/2.0)
					distanceFromCamera = glm::min(distanceFromCamera, glm::distance(planet->cameraPos, topRightPos));
				if (distanceFromCamera > chunkSize/2.0)
					distanceFromCamera = glm::min(distanceFromCamera, glm::distance(planet->cameraPos, bottomLeftPos));
				if (distanceFromCamera > chunkSize/2.0)
					distanceFromCamera = glm::min(distanceFromCamera, glm::distance(planet->cameraPos, bottomRightPos));
				if (distanceFromCamera < chunkSize/2.0)
					distanceFromCamera = glm::max(distanceFromCamera, chunkSize/2.0);
			}
			
			bool IsLastLevel() {
				return (chunkSize/vertexSubdivisionsPerChunk < targetVertexSeparationInMeters*1.9);
			}
			
			bool ShouldAddSubChunks() {
				if (IsLastLevel()) return false;
				return chunkSize / distanceFromCamera > 1.0;
			}
			
			bool ShouldRemoveSubChunks() {
				if (IsLastLevel()) return false;
				return chunkSize / distanceFromCamera < 0.5;
			}
			
			void ProcessVisibility(Renderer* renderer, Device* renderingDevice) {
				RefreshDistanceFromCamera();
				
				// Angle Culling
				chunkVisibleByAngle = 
						glm::dot(planet->cameraPos - centerPosLowestPoint, centerPos) > 0.0 ||
						glm::dot(planet->cameraPos - topLeftPosLowestPoint, topLeftPos) > 0.0 ||
						glm::dot(planet->cameraPos - topRightPosLowestPoint, topRightPos) > 0.0 ||
						glm::dot(planet->cameraPos - bottomLeftPosLowestPoint, bottomLeftPos) > 0.0 ||
						glm::dot(planet->cameraPos - bottomRightPosLowestPoint, bottomRightPos) > 0.0 ;
				
				// Green = last level (most precise)
				if (IsLastLevel())
					planetChunkPushConstant.testColor = glm::vec4{0,1,0,1};
				else
					planetChunkPushConstant.testColor = glm::vec4{1,1,1,1};
				
				if (chunkVisibleByAngle) {
					if (ShouldAddSubChunks()) {
						meshActive = false;
						AutoAddSubChunks();
					} else {
						if (ShouldRemoveSubChunks()) {
							for (auto* subChunk : subChunks) {
								subChunk->meshActive = false;
							}
						}
						// Should generate current chunk
						if (!meshGenerated && !meshActive) {
							meshActive = true;
							GenerateAsync();
						}
					}
					
					if (hasSubChunks) {
						bool subChunksReadyToRender = true;
						for (auto* subChunk : subChunks) {
							subChunk->ProcessVisibility(renderer, renderingDevice);
							// if (subChunk->chunkVisibleByAngle && subChunk->meshActive) {
							// 	if (!subChunk->hasSubChunks && (!subChunk->meshGenerated || !subChunk->meshAllocated)) {
							// 		subChunksReadyToRender = false;
							// 	} else if (subChunk->hasSubChunks && !subChunk->subChunksReadyToRender) {
							// 		subChunksReadyToRender = false;
							// 	}
							// }
						}
						this->subChunksReadyToRender = subChunksReadyToRender;
					}
					
					if (meshActive && meshGenerated && !meshAllocated) {
						Allocate(renderer, renderingDevice);
					}
				}
			}
			
			// void Sort() {
			// 	std::sort(subChunks.begin(), subChunks.end(), [](Chunk* a, Chunk* b) -> bool {return /*a->level > b->level ||*/ a->distanceFromCamera < b->distanceFromCamera;});
			// 	if (hasSubChunks) for (auto* chunk : subChunks) {
			// 		chunk->Sort();
			// 	}
			// }
			
		};
		
		std::vector<Chunk*> chunks {};
		glm::dvec3 cameraPos {};
		bool created = false;
		
		double GetHeightMap(glm::dvec3 normalizedPos) {
			double height = v4d::noise::SimplexFractal(normalizedPos*200.0, 16);
			return solidRadius + height*heightVariation;
		}
		
		double GetSolidCirconference() {
			return solidRadius * 2.0 * 3.14159265359;
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
		
		void AutoCreate(Renderer* renderer, Device* renderingDevice) {
			if (!created) {
				chunks.reserve(nbBaseChunksPerPlanet);
				for (int face = 0; face < 6; ++face) {
					auto [faceDir, topDir, rightDir] = GetFaceVectors(face);
					
					topDir /= chunkSubdivisionsPerFace;
					rightDir /= chunkSubdivisionsPerFace;
					
					for (int row = 0; row < chunkSubdivisionsPerFace; ++row) {
						for (int col = 0; col < chunkSubdivisionsPerFace; ++col) {
							double bottomOffset =	row*2 - chunkSubdivisionsPerFace;
							double topOffset =		row*2 - chunkSubdivisionsPerFace + 2;
							double leftOffset =		col*2 - chunkSubdivisionsPerFace;
							double rightOffset =	col*2 - chunkSubdivisionsPerFace + 2;
							chunks.push_back(new Chunk{
								this,
								face,
								0,
								faceDir + topDir*topOffset 		+ rightDir*leftOffset,
								faceDir + topDir*topOffset 		+ rightDir*rightOffset,
								faceDir + topDir*bottomOffset 	+ rightDir*leftOffset,
								faceDir + topDir*bottomOffset 	+ rightDir*rightOffset
							});
						}
					}
				}
				// Sort();
				created = true;
			}
		}
		
		void AutoDestroy(Device* renderingDevice) {
			if (created) {
				created = false;
				for (auto* chunk : chunks) {
					chunk->meshActive = false;
					chunk->AutoRemoveSubChunks(renderingDevice);
					chunk->Free(renderingDevice);
					delete chunk;
				}
				chunks.clear();
			}
		}
		
		void Render(Renderer* renderer, Device* renderingDevice, VkCommandBuffer commandBuffer, RasterShaderPipeline* shader, Camera& camera) {
			for (auto* chunk : chunks) {
				chunk->Render(renderer, renderingDevice, commandBuffer, shader, camera);
			}
		}
		
		void RefreshCameraPos(Camera& mainCamera) {
			//TODO take planet rotation into consideration
			cameraPos = (mainCamera.GetWorldPosition() - absolutePosition);
		}
		
		void ProcessVisibility(Renderer* renderer, Device* renderingDevice) {
			for (auto* chunk : chunks) {
				chunk->ProcessVisibility(renderer, renderingDevice);
			}
		}
		
		// void Sort() {
		// 	std::sort(chunks.begin(), chunks.end(), [](Chunk* a, Chunk* b) -> bool {return /*a->level > b->level ||*/ a->distanceFromCamera < b->distanceFromCamera;});
		// 	for (auto* chunk : chunks) {
		// 		chunk->Sort();
		// 	}
		// }
		
	};
	
	// std::array<PlanetInfo, 255> planets {};
	// Buffer planetsBuffer { VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(PlanetInfo) * 255 };
	
	PlanetInfo planetInfo {
		24000000,
		23900000,
		10000,
		// {0, 21740000, -10000000}
		{0, 28000000, -10000000}
		// {20000000, 20000000, -20000000}
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
		planetShader.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		planetShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		planetShader.depthStencilState.depthWriteEnable = VK_TRUE;
		planetShader.depthStencilState.depthTestEnable = VK_TRUE;
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
	}
	
	void FreeBuffers(Renderer* renderer, Device* renderingDevice) {
		// planetsBuffer.Free(renderingDevice);
		
		//TODO For each planet, and put it in dynamic update
		planetInfo.AutoDestroy(renderingDevice);
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
	
	void RunInOpaqueLightingPass(Renderer* renderer, Device* renderingDevice, VkCommandBuffer commandBuffer, Camera& mainCamera) {
		//TODO For each planet
		planetInfo.Render(renderer, renderingDevice, commandBuffer, &planetShader, mainCamera);
	}
	
	void RunLowPriorityGraphics(Device* renderingDevice, VkCommandBuffer commandBuffer) {
		
	}
	
	void FrameUpdate(Renderer* renderer, Device* renderingDevice, Camera& mainCamera) {
		
	}
	
	void LowPriorityFrameUpdate(Renderer* renderer, Device* renderingDevice, Camera& mainCamera, Queue& lowPriorityGraphicsQueue) {
		//TODO For each planet
		planetInfo.RefreshCameraPos(mainCamera);
		planetInfo.AutoCreate(renderer, renderingDevice);
		planetInfo.ProcessVisibility(renderer, renderingDevice);
		
		// LOG(glm::distance(planetInfo.absolutePosition, mainCamera.GetWorldPosition()) - planetInfo.solidRadius + planetInfo.heightVariation);
	}
	
	
	
	
};


// std::mutex Planet::PlanetInfo::Chunk::chunkGeneratorMutex {};
// std::priority_queue<Planet::PlanetInfo::Chunk*> Planet::PlanetInfo::Chunk::chunksToGenerate {};
// std::queue<Planet::PlanetInfo::Chunk*> Planet::PlanetInfo::Chunk::chunksToUnload {};

