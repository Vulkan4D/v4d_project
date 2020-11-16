#define _V4D_MODULE
#include <v4d.h>
#include "actions.hh"
#include "common.hh"
#include "../V4D_flycam/common.hh"
#include "../V4D_hybrid/Texture2D.hpp"
#include "../V4D_multiplayer/ServerSideObjects.hh"

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
std::array<ImTextureID, NB_BLOCKS> blocks_imGuiImg {};

Renderer* r = nullptr;
v4d::scene::Scene* scene = nullptr;
PlayerView* playerView = nullptr;
V4D_Mod* mainRenderModule = nullptr;

BuildInterface buildInterface {};
CachedData cachedData {};

V4D_Mod* mainMultiplayerModule = nullptr;
std::shared_ptr<v4d::networking::ListeningServer> server = nullptr;
ServerSideObjects* serverSideObjects = nullptr;

std::shared_ptr<OutgoingConnection> client = nullptr;
std::recursive_mutex clientActionQueueMutex;
std::queue<v4d::data::Stream> clientActionQueue {};

void ClientEnqueueAction(v4d::data::WriteOnlyStream& stream) {
	std::lock_guard lock(clientActionQueueMutex);
	clientActionQueue.emplace(stream);
}

V4D_MODULE_CLASS(V4D_Mod) {
	
	V4D_MODULE_FUNC(void, ModuleLoad) {
		playerView = (PlayerView*)V4D_Mod::LoadModule("V4D_flycam")->ModuleGetCustomPtr(0);
		mainRenderModule = V4D_Mod::LoadModule("V4D_hybrid");
		mainMultiplayerModule = V4D_Mod::LoadModule("V4D_multiplayer");
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
		
		static const int maxObjectsInMemory = 10; // 256 objects = 229 Mb
		static const int maxBlocksInMemory = maxObjectsInMemory * 1024;
		v4d::scene::Geometry::globalBuffers.objectBuffer.Extend(maxObjectsInMemory);
		v4d::scene::Geometry::globalBuffers.geometryBuffer.Extend(maxObjectsInMemory);
		v4d::scene::Geometry::globalBuffers.vertexBuffer.Extend(maxBlocksInMemory * Block::MAX_VERTICES);
		v4d::scene::Geometry::globalBuffers.indexBuffer.Extend(maxBlocksInMemory * Block::MAX_INDICES);
	}
	V4D_MODULE_FUNC(void, UnloadScene) {
		buildInterface.UnloadScene();
	}
	V4D_MODULE_FUNC(void, CreateGameObject, v4d::scene::NetworkGameObjectPtr obj) {
		std::lock_guard lock(cachedData.objectMapsMutex);
		switch (obj->type) {
			case OBJECT_TYPE::Build:{
				cachedData.builds[obj->id] = std::make_shared<Build>(obj->id);
				cachedData.buildBlocks[obj->id] = {};
			}break;
		}
	}
	V4D_MODULE_FUNC(void, DestroyGameObject, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		std::lock_guard lock(cachedData.objectMapsMutex);
		switch (obj->type) {
			case OBJECT_TYPE::Build:{
				try {cachedData.builds.erase(obj->id);} catch(...){}
				try {cachedData.buildBlocks.erase(obj->id);} catch(...){}
			}break;
		}
	}
	V4D_MODULE_FUNC(void, AddGameObjectToScene, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		std::lock_guard lock(cachedData.objectMapsMutex);
		switch (obj->type) {
			case OBJECT_TYPE::Build:{
				obj->objectInstance = cachedData.builds[obj->id]->AddToScene(scene);
				obj->objectInstance->AssignToModule(THIS_MODULE, obj->id);
				obj->objectInstance->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
			}break;
		}
	}
	
	// Server-Only
	V4D_MODULE_FUNC(void, SendStreamCustomGameObjectData, v4d::scene::NetworkGameObjectPtr obj, v4d::data::WriteOnlyStream& stream) {
		std::lock_guard lock1(cachedData.serverObjectMapsMutex);
		{// Data over network
			stream.Write(cachedData.serverBuildBlocks[obj->id]);
		}
	}
	
	// Client-Only
	V4D_MODULE_FUNC(void, ReceiveStreamCustomGameObjectData, v4d::scene::NetworkGameObjectPtr obj, v4d::data::ReadOnlyStream& stream) {
		std::lock_guard lock(cachedData.objectMapsMutex);
		{// Data over network
			cachedData.buildBlocks[obj->id] = stream.Read<std::vector<Block>>();
		}
		try { // Update build in-game
			auto& build = cachedData.builds.at(obj->id);
			LOG("Client Reveived build")
			if (build) {
				auto& blocks = cachedData.buildBlocks.at(obj->id);
				build->SwapBlocksVector(blocks);
			}
		} catch(...){}
	}
	
	#pragma endregion
	
	#pragma region Rendering
	
	V4D_MODULE_FUNC(void, InitRenderer, Renderer* _r) {
		r = _r;
	}
	
	V4D_MODULE_FUNC(void, CreateVulkanResources2, v4d::graphics::vulkan::Device* device) {
		for (auto& tex : blocks_tex) {
			tex.AllocateAndWriteStagingMemory(device);
			tex.CreateImage(device, VK_IMAGE_TILING_OPTIMAL);
			auto commandBuffer = device->BeginSingleTimeCommands(device->GetQueue("graphics"));
				tex.CopyStagingBufferToImage(device, commandBuffer);
			device->EndSingleTimeCommands(device->GetQueue("graphics"), commandBuffer);
			tex.FreeStagingMemory(device);
		}
	}
	V4D_MODULE_FUNC(void, DestroyVulkanResources2, v4d::graphics::vulkan::Device* device) {
		for (int i = 0; i < NB_BLOCKS; ++i) {
			if (blocks_imGuiImg[i]) {
				// ImGui_ImplVulkan_RemoveTexture(blocks_imGuiImg[i]); // Descriptor pool is already destroyed here, thus this line does not work... maybe find another way, or is it even necessary to free old descriptor sets?...
				blocks_imGuiImg[i] = nullptr;
			}
			blocks_tex[i].DestroyImage(device);
		}
	}
	
	V4D_MODULE_FUNC(void, DrawUi2) {
		ImGui::SetNextWindowPos({20, 150}, ImGuiCond_FirstUseEver); // or ImGuiCond_Once
		ImGui::SetNextWindowSize({380, 160}, ImGuiCond_FirstUseEver);
		ImGui::Begin("Build System");
		for (int i = 0; i < NB_BLOCKS; ++i) {
			if (!blocks_imGuiImg[i]) blocks_imGuiImg[i] = (ImTextureID)ImGui_ImplVulkan_AddTexture(blocks_tex[i].GetImage()->sampler, blocks_tex[i].GetImage()->view, VK_IMAGE_LAYOUT_GENERAL);
			ImGui::SetCursorPos({float(i) * 74 + 2, 24});
			if (ImGui::ImageButton(blocks_imGuiImg[i], {64, 64}, {0,0}, {1,1}, -1, buildInterface.selectedBlockType==i?ImVec4{0,0.5,0,1}:ImVec4{0,0,0,1}, buildInterface.selectedBlockType==-1?ImVec4{1,1,1,1}:ImVec4{1,1,1,0.8})) {
				buildInterface.selectedBlockType = i;
				buildInterface.RemakeTmpBlock();
			}
		}
		if (buildInterface.selectedBlockType != -1) {
			auto activeColor = ImVec4{0,1,0, 1.0};
			auto inactiveColor = ImVec4{1,1,1, 0.6};
			
			ImGui::SetCursorPos({5, 100});
			ImGui::SetNextItemWidth(90);
			ImGui::PushStyleColor(ImGuiCol_Text, buildInterface.selectedEditValue==0? activeColor : inactiveColor);
			if (ImGui::InputFloat("X", &buildInterface.blockSize[buildInterface.selectedBlockType][0], 0.2f, 1.0f, 1, ImGuiInputTextFlags_None)) {
				buildInterface.RemakeTmpBlock();
			}
			ImGui::PopStyleColor();
			
			ImGui::SetCursorPos({135, 100});
			ImGui::SetNextItemWidth(90);
			ImGui::PushStyleColor(ImGuiCol_Text, buildInterface.selectedEditValue==1? activeColor : inactiveColor);
			if (ImGui::InputFloat("Y", &buildInterface.blockSize[buildInterface.selectedBlockType][1], 0.2f, 1.0f, 1, ImGuiInputTextFlags_None)) {
				buildInterface.RemakeTmpBlock();
			}
			ImGui::PopStyleColor();
			
			ImGui::SetCursorPos({260, 100});
			ImGui::SetNextItemWidth(90);
			ImGui::PushStyleColor(ImGuiCol_Text, buildInterface.selectedEditValue==2? activeColor : inactiveColor);
			if (ImGui::InputFloat("Z", &buildInterface.blockSize[buildInterface.selectedBlockType][2], 0.2f, 1.0f, 1, ImGuiInputTextFlags_None)) {
				buildInterface.RemakeTmpBlock();
			}
			ImGui::PopStyleColor();
			
			ImGui::SetCursorPos({5, 125});
			ImGui::PushStyleColor(ImGuiCol_Text, buildInterface.selectedEditValue==3? activeColor : inactiveColor);
			if (ImGui::Button("Rotate")) {
				buildInterface.NextBlockRotation();
				buildInterface.RemakeTmpBlock();
			}
			ImGui::PopStyleColor();
		}
		ImGui::End();
	}
	
	V4D_MODULE_FUNC(void, BeginFrameUpdate) {
		std::lock_guard lock(buildInterface.mu);
		buildInterface.UpdateTmpBlock();
		buildInterface.hitBlock = std::nullopt;
		buildInterface.hitBuild = nullptr;
	}
	
	V4D_MODULE_FUNC(void, OnRendererRayCastHit, v4d::graphics::RenderRayCastHit hit) {
		std::scoped_lock lock(buildInterface.mu, cachedData.objectMapsMutex);
		try {
			auto build = cachedData.builds.at(hit.objId);
			buildInterface.hitBuild = build;
			buildInterface.hitBlock = hit;
		} catch(...){}
	}
	
	V4D_MODULE_FUNC(void, InitVulkanLayouts) {
		
		// Blocks shader
		auto* rasterVisibilityPipelineLayout = mainRenderModule->GetPipelineLayout("pl_visibility_raster");
		blockShader = new RasterShaderPipeline(*rasterVisibilityPipelineLayout, {
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/blocks.vert"),
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/blocks.frag"),
		});
		mainRenderModule->AddShader("sg_visibility", blockShader);
		
		// Crosshair shader
		auto* overlayPipelineLayout = mainRenderModule->GetPipelineLayout("pl_overlay");
		crosshairShader = new RasterShaderPipeline(*overlayPipelineLayout, {
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/crosshair.vert"),
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/crosshair.frag"),
		});
		mainRenderModule->AddShader("sg_overlay", crosshairShader);
	}
	
	V4D_MODULE_FUNC(void, ConfigureShaders) {
		auto* shaderBindingTableVisibility = mainRenderModule->GetShaderBindingTable("sbt_visibility");
		auto* shaderBindingTableLighting = mainRenderModule->GetShaderBindingTable("sbt_lighting");
		
		// Blocks
		Geometry::geometryRenderTypes["block"].sbtOffset = 
			shaderBindingTableVisibility->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/blocks.rchit"));
			shaderBindingTableLighting->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/blocks.rchit"));
		Geometry::geometryRenderTypes["block"].rasterShader = blockShader;
		blockShader->AddVertexInputBinding(sizeof(Geometry::VertexBuffer_T), VK_VERTEX_INPUT_RATE_VERTEX, Geometry::VertexBuffer_T::GetInputAttributes());
		blockShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
			blockShader->SetData(&Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer, 0);
		#else
			blockShader->SetData(&Geometry::globalBuffers.vertexBuffer, &Geometry::globalBuffers.indexBuffer, 0);
		#endif
		
		// Crosshair
		crosshairShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		crosshairShader->depthStencilState.depthTestEnable = VK_FALSE;
		crosshairShader->depthStencilState.depthWriteEnable = VK_FALSE;
		crosshairShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		crosshairShader->SetData(1);
	}
	
	#pragma endregion
	
	#pragma region Server-side networking
	
	V4D_MODULE_FUNC(void, InitServer, std::shared_ptr<ListeningServer> _srv) {
		server = _srv;
		serverSideObjects = (ServerSideObjects*)mainMultiplayerModule->ModuleGetCustomPtr(0);
	}
	
	V4D_MODULE_FUNC(void, ServerReceiveAction, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		// NetworkGameObjectPtr playerObj = serverSideObjects->players.at(client->id);
		auto action = stream->Read<Action>();
		switch (action) {
			
			case CREATE_NEW_BUILD:{
				// Network data
				auto transform = stream->Read<NetworkGameObjectTransform>();
				auto block = stream->Read<Block>();
				//
				std::scoped_lock lock(serverSideObjects->mutex, cachedData.serverObjectMapsMutex);
				auto obj = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::Build);
				obj->SetTransformFromNetwork(transform);
				obj->isDynamic = true;
				obj->physicsClientID = client->id;
				cachedData.serverBuildBlocks[obj->id].push_back(block);
			}break;
		
			case ADD_BLOCK_TO_BUILD:{
				// Network data
				auto parentId = stream->Read<NetworkGameObject::Id>();
				auto block = stream->Read<Block>();
				//
				std::scoped_lock lock(serverSideObjects->mutex, cachedData.serverObjectMapsMutex);
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
				try {
					auto obj = serverSideObjects->objects.at(parentId);
					obj->Iterate();
				}catch(...){break;}
			}break;
		
			case REMOVE_BLOCK_FROM_BUILD:{
				// Network data
				auto parentId = stream->Read<NetworkGameObject::Id>();
				uint32_t blockIndex = stream->Read<uint32_t>();
				//
				std::scoped_lock lock(serverSideObjects->mutex, cachedData.serverObjectMapsMutex);
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
						auto obj = serverSideObjects->objects.at(parentId);
						obj->active = false;
						break;
					}
				}catch(...){break;}
				try {
					auto obj = serverSideObjects->objects.at(parentId);
					obj->Iterate();
				}catch(...){break;}
			}break;
		
			default: 
				LOG_ERROR("Server ReceiveAction UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	#pragma endregion
	
	#pragma region Client-Side networking
	
	V4D_MODULE_FUNC(void, InitClient, std::shared_ptr<OutgoingConnection> _c) {
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
				&& (!ImGui::IsAnyWindowFocused() || key == GLFW_KEY_ESCAPE)
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
					break;
				case GLFW_KEY_1:
					buildInterface.selectedBlockType = 0;
					break;
				case GLFW_KEY_2:
					buildInterface.selectedBlockType = 1;
					break;
				case GLFW_KEY_3:
					buildInterface.selectedBlockType = 2;
					break;
				case GLFW_KEY_4:
					buildInterface.selectedBlockType = 3;
					break;
				case GLFW_KEY_5:
					buildInterface.selectedBlockType = 4;
					break;
					
				case GLFW_KEY_X: // delete block
					if (buildInterface.cachedHitBlock.hasHit) {
						auto hitBuild = buildInterface.cachedHitBlock.build.lock();
						if (hitBuild) {
							PackedBlockCustomData customData;
							customData.packed = buildInterface.cachedHitBlock.customData0;
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
					break;
			}
			buildInterface.RemakeTmpBlock();
		}
		
		// Shift key for higher precision grid
		if (key == GLFW_KEY_LEFT_SHIFT) {
			if (action != GLFW_RELEASE) buildInterface.highPrecisionGrid = true;
			else buildInterface.highPrecisionGrid = false;
		}
		
		playerView->canChangeVelocity = buildInterface.selectedBlockType == -1;
	}
	
	V4D_MODULE_FUNC(void, MouseButtonCallback, int button, int action, int mods) {
		std::lock_guard lock(buildInterface.mu);
		
		if (action == GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsAnyWindowFocused()
			#endif
		) {
			switch (button) {
				case GLFW_MOUSE_BUTTON_1:
					// Left Click
					if (buildInterface.selectedBlockType != -1 && buildInterface.isValid) {
						if (buildInterface.tmpBlock && buildInterface.tmpBlock->block) {
							auto block = *buildInterface.tmpBlock->block;
							auto parent = buildInterface.tmpBuildParent;
							
							block.SetColor(BLOCK_COLOR_GREY);
							
							if (parent) {
								v4d::data::WriteOnlyStream stream(256);
									stream << ADD_BLOCK_TO_BUILD;
									// Network data 
									stream << parent->networkId;
									stream << block;
								ClientEnqueueAction(stream);
							} else {
								NetworkGameObjectTransform transform {};
								transform.SetFromTransformAndVelocity(buildInterface.GetTmpBuildWorldTransform(), {0,0,0});
								v4d::data::WriteOnlyStream stream(256);
									stream << CREATE_NEW_BUILD;
									// Network data 
									stream << transform;
									stream << block;
								ClientEnqueueAction(stream);
							}
							
							// // deselect build tool
							// buildInterface.selectedBlockType = -1;
							// buildInterface.RemakeTmpBlock();
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
					buildInterface.NextBlockRotation(y);
				}
			} else if (x != 0) {
				buildInterface.selectedEditValue += x;
				if (buildInterface.selectedEditValue > 3) buildInterface.selectedEditValue = 0;
				if (buildInterface.selectedEditValue < 0) buildInterface.selectedEditValue = 3;
			}
		}
		buildInterface.RemakeTmpBlock();
	}
	
	#pragma endregion
	
};