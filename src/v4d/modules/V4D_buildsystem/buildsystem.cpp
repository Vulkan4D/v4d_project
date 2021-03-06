#include <v4d.h>
#include <V4D_Mod.h>

#include "v4d/game/Entity.h"
#include "v4d/game/ServerSideEntity.hpp"
#include "v4d/game/ClientSideEntity.hpp"
#include "v4d/game/ServerSidePlayer.hpp"
#include "v4d/game/Collider.hpp"

#include "utilities/io/Logger.h"
#include "utilities/networking/ListeningServer.h"
#include "utilities/networking/OutgoingConnection.h"

#include "actions.hh"
#include "common.hh"
#include "../V4D_flycam/common.hh"
#include "../V4D_raytracing/Texture2D.hpp"

using namespace v4d::scene;
using namespace networking::actions;
using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::vulkan::rtx;

RasterShaderPipeline* blockShader = nullptr;
RasterShaderPipeline* crosshairShader = nullptr;

std::array<Texture2D, NB_BLOCKS> blocks_tex {
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/0.png"), STBI_rgb_alpha},
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/1.png"), STBI_rgb_alpha},
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/2.png"), STBI_rgb_alpha},
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/3.png"), STBI_rgb_alpha},
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/4.png"), STBI_rgb_alpha},
};

#ifdef _ENABLE_IMGUI
std::array<ImTextureID, NB_BLOCKS> blocks_imGuiImg {};
#endif

Renderer* r = nullptr;
v4d::scene::Scene* scene = nullptr;
PlayerView* playerView = nullptr;
V4D_Mod* mainRenderModule = nullptr;

BuildInterface buildInterface {};
CachedData cachedData {};

V4D_Mod* mainMultiplayerModule = nullptr;
std::shared_ptr<v4d::networking::ListeningServer> server = nullptr;

std::shared_ptr<v4d::networking::OutgoingConnection> client = nullptr;
std::recursive_mutex clientActionQueueMutex;
std::queue<v4d::data::Stream> clientActionQueue {};

void ClientEnqueueAction(v4d::data::WriteOnlyStream& stream) {
	std::lock_guard lock(clientActionQueueMutex);
	clientActionQueue.emplace(stream);
}

V4D_MODULE_CLASS(V4D_Mod) {
	
	V4D_MODULE_FUNC(void, ModuleLoad) {
		playerView = (PlayerView*)V4D_Mod::LoadModule("V4D_flycam")->ModuleGetCustomPtr(0);
		mainRenderModule = V4D_Mod::LoadModule(APP_MAIN_RENDER_MODULE);
		mainMultiplayerModule = V4D_Mod::LoadModule(APP_MAIN_MULTIPLAYER_MODULE);
	}
	
	V4D_MODULE_FUNC(void, ModuleUnload) {
		if (blockShader) delete blockShader;
		if (crosshairShader) delete crosshairShader;
	}
	
	#pragma region Scene & GameObjects
	
	// Client-Only
	V4D_MODULE_FUNC(void, LoadScene, v4d::scene::Scene* s) {
		scene = s;
		buildInterface.scene = scene;
	}
	
	V4D_MODULE_FUNC(void, CreateEntity, int64_t entityUniqueID, uint64_t type) {
		std::lock_guard lock(cachedData.objectMapsMutex);
		switch (type) {
			case OBJECT_TYPE::Build:{
				cachedData.builds[entityUniqueID] = std::make_shared<Build>(entityUniqueID);
				cachedData.buildBlocks[entityUniqueID] = {};
			}break;
		}
	}
	V4D_MODULE_FUNC(void, DestroyEntity, int64_t entityUniqueID, uint64_t type) {
		std::lock_guard lock(cachedData.objectMapsMutex);
		switch (type) {
			case OBJECT_TYPE::Build:{
				try {cachedData.builds.erase(entityUniqueID);} catch(...){}
				try {cachedData.buildBlocks.erase(entityUniqueID);} catch(...){}
			}break;
		}
	}
	
	// Server-Only
	V4D_MODULE_FUNC(void, StreamSendEntityData, int64_t entityUniqueID, uint64_t type, v4d::data::WriteOnlyStream& stream) {
		std::lock_guard lock1(cachedData.serverObjectMapsMutex);
		{// Data over network
			stream.Write(cachedData.serverBuildBlocks[entityUniqueID]);
		}
	}
	
	// Client-Only
	V4D_MODULE_FUNC(void, StreamReceiveEntityData, int64_t entityUniqueID, uint64_t type, v4d::data::ReadOnlyStream& stream) {
		std::lock_guard lock(cachedData.objectMapsMutex);
		{// Data over network
			cachedData.buildBlocks[entityUniqueID] = stream.Read<std::vector<Block>>();
		}
		try { // Update build in-game
			ClientSideEntity::Ptr entity = ClientSideEntity::Get(entityUniqueID);
			auto& build = cachedData.builds.at(entityUniqueID);
			if (build) {
				auto& blocks = cachedData.buildBlocks.at(entityUniqueID);
				entity->renderableGeometryEntityInstances["blocks"] = build->SwapBlocksAndRebuild(blocks);
				entity->posInit = false;
				entity->renderableGeometryEntityInstances["blocks"] = cachedData.builds[entityUniqueID]->CreateEntity();
			}
		} catch(...){}
	}
	
	#pragma endregion
	
	#pragma region Rendering
	
	V4D_MODULE_FUNC(void, InitRenderer, Renderer* _r) {
		r = _r;
		buildInterface.device = r->renderingDevice;
	}
	
	V4D_MODULE_FUNC(void, CreateVulkanResources2, v4d::graphics::vulkan::Device* device) {
		#ifdef _ENABLE_IMGUI
			for (auto& tex : blocks_tex) {
				tex.AllocateAndWriteStagingMemory(device);
				tex.CreateImage(device);
				auto commandBuffer = device->BeginSingleTimeCommands(device->GetQueue("graphics"));
					tex.CopyStagingBufferToImage(device, commandBuffer);
				device->EndSingleTimeCommands(device->GetQueue("graphics"), commandBuffer);
				tex.FreeStagingMemory(device);
			}
		#endif
	}
	V4D_MODULE_FUNC(void, DestroyVulkanResources2, v4d::graphics::vulkan::Device* device) {
		#ifdef _ENABLE_IMGUI
			for (int i = 0; i < NB_BLOCKS; ++i) {
				if (blocks_imGuiImg[i]) {
					// ImGui_ImplVulkan_RemoveTexture(blocks_imGuiImg[i]); // Descriptor pool is already destroyed here, thus this line does not work... maybe find another way, or is it even necessary to free old descriptor sets?...
					blocks_imGuiImg[i] = nullptr;
				}
				blocks_tex[i].DestroyImage(device);
			}
		#endif
		buildInterface.Reset();
	}
	
	V4D_MODULE_FUNC(void, DrawUi2) {
		#ifdef _ENABLE_IMGUI
			ImGui::SetNextWindowPos({20, 150}, ImGuiCond_FirstUseEver); // or ImGuiCond_Once
			ImGui::SetNextWindowSize({460, 160}, ImGuiCond_FirstUseEver);
			ImGui::Begin("Build System");
			auto activeColor = ImVec4{0,1,0, 1.0};
			auto inactiveColor = ImVec4{1,1,1, 0.6};
			
			// Block shapes
			for (int i = 0; i < NB_BLOCKS; ++i) {
				if (!blocks_imGuiImg[i]) blocks_imGuiImg[i] = (ImTextureID)ImGui_ImplVulkan_AddTexture(blocks_tex[i].GetImage()->sampler, blocks_tex[i].GetImage()->view, VK_IMAGE_LAYOUT_GENERAL);
				ImGui::SetCursorPos({float(i) * 74 + 2, 24});
				if (ImGui::ImageButton(blocks_imGuiImg[i], {64, 64}, {0,0}, {1,1}, -1, buildInterface.selectedBlockType==i?ImVec4{0,0.5,0,1}:ImVec4{0,0,0,1}, buildInterface.selectedBlockType==-1?ImVec4{1,1,1,1}:ImVec4{1,1,1,0.8})) {
					buildInterface.selectedBlockType = i;
					buildInterface.paintMode = false;
					buildInterface.isDirty = true;
				}
			}
			
			// Paint
			ImGui::SetCursorPos({float(NB_BLOCKS) * 74 + 2, 24});
			ImGui::PushStyleColor(ImGuiCol_Text, buildInterface.paintMode? activeColor : inactiveColor);
			if (ImGui::Button("Paint", {64, 64})) {
				buildInterface.selectedBlockType = -1;
				buildInterface.paintMode = true;
				buildInterface.isDirty = true;
			}
			ImGui::PopStyleColor();
			
			if (buildInterface.selectedBlockType != -1) {
				// Size/Orientation
				
				ImGui::SetCursorPos({5, 100});
				ImGui::SetNextItemWidth(90);
				ImGui::PushStyleColor(ImGuiCol_Text, buildInterface.selectedEditValue==0? activeColor : inactiveColor);
				if (ImGui::InputFloat("X", &buildInterface.blockSize[buildInterface.selectedBlockType][0], 0.2f, 1.0f, "%.1f", ImGuiInputTextFlags_None)) {
					buildInterface.isDirty = true;
				}
				ImGui::PopStyleColor();
				
				ImGui::SetCursorPos({135, 100});
				ImGui::SetNextItemWidth(90);
				ImGui::PushStyleColor(ImGuiCol_Text, buildInterface.selectedEditValue==1? activeColor : inactiveColor);
				if (ImGui::InputFloat("Y", &buildInterface.blockSize[buildInterface.selectedBlockType][1], 0.2f, 1.0f, "%.1f", ImGuiInputTextFlags_None)) {
					buildInterface.isDirty = true;
				}
				ImGui::PopStyleColor();
				
				ImGui::SetCursorPos({260, 100});
				ImGui::SetNextItemWidth(90);
				ImGui::PushStyleColor(ImGuiCol_Text, buildInterface.selectedEditValue==2? activeColor : inactiveColor);
				if (ImGui::InputFloat("Z", &buildInterface.blockSize[buildInterface.selectedBlockType][2], 0.2f, 1.0f, "%.1f", ImGuiInputTextFlags_None)) {
					buildInterface.isDirty = true;
				}
				ImGui::PopStyleColor();
				
				ImGui::SetCursorPos({5, 125});
				ImGui::PushStyleColor(ImGuiCol_Text, buildInterface.selectedEditValue==3? activeColor : inactiveColor);
				if (ImGui::Button("Rotate")) {
					buildInterface.FindNextValidBlockRotation();
					buildInterface.isDirty = true;
				}
				ImGui::PopStyleColor();
				
			} else if (buildInterface.paintMode) {
				// Colors
				for (int i = 0; i < NB_COLORS; ++i) {
					ImVec4 c{COLORS[i].r, COLORS[i].g, COLORS[i].b, buildInterface.selectedColor==i? 1.0f: 0.2f};
					ImGui::PushStyleColor(ImGuiCol_Button, c);
					ImGui::SetCursorPos({32.0f*i + 5, 100.0f});
					if (ImGui::Button((std::string("    ") + std::to_string(i)).c_str(), {28, 28})) {
						buildInterface.selectedColor = i;
					}
					ImGui::PopStyleColor();
				}
				// Vertex Gradients checkbox
				ImGui::Checkbox("Vertex gradients", &buildInterface.paintModeVertexGradients);
			}
			
			ImGui::End();
		#endif
	}
	
	V4D_MODULE_FUNC(void, RenderFrame_BeforeUpdate) {
		buildInterface.UpdateTmpBlock();
	}
	
	V4D_MODULE_FUNC(void, OnRendererRayCastHit, v4d::graphics::RayCast hit) {
		std::scoped_lock lock(buildInterface.mu, cachedData.objectMapsMutex);
		try {
			auto build = cachedData.builds.at(hit.objId);
			buildInterface.hitBuild = build;
			buildInterface.hitBlock = hit;
		} catch(...){}
	}
	V4D_MODULE_FUNC(void, OnRendererRayCastOut, v4d::graphics::RayCast) {
		std::scoped_lock lock(buildInterface.mu);
		buildInterface.hitBlock = std::nullopt;
		buildInterface.hitBuild = nullptr;
	}
	
	V4D_MODULE_FUNC(void, InitVulkanLayouts) {
		// Crosshair shader
		auto* overlayPipelineLayout = mainRenderModule->GetPipelineLayout("pl_overlay");
		crosshairShader = new RasterShaderPipeline(*overlayPipelineLayout, {
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/crosshair.vert"),
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/crosshair.frag"),
		});
		mainRenderModule->AddShader("sg_overlay", crosshairShader);
	}
	
	V4D_MODULE_FUNC(void, ConfigureShaders) {
		// Blocks
		mainRenderModule->AddRayTracingHitShader(THIS_MODULE, "block");
		
		// Crosshair
		crosshairShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		crosshairShader->depthStencilState.depthTestEnable = VK_FALSE;
		crosshairShader->depthStencilState.depthWriteEnable = VK_FALSE;
		crosshairShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		crosshairShader->SetData(1);
	}
	
	#pragma endregion
	
	#pragma region Server-side networking
	
	V4D_MODULE_FUNC(void, InitServer, std::shared_ptr<v4d::networking::ListeningServer> _srv) {
		server = _srv;
	}
	
	V4D_MODULE_FUNC(void, ServerReceiveAction, v4d::io::SocketPtr stream, v4d::networking::IncomingClientPtr client) {
		// NetworkGameObjectPtr playerObj = serverSideObjects->players.at(client->id);
		auto action = stream->Read<Action>();
		switch (action) {
			
			case CREATE_NEW_BUILD:{
				// Network data
				auto position = stream->Read<Entity::Position>();
				auto orientation = stream->Read<Entity::Orientation>();
				auto block = stream->Read<Block>();
				//
				std::scoped_lock lock(cachedData.serverObjectMapsMutex);
				ServerSideEntity::Ptr entity = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::Build);
				entity->position = position;
				entity->orientation = orientation;
				entity->SetDynamic();
				cachedData.serverBuildBlocks[entity->GetID()].push_back(block);
				entity->Activate();
			}break;
		
			case ADD_BLOCK_TO_BUILD:{
				// Network data
				auto parentId = stream->Read<Entity::Id>();
				auto block = stream->Read<Block>();
				//
				std::scoped_lock lock(cachedData.serverObjectMapsMutex);
				try {
					auto& buildBlocks = cachedData.serverBuildBlocks.at(parentId);
					uint32_t nextIndex = 1;
					for (auto& b : buildBlocks) {
						nextIndex = std::max(nextIndex, b.GetIndex()+1);
					}
					block.SetIndex(nextIndex);
					if (Build::IsBlockAdditionValid(buildBlocks, block)) {
						buildBlocks.push_back(block);
					}
				}catch(...){break;}
				if (ServerSideEntity::Ptr entity = ServerSideEntity::Get(parentId); entity) {
					entity->Iterate();
				}
			}break;
		
			case REMOVE_BLOCK_FROM_BUILD:{
				// Network data
				auto parentId = stream->Read<Entity::Id>();
				uint32_t blockIndex = stream->Read<uint32_t>();
				//
				std::scoped_lock lock(cachedData.serverObjectMapsMutex);
				try {
					auto& buildBlocks = cachedData.serverBuildBlocks.at(parentId);
					for (auto&b : buildBlocks) {
						if (b.GetIndex() == blockIndex) {
							b = buildBlocks.back();
							buildBlocks.pop_back();
							break;
						}
					}
					if (buildBlocks.size() == 0) {
						if (ServerSideEntity::Ptr entity = ServerSideEntity::Get(parentId); entity) {
							entity->Deactivate();
						}
						break;
					}
				}catch(...){break;}
				if (ServerSideEntity::Ptr entity = ServerSideEntity::Get(parentId); entity) {
					entity->Iterate();
				}
			}break;
		
			case PAINT_BLOCK_FACE:{
				// Network data
				auto parentId = stream->Read<Entity::Id>();
				uint32_t blockIndex = stream->Read<uint32_t>();
				auto faceIndex = stream->Read<uint8_t>();
				auto colorIndex = stream->Read<uint8_t>();
				//
				std::scoped_lock lock(cachedData.serverObjectMapsMutex);
				try {
					auto& buildBlocks = cachedData.serverBuildBlocks.at(parentId);
					for (auto&b : buildBlocks) {
						if (b.GetIndex() == blockIndex) {
							b.SetFaceColor(faceIndex, colorIndex);
							break;
						}
					}
				}catch(...){break;}
				if (ServerSideEntity::Ptr entity = ServerSideEntity::Get(parentId); entity) {
					entity->Iterate();
				}
			}break;
		
			case PAINT_BLOCK_VERTEX_GRADIENT:{
				// Network data
				auto parentId = stream->Read<Entity::Id>();
				uint32_t blockIndex = stream->Read<uint32_t>();
				auto vertexIndex = stream->Read<uint8_t>();
				auto colorIndex = stream->Read<uint8_t>();
				//
				std::scoped_lock lock(cachedData.serverObjectMapsMutex);
				try {
					auto& buildBlocks = cachedData.serverBuildBlocks.at(parentId);
					for (auto&b : buildBlocks) {
						if (b.GetIndex() == blockIndex) {
							b.SetVertexGradientColor(vertexIndex, colorIndex);
							break;
						}
					}
				}catch(...){break;}
				if (ServerSideEntity::Ptr entity = ServerSideEntity::Get(parentId); entity) {
					entity->Iterate();
				}
			}break;
			
			default: 
				LOG_ERROR("Server ReceiveAction UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	#pragma endregion
	
	#pragma region Client-Side networking
	
	V4D_MODULE_FUNC(void, InitClient, std::shared_ptr<v4d::networking::OutgoingConnection> _c) {
		client = _c;
	}
	
	V4D_MODULE_FUNC(void, ClientSendActions, v4d::io::SocketPtr stream) {
		std::lock_guard lock(clientActionQueueMutex);
		while (clientActionQueue.size()) {
			stream->Begin();
				stream->EmplaceStream(clientActionQueue.front());
			stream->End();
			clientActionQueue.pop();
		}
	}
	
	#pragma endregion
	
	#pragma region Input
	
	V4D_MODULE_FUNC(std::string, InputCallbackName) {return THIS_MODULE;}
	
	V4D_MODULE_FUNC(void, InputKeyCallback, int key, int scancode, int action, int mods) {
		std::lock_guard lock(buildInterface.mu);
		
		if (action != GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) || key == GLFW_KEY_ESCAPE)
			#endif
		) {
			// LOG(scancode) //TODO build platform-specific mapping for scancode when key == -1
			switch (key) {
				
				case GLFW_KEY_TAB:
					buildInterface.selectedEditValue++;
					if (buildInterface.selectedEditValue > 3) buildInterface.selectedEditValue = 0;
					break;
					
				case GLFW_KEY_0:
					buildInterface.selectedBlockType = -1;
					buildInterface.paintMode = false;
					buildInterface.isDirty = true;
					break;
				case GLFW_KEY_1:
					buildInterface.selectedBlockType = 0;
					buildInterface.paintMode = false;
					buildInterface.isDirty = true;
					break;
				case GLFW_KEY_2:
					buildInterface.selectedBlockType = 1;
					buildInterface.paintMode = false;
					buildInterface.isDirty = true;
					break;
				case GLFW_KEY_3:
					buildInterface.selectedBlockType = 2;
					buildInterface.paintMode = false;
					buildInterface.isDirty = true;
					break;
				case GLFW_KEY_4:
					buildInterface.selectedBlockType = 3;
					buildInterface.paintMode = false;
					buildInterface.isDirty = true;
					break;
				case GLFW_KEY_5:
					buildInterface.selectedBlockType = 4;
					buildInterface.paintMode = false;
					buildInterface.isDirty = true;
					break;
				case GLFW_KEY_6:
					buildInterface.selectedBlockType = -1;
					buildInterface.paintMode = true;
					buildInterface.isDirty = true;
					break;
					
				case GLFW_KEY_X: // delete block
					if (buildInterface.cachedHitBlock.hasHit) {
						auto hitBuild = buildInterface.cachedHitBlock.build.lock();
						if (hitBuild) {
							PackedBlockCustomData customData;
							customData.packed = buildInterface.cachedHitBlock.customData;
							auto parentBlock = hitBuild->GetBlock(customData.blockIndex);
							if (parentBlock.has_value()) {
								v4d::data::WriteOnlyStream stream(32);
									stream << REMOVE_BLOCK_FROM_BUILD;
									// Network data 
									stream << hitBuild->networkId;
									stream << parentBlock->GetIndex();
								ClientEnqueueAction(stream);
							}
						}
					}
					buildInterface.isDirty = true;
					break;
			}
		}
		
		// Shift key for higher precision grid
		if (key == GLFW_KEY_LEFT_SHIFT) {
			bool highPrecisionGrid = (action != GLFW_RELEASE);
			if (buildInterface.highPrecisionGrid != highPrecisionGrid) {
				buildInterface.highPrecisionGrid = highPrecisionGrid;
				buildInterface.isDirty = true;
			}
		}
		// C key for create mode
		if (key == GLFW_KEY_C) {
			bool createMode = (action != GLFW_RELEASE);
			if (buildInterface.createMode != createMode) {
				buildInterface.createMode = createMode;
				buildInterface.isDirty = true;
			}
		}
		
		playerView->canChangeVelocity = buildInterface.selectedBlockType == -1 && !buildInterface.paintMode;
	}
	
	V4D_MODULE_FUNC(void, MouseButtonCallback, int button, int action, int mods) {
		std::lock_guard lock(buildInterface.mu);
		
		if (action == GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)
			#endif
		) {
			switch (button) {
				case GLFW_MOUSE_BUTTON_1:
					// Left Click
					if (buildInterface.selectedBlockType != -1 && buildInterface.isValid) {
						if (buildInterface.tmpBlock) {
							auto block = buildInterface.tmpBlock->block;
							auto parent = buildInterface.tmpBuildParent;
							
							block.SetColor(BLOCK_COLOR_GREY);
							
							if (parent && !buildInterface.createMode) {
								v4d::data::WriteOnlyStream stream(256);
									stream << ADD_BLOCK_TO_BUILD;
									// Network data 
									stream << parent->networkId;
									stream << block;
								ClientEnqueueAction(stream);
							} else if (buildInterface.createMode && !parent) {
								auto t = buildInterface.GetTmpBuildWorldTransform();
								Entity::Position position = t[3];
								Entity::Orientation orientation = glm::quat_cast(t);
								v4d::data::WriteOnlyStream stream(256);
									stream << CREATE_NEW_BUILD;
									// Network data 
									stream << position;
									stream << orientation;
									stream << block;
								ClientEnqueueAction(stream);
							}
							
							// // deselect build tool
							// buildInterface.selectedBlockType = -1;
							// buildInterface.isDirty = true;
						}
					} else if (buildInterface.paintMode && buildInterface.cachedHitBlock.hasHit) {
						auto hitBuild = buildInterface.cachedHitBlock.build.lock();
						if (hitBuild) {
							PackedBlockCustomData customData;
							customData.packed = buildInterface.cachedHitBlock.customData;
							auto parentBlock = hitBuild->GetBlock(customData.blockIndex);
							if (parentBlock.has_value()) {
								if (buildInterface.paintModeVertexGradients) {
									auto points = parentBlock->GetFinalPointsPositions();
									uint8_t vertexIndex = 0;
									float closest = 99999.f;
									for (int i = 0; i < points.size(); ++i) {
										auto dist = glm::distance(buildInterface.cachedHitBlock.hitPositionOnBuild, points[i]);
										if (dist < closest) {
											closest = dist;
											vertexIndex = i;
										}
									}
									v4d::data::WriteOnlyStream stream(256);
										stream << PAINT_BLOCK_VERTEX_GRADIENT;
										// Network data
										stream << hitBuild->networkId;
										stream << parentBlock->GetIndex();
										stream << (uint8_t)vertexIndex;
										stream << (uint8_t)buildInterface.selectedColor;
									ClientEnqueueAction(stream);
								} else {
									if (customData.faceIndex != 7) {
										v4d::data::WriteOnlyStream stream(256);
											stream << PAINT_BLOCK_FACE;
											// Network data
											stream << hitBuild->networkId;
											stream << parentBlock->GetIndex();
											stream << (uint8_t)customData.faceIndex;
											stream << (uint8_t)buildInterface.selectedColor;
										ClientEnqueueAction(stream);
									}
								}
							}
						}
					}
				break;
				case GLFW_MOUSE_BUTTON_2:
					// Right Click
				break;
				case GLFW_MOUSE_BUTTON_3:
					// Middle Click
				break;
			}
		}
		
		playerView->canChangeVelocity = buildInterface.selectedBlockType == -1;
	}
	
	V4D_MODULE_FUNC(void, InputScrollCallback, double x, double y) {
		std::lock_guard lock(buildInterface.mu);
		
		if (buildInterface.selectedBlockType != -1) {
			if (y != 0) {
				if (buildInterface.selectedEditValue < 3) {
					
					// Resize
					float& val = buildInterface.blockSize[buildInterface.selectedBlockType][buildInterface.selectedEditValue];
					if (buildInterface.highPrecisionGrid) {
						val += glm::sign(y) / 5.0f;
					} else {
						val = glm::round(val) + glm::sign(y);
					}
					
					// Minimum size
					if (val < 0.2f) val = buildInterface.highPrecisionGrid? 0.2f : 1.0f;
					
					// Maximum size
					// if (val > 102.4f) val = 102.4f; // actual limit
					if (val > 100.0f) val = 100.0f; // nicer limit
					
				} else {
					// Rotate
					buildInterface.FindNextValidBlockRotation(glm::sign(y));
				}
			} else if (x != 0) {
				buildInterface.selectedEditValue += x;
				if (buildInterface.selectedEditValue > 3) buildInterface.selectedEditValue = 0;
				if (buildInterface.selectedEditValue < 0) buildInterface.selectedEditValue = 3;
			}
		} else if (buildInterface.paintMode) {
			buildInterface.selectedColor += glm::sign(y);
			if (buildInterface.selectedColor >= NB_COLORS) buildInterface.selectedColor = 0;
			if (buildInterface.selectedColor < 0) buildInterface.selectedColor = NB_COLORS;
		}
		buildInterface.isDirty = true;
	}
	
	#pragma endregion
	
};
