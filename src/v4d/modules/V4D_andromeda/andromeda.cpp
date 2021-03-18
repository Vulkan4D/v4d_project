#include <v4d.h>

#include "GalaxyGenerator.h"
#include "Celestial.h"
#include "StarSystem.h"

#include "../V4D_multiplayer/ServerSideObjects.hh"
#include "../V4D_multiplayer/ClientSideObjects.hh"

#pragma region Global Pointers

	V4D_Mod* mainRenderModule = nullptr;
	V4D_Mod* mainMultiplayerModule = nullptr;
	v4d::graphics::Window* window = nullptr;
	v4d::graphics::Renderer* r = nullptr;
	v4d::scene::Scene* scene = nullptr;
	std::shared_ptr<ListeningServer> server = nullptr;
	ServerSideObjects* serverSideObjects = nullptr;
	std::shared_ptr<OutgoingConnection> client = nullptr;
	ClientSideObjects* clientSideObjects = nullptr;

#pragma endregion

#pragma region Networking

	namespace networking::action {
		typedef uint8_t Action;
		// const Action EXTENDED_ACTION = 0;

		// Action stream Only (TCP)
		const Action ASSIGN_PLAYER_OBJ = 0; // + (objectID) = 4
		
		const Action TEST_OBJ = 101; // + (string) = variable size

	}

	namespace OBJECT_TYPE {
		const uint32_t Player = 0;
		const uint32_t Ball = 1;
	}

	std::recursive_mutex serverActionQueueMutex;
	std::unordered_map<uint64_t /* clientID */, std::queue<v4d::data::Stream>> serverActionQueuePerClient {};
	void ServerEnqueueAction(uint64_t clientId, v4d::data::WriteOnlyStream& stream) {
		std::lock_guard lock(serverActionQueueMutex);
		serverActionQueuePerClient[clientId].emplace(stream);
	}

	std::recursive_mutex clientActionQueueMutex;
	std::queue<v4d::data::Stream> clientActionQueue {};
	void ClientEnqueueAction(v4d::data::WriteOnlyStream& stream) {
		std::lock_guard lock(clientActionQueueMutex);
		clientActionQueue.emplace(stream);
	}

#pragma endregion

#pragma region Galaxy

	struct GalaxySnapshot {
		GalacticPosition galacticPosition;
		glm::dvec3 positionInStarSystem;
		glm::dvec3 positionInGalaxyLY;
		double timestamp;
		
		mutable std::mutex mu;
		std::unique_lock<std::mutex> Lock() const {return std::unique_lock<std::mutex>(mu);}
		void Set(const GalacticPosition& galacticPosition, const glm::dvec3& positionInStarSystem, const glm::dvec3& positionInGalaxyLY, const double& timestamp) {
			std::lock_guard lock(mu);
			this->galacticPosition = galacticPosition;
			this->positionInStarSystem = positionInStarSystem;
			this->positionInGalaxyLY = positionInGalaxyLY;
			this->timestamp = timestamp;
		}
		GalaxySnapshot() = default;
		GalaxySnapshot(const GalaxySnapshot& snapshot) {
			std::scoped_lock lock(mu, snapshot.mu);
			this->galacticPosition = snapshot.galacticPosition;
			this->positionInStarSystem = snapshot.positionInStarSystem;
			this->positionInGalaxyLY = snapshot.positionInGalaxyLY;
			this->timestamp = snapshot.timestamp;
		}
	} currentGalaxySnapshot;

	constexpr bool GALAXY_FADE = false;
	
	v4d::graphics::vulkan::RenderPass galaxyBackgroundRenderPass;
	v4d::graphics::vulkan::RasterShaderPipeline* galaxyBackgroundShader = nullptr;
	v4d::graphics::vulkan::RasterShaderPipeline* galaxyFadeShader = nullptr;
	v4d::graphics::CubeMapImage* img_background = nullptr;
	struct StarVertex {
		glm::vec4 position;
		glm::vec4 color;
		static std::vector<v4d::graphics::vulkan::VertexInputAttributeDescription> GetInputAttributes() {
			return {
				{0, offsetof(StarVertex, position), VK_FORMAT_R32G32B32A32_SFLOAT},
				{1, offsetof(StarVertex, color), VK_FORMAT_R32G32B32A32_SFLOAT},
			};
		}
	};
	struct GalaxyPushConstant {
		// used for stars and fade
		float screenSize = 0;
		float brightnessFactor = 1.0;
		// used only for stars
		float minViewDistance = 0.1;
		float maxViewDistance = STAR_MAX_VISIBLE_DISTANCE; // Light-years
		glm::vec4 relativePosition {0}; // w = sizeFactor
	};
	struct GalaxyChunk {
		v4d::graphics::StagingBuffer<StarVertex, uint(STAR_CHUNK_SIZE*STAR_CHUNK_SIZE*STAR_CHUNK_SIZE*STAR_CHUNK_DENSITY_MULT*1.333)> buffer {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT};
		uint32_t count = 0;
		GalaxyPushConstant pushConstant;
		bool allocated = false;
		bool pushed = false;
		float sizeFactor = 1.0;
		
		void Allocate() {
			buffer.Allocate(r->renderingDevice);
			allocated = true;
		}
		void Free() {
			buffer.Free();
			allocated = false;
		}
		void Push(VkCommandBuffer commandBuffer) {
			if (!pushed) {
				pushConstant.screenSize = img_background->width;
				buffer.Push(commandBuffer, count);
				pushed = true;
			}
		}
		void Update(glm::dvec3 pos) {
			pushConstant.relativePosition = glm::vec4(pos, sizeFactor);
		}
		void GenerateStars(glm::ivec3 pos) {
			if (!allocated) Allocate();
			const glm::ivec3 bounds {int(STAR_CHUNK_SIZE/2)};
			// Galaxy Gen
			for (int x = -bounds.x; x < bounds.x; ++x) {
				for (int y = -bounds.y; y < bounds.y; ++y) {
					for (int z = -bounds.z; z < bounds.z; ++z) {
						const glm::ivec3 posInGalaxy = pos + glm::ivec3(x,y,z);
						if (GalaxyGenerator::GetStarSystemPresence(posInGalaxy)) {
							const StarSystem& starSystem(posInGalaxy);
							glm::vec4 color = starSystem.GetVisibleColor();
							if (color.a > 0.0001) {
								const glm::dvec3& offset = starSystem.GetOffsetLY();
								buffer[count++] = {
									{x+offset.x, y+offset.y, z+offset.z, color.a},
									color
								};
							}
						}
					}
				}
			}
		}
		void Draw(VkCommandBuffer commandBuffer) {
			galaxyBackgroundShader->SetData(buffer.GetDeviceLocalBuffer(), count);
			galaxyBackgroundShader->Execute(r->renderingDevice, commandBuffer, 1, &pushConstant);
		}
	};
	struct PlanetDebug {
		v4d::graphics::Buffer buffer {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(StarVertex)};
		GalaxyPushConstant pushConstant;
		bool allocated = false;
		
		void Allocate() {
			buffer.Allocate(r->renderingDevice, v4d::graphics::MemoryUsage::MEMORY_USAGE_CPU_TO_GPU);
			buffer.MapMemory(r->renderingDevice);
			allocated = true;
		}
		void Free() {
			if (allocated) {
				buffer.UnmapMemory(r->renderingDevice);
				buffer.Free(r->renderingDevice);
				allocated = false;
			}
		}
		StarVertex* operator->() {
			if (!allocated) Allocate();
			return (StarVertex*)buffer.data;
		}
		void Update(glm::dvec3 position, Celestial* celestial) {
			pushConstant.brightnessFactor = 1.0;
			pushConstant.minViewDistance = 0;
			pushConstant.maxViewDistance = 0;
			pushConstant.relativePosition = glm::vec4(position, 2.0);
			(*this)->position = glm::dvec4(0);
			(*this)->color = glm::vec4(0);
			switch (celestial->GetType()) {
				case CelestialType::HyperGiant: // white
					(*this)->color = glm::vec4(1,1,1,1);
					(*this)->position.w = 8.0;
					break;
				case CelestialType::Star: // yellow
					(*this)->color = glm::vec4(1,1,0,1);
					(*this)->position.w = 4.0;
					break;
				case CelestialType::BrownDwarf: // red
					(*this)->color = glm::vec4(1,0,0,1);
					(*this)->position.w = 2.0;
					break;
				case CelestialType::Planet: // green
					(*this)->color = glm::vec4(0,1,0,1);
					break;
				case CelestialType::GasGiant: // turquoise
					(*this)->color = glm::vec4(0,1,1,1);
					(*this)->position.w = 2.0;
					break;
				case CelestialType::BlackHole: // blue
					(*this)->color = glm::vec4(0,0,1,1);
					(*this)->position.w = 4.0;
					break;
				case CelestialType::Asteroid: // orange
					(*this)->color = glm::vec4(1,0.5,0,0.5);
					(*this)->position.w = 0.5;
					break;
				case CelestialType::SuperMassiveBlackHole: // pink
					(*this)->color = glm::vec4(1,0,1,1);
					(*this)->position.w = 8.0;
					break;
			}
		}
		void Draw(VkCommandBuffer commandBuffer) {
			galaxyBackgroundShader->SetData(buffer.buffer, 1);
			galaxyBackgroundShader->Execute(r->renderingDevice, commandBuffer, 1, &pushConstant);
		}
	};
	std::unordered_map<uint64_t, PlanetDebug> planetsDebug {};
	std::unordered_map<glm::ivec3, GalaxyChunk> galaxyChunks {};

#pragma endregion




#include "../V4D_flycam/common.hh"
PlayerView* playerView = nullptr;

GalacticPosition GetDefaultGalacticPosition() {
	GalacticPosition defaultGalacticPosition;
	defaultGalacticPosition.SetReferenceFrame(135983499432086); // very large trinary system with over 300 bodies
	// defaultGalacticPosition.SetReferenceFrame(139712601895775); // 8 stars
	// defaultGalacticPosition.SetReferenceFrame(134436245599421); // very small system
	return defaultGalacticPosition;
}



V4D_MODULE_CLASS(V4D_Mod) {
	
#pragma region Init
	
	V4D_MODULE_FUNC(int, OrderIndex) {return -10;}
	
	V4D_MODULE_FUNC(void, LoadScene, v4d::scene::Scene* _s) {
		scene = _s;
		
		// Load Dependencies
		mainRenderModule = V4D_Mod::LoadModule(APP_MAIN_RENDER_MODULE);
		mainMultiplayerModule = V4D_Mod::LoadModule(APP_MAIN_MULTIPLAYER_MODULE);
		
		playerView = (PlayerView*)V4D_Mod::LoadModule("V4D_flycam")->ModuleGetCustomPtr(0);
		
		playerView->SetInitialViewDirection({0,1,0}, {0,0,1});
		playerView->useFreeFlyCam = true;
		playerView->camSpeed = LY2M(0.1);
		
		galaxyBackgroundShader = new v4d::graphics::vulkan::RasterShaderPipeline(*mainRenderModule->GetPipelineLayout("pl_background"), {
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/galaxy.vert"),
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/galaxy.geom"),
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/galaxy.frag"),
		});
		mainRenderModule->AddShader("sg_background", galaxyBackgroundShader);
		galaxyFadeShader = new v4d::graphics::vulkan::RasterShaderPipeline(*mainRenderModule->GetPipelineLayout("pl_background"), {
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/galaxy.fade.vert"),
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/galaxy.fade.frag"),
		});
		mainRenderModule->AddShader("sg_background", galaxyFadeShader);
		img_background = (v4d::graphics::CubeMapImage*)mainRenderModule->GetImage("img_background");
	}
	
	V4D_MODULE_FUNC(void, UnloadScene) {
		if (galaxyBackgroundShader) delete galaxyBackgroundShader;
		if (galaxyFadeShader) delete galaxyFadeShader;
	}
	
	V4D_MODULE_FUNC(void, InitWindow, v4d::graphics::Window* w) {
		window = w;
	}
	
	V4D_MODULE_FUNC(void, InitRenderer, v4d::graphics::Renderer* _r) {
		r = _r;
	}
	
	V4D_MODULE_FUNC(void, InitServer, std::shared_ptr<ListeningServer> _srv) {
		server = _srv;
		serverSideObjects = (ServerSideObjects*)mainMultiplayerModule->ModuleGetCustomPtr(CUSTOM_PTR_SERVER_OBJECTS);
	}
	
	V4D_MODULE_FUNC(void, InitClient, std::shared_ptr<OutgoingConnection> _c) {
		client = _c;
		clientSideObjects = (ClientSideObjects*)mainMultiplayerModule->ModuleGetCustomPtr(CUSTOM_PTR_CLIENT_OBJECTS);
	}
	
#pragma endregion
	
#pragma region Input
	
	V4D_MODULE_FUNC(std::string, InputCallbackName) {return THIS_MODULE;}
	
	V4D_MODULE_FUNC(void, InputKeyCallback, int key, int scancode, int action, int mods) {
		if (action != GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& (!ImGui::IsAnyWindowFocused() || key == GLFW_KEY_ESCAPE)
			#endif
		) {
			// LOG(scancode) //TODO build platform-specific mapping for scancode when key == -1
			switch (key) {
				
				// Quit
				case GLFW_KEY_ESCAPE:
					glfwSetWindowShouldClose(window->GetHandle(), 1);
					break;
					
				// // Throw stuff
				// case GLFW_KEY_B:{
				// 	v4d::data::WriteOnlyStream stream(32);
				// 		stream << networking::action::TEST_OBJ;
				// 		stream << std::string("ball");
				// 		stream << DVector3{playerView->viewForward.x, playerView->viewForward.y, playerView->viewForward.z};
				// 	ClientEnqueueAction(stream);
				// }break;
			}
		}
	}
	
	V4D_MODULE_FUNC(void, MouseButtonCallback, int button, int action, int mods) {
		if (action == GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsAnyWindowFocused()
			#endif
		) {
			switch (button) {
				case GLFW_MOUSE_BUTTON_1:
					glfwSetInputMode(window->GetHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
					glfwSetCursorPos(window->GetHandle(), 0, 0);
					break;
				case GLFW_MOUSE_BUTTON_2:
					glfwSetCursorPos(window->GetHandle(), 0, 0);
					glfwSetInputMode(window->GetHandle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
					break;
			}
		}
	}
	
#pragma endregion
	
#pragma region Networking
	
	V4D_MODULE_FUNC(void, ServerSendActions, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		std::scoped_lock lock(serverActionQueueMutex);
		auto& actionQueue = serverActionQueuePerClient[client->id];
		while (actionQueue.size()) {
			// LOG_DEBUG("Server SendActionFromQueue for client " << client->id)
			stream->Begin();
				stream->EmplaceStream(actionQueue.front());
			stream->End();
			actionQueue.pop();
		}
	}
	
	V4D_MODULE_FUNC(void, ClientSendActions, v4d::io::SocketPtr stream) {
		std::lock_guard lock(clientActionQueueMutex);
		while (clientActionQueue.size()) {
			// LOG_DEBUG("Client SendActionFromQueue")
			stream->Begin();
				stream->EmplaceStream(clientActionQueue.front());
			stream->End();
			clientActionQueue.pop();
		}
	}
	
	// V4D_MODULE_FUNC(void, SendStreamCustomGameObjectData, v4d::scene::NetworkGameObjectPtr obj, v4d::data::WriteOnlyStream& stream) {}
	// V4D_MODULE_FUNC(void, ReceiveStreamCustomGameObjectData, v4d::scene::NetworkGameObjectPtr obj, v4d::data::ReadOnlyStream& stream) {}
	
	V4D_MODULE_FUNC(void, ServerIncomingClient, v4d::networking::IncomingClientPtr client) {
		LOG("Server: IncomingClient " << client->id)
		NetworkGameObject::Id playerObjId;
		v4d::scene::NetworkGameObjectPtr obj;
		{
			std::lock_guard lock(serverSideObjects->mutex);
			obj = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::Player);
			obj->physicsClientID = client->id;
			obj->isDynamic = true;
			obj->clientIterations[client->id] = 0;
			serverSideObjects->players[client->id] = obj;
			playerObjId = obj->id;
		}
		
		// // Set player position
		// const glm::dvec3 sun1Position = {-1.496e+11,0, 0};
		// auto worldPosition = glm::dvec3{-493804, -7.27024e+06, 3.33978e+06};
		// auto forwardVector = glm::normalize(sun1Position);
		// auto upVector = glm::normalize(worldPosition);
		// auto rightVector = glm::cross(forwardVector, upVector);
		// worldPosition += rightVector * (double)client->id;
		// obj->SetTransform(worldPosition, forwardVector, upVector);
		
		obj->parent = GetDefaultGalacticPosition().rawValue;
		
		v4d::data::WriteOnlyStream stream(sizeof(networking::action::Action) + sizeof(playerObjId));
			stream << networking::action::ASSIGN_PLAYER_OBJ;
			stream << playerObjId;
		
		ServerEnqueueAction(client->id, stream);
	}
	
	V4D_MODULE_FUNC(void, ServerReceiveAction, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		auto action = stream->Read<networking::action::Action>();
		switch (action) {
			
			case networking::action::TEST_OBJ:{
				
			}break;
		
			default: 
				LOG_ERROR("Server ReceiveAction UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	V4D_MODULE_FUNC(void, ClientReceiveAction, v4d::io::SocketPtr stream) {
		auto action = stream->Read<networking::action::Action>();
		switch (action) {
			case networking::action::ASSIGN_PLAYER_OBJ:{ // assign object to client for camera
				auto id = stream->Read<NetworkGameObject::Id>();
				
				// LOG_DEBUG("Client ReceiveAction ASSIGN_PLAYER_OBJ for obj id " << id)
				try {
					std::lock_guard lock(clientSideObjects->mutex);
					auto obj = clientSideObjects->objects.at(id);
					if (auto entity = obj->renderableGeometryEntityInstance.lock(); entity) {
						scene->cameraParent = entity;
					}
					// scene->camera.worldPosition = ...
					// playerView->SetInitialViewDirection(forwardVector, upVector, true);
				} catch (std::exception& err) {
					LOG_ERROR("Client ReceiveAction ASSIGN_PLAYER_OBJ ("<<id<<") : " << err.what())
				}
			}break;
			default: 
				LOG_ERROR("Client ReceiveAction UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	
	
#pragma endregion

#pragma region GameObjects
	
	// V4D_MODULE_FUNC(void, CreateGameObject, v4d::scene::NetworkGameObjectPtr obj) {
	// 	switch (obj->type) {
	// 		case OBJECT_TYPE::Player:{
	// 		}break;
	// 	}
	// }
	
	V4D_MODULE_FUNC(void, AddGameObjectToScene, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		switch (obj->type) {
			case OBJECT_TYPE::Player:{
				auto entity = v4d::graphics::RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				// entity->generator = [](auto* entity, auto* device){};
				// entity->generator = cake;
			}break;
			// case OBJECT_TYPE::Ball:{
			// 	auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
			// 	obj->renderableGeometryEntityInstance = entity;
			// 	float radius = 0.5f;
			// 	auto physics = entity->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 1.0f);
			// 	physics->SetSphereCollider(radius);
			// 	physics->friction = 0.6;
			// 	physics->bounciness = 0.7;
			// 	entity->generator = [radius](RenderableGeometryEntity* entity, Device* device){
			// 		RenderableGeometryEntity::Material mat {};
			// 		mat.visibility.textures[0] = Renderer::sbtOffsets["call:tex_checker"];
			// 		mat.visibility.texFactors[0] = 255;
			// 		mat.visibility.roughness = 0;
			// 		mat.visibility.metallic = 1;
			// 		entity->Allocate(device, "V4D_raytracing:aabb_sphere")->material = mat;
			// 		entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
			// 		entity->Add_meshVertexColorU8()->AllocateBuffers(device, {127,127,127,255});
			// 	};
			// }break;
		}
	}
	
#pragma endregion
	
#pragma region Rendering

	V4D_MODULE_FUNC(void, ConfigureShaders) {
		mainRenderModule->GetPipelineLayout("pl_background")->AddPushConstant<GalaxyPushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		
		galaxyBackgroundShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		galaxyBackgroundShader->rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		galaxyBackgroundShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		galaxyBackgroundShader->depthStencilState.depthTestEnable = VK_FALSE;
		galaxyBackgroundShader->depthStencilState.depthWriteEnable = VK_FALSE;
		galaxyBackgroundShader->AddVertexInputBinding(sizeof(StarVertex), VK_VERTEX_INPUT_RATE_VERTEX, StarVertex::GetInputAttributes());
		
		galaxyFadeShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		galaxyFadeShader->rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		galaxyFadeShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		galaxyFadeShader->depthStencilState.depthTestEnable = VK_FALSE;
		galaxyFadeShader->depthStencilState.depthWriteEnable = VK_FALSE;
		galaxyFadeShader->SetData(6);
	}

	V4D_MODULE_FUNC(void, CreateVulkanPipelines) {
		const int nbColorAttachments = 1;
		VkAttachmentDescription attachment {};
		VkAttachmentReference colorAttachmentRef {};
		
		// Color attachment
		attachment.format = img_background->format;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = GALAXY_FADE? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
		attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		colorAttachmentRef = {
			galaxyBackgroundRenderPass.AddAttachment(attachment),
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};
	
		VkSubpassDescription subpass {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
		galaxyBackgroundRenderPass.AddSubpass(subpass);
		
		// Create the render pass
		galaxyBackgroundRenderPass.Create(r->renderingDevice);
		galaxyBackgroundRenderPass.CreateFrameBuffers(r->renderingDevice, *img_background);
		
		// Stars shader
		galaxyBackgroundShader->SetRenderPass(img_background, galaxyBackgroundRenderPass.handle, 0);
		galaxyBackgroundShader->AddColorBlendAttachmentState(
			VK_TRUE
			/*srcColorBlendFactor*/		,VK_BLEND_FACTOR_ONE
			/*dstColorBlendFactor*/		,VK_BLEND_FACTOR_ONE
			/*colorBlendOp*/			,VK_BLEND_OP_MAX
			/*srcAlphaBlendFactor*/		,VK_BLEND_FACTOR_ONE
			/*dstAlphaBlendFactor*/		,VK_BLEND_FACTOR_ONE
			/*alphaBlendOp*/			,VK_BLEND_OP_MAX
		);
		galaxyBackgroundShader->CreatePipeline(r->renderingDevice);
		
		// Fade shader
		galaxyFadeShader->SetRenderPass(img_background, galaxyBackgroundRenderPass.handle, 0);
		galaxyFadeShader->AddColorBlendAttachmentState(
			VK_TRUE
			/*srcColorBlendFactor*/		,VK_BLEND_FACTOR_ONE
			/*dstColorBlendFactor*/		,VK_BLEND_FACTOR_ONE
			/*colorBlendOp*/			,VK_BLEND_OP_REVERSE_SUBTRACT
			/*srcAlphaBlendFactor*/		,VK_BLEND_FACTOR_ZERO
			/*dstAlphaBlendFactor*/		,VK_BLEND_FACTOR_ZERO
			/*alphaBlendOp*/			,VK_BLEND_OP_MIN
		);
		galaxyFadeShader->CreatePipeline(r->renderingDevice);
	}
	
	V4D_MODULE_FUNC(void, DestroyVulkanPipelines) {
		galaxyBackgroundShader->DestroyPipeline(r->renderingDevice);
		galaxyFadeShader->DestroyPipeline(r->renderingDevice);
		galaxyBackgroundRenderPass.DestroyFrameBuffers(r->renderingDevice);
		galaxyBackgroundRenderPass.Destroy(r->renderingDevice);
	}
	
	V4D_MODULE_FUNC(void, DestroyVulkanResources) {
		for (auto&[pos, chunk] : galaxyChunks) {
			chunk.Free();
		}
		galaxyChunks.clear();
		
		for (auto&[id, planet] : planetsDebug) {
			planet.Free();
		}
		planetsDebug.clear();
	}
	
	V4D_MODULE_FUNC(void, RenderFrame_BeforeUpdate) {
		if (auto cameraParent = scene->cameraParent.lock(); cameraParent) {
			if (auto cameraParentRoot = v4d::graphics::RenderableGeometryEntity::GetRoot(cameraParent); cameraParentRoot) {
				GalacticPosition galacticPosition {cameraParentRoot->parentId};
				
				if (galacticPosition.IsValid()) {
					double playerPositionTimestamp = scene->timestamp;
					if (galacticPosition.IsCelestial()) {
						scene->camera.originOffset = GalaxyGenerator::GetCelestial(galacticPosition)->GetAbsolutePositionInOrbit<glm::i64vec3>(playerPositionTimestamp) + glm::i64vec3(LY2M(GalaxyGenerator::GetStarSystem(galacticPosition)->GetOffsetLY()));
					} else {
						// Origin reset
						glm::i64vec3 originOffsetLY = glm::round(M2LY(glm::dvec3(scene->camera.originOffset)));
						if (originOffsetLY.x != 0 || originOffsetLY.y != 0 || originOffsetLY.z != 0) {
							galacticPosition.posInGalaxy_x = uint64_t(int64_t(galacticPosition.posInGalaxy_x) + originOffsetLY.x);
							galacticPosition.posInGalaxy_y = uint64_t(int64_t(galacticPosition.posInGalaxy_y) + originOffsetLY.y);
							galacticPosition.posInGalaxy_z = uint64_t(int64_t(galacticPosition.posInGalaxy_z) + originOffsetLY.z);
							scene->camera.originOffset.x -= LY2M_INT(originOffsetLY.x);
							scene->camera.originOffset.y -= LY2M_INT(originOffsetLY.y);
							scene->camera.originOffset.z -= LY2M_INT(originOffsetLY.z);
							cameraParentRoot->parentId = galacticPosition.rawValue;
							LOG(galacticPosition.posInGalaxy_x << " " << galacticPosition.posInGalaxy_y << " " << galacticPosition.posInGalaxy_z)
						}
					}
					glm::dvec3 playerPositionInStarSystem = glm::dvec3(scene->camera.originOffset) + scene->camera.worldPosition;
					glm::dvec3 playerPositionInGalaxyLY = glm::dvec3(galacticPosition.posInGalaxy_x, galacticPosition.posInGalaxy_y, galacticPosition.posInGalaxy_z) + M2LY(playerPositionInStarSystem);
					currentGalaxySnapshot.Set(galacticPosition, playerPositionInStarSystem, playerPositionInGalaxyLY, playerPositionTimestamp);
				} else {
					LOG_ERROR("Galactic Position is INVALID")
				}
			}
		}
	}
	
	V4D_MODULE_FUNC(void, BeginSecondaryFrameUpdate) {
		
		// Location and Moment
		GalaxySnapshot galaxySnapshot(currentGalaxySnapshot);
		
		if (galaxySnapshot.galacticPosition.IsValid()) {
			{// Stars
				glm::ivec3 playerPos = glm::round(galaxySnapshot.positionInGalaxyLY / STAR_CHUNK_SIZE);
				for (int x = -STAR_CHUNK_GEN_OFFSET; x <= STAR_CHUNK_GEN_OFFSET; ++x) {
					for (int y = -STAR_CHUNK_GEN_OFFSET; y <= STAR_CHUNK_GEN_OFFSET; ++y) {
						for (int z = -STAR_CHUNK_GEN_OFFSET; z <= STAR_CHUNK_GEN_OFFSET; ++z) {
							const auto pos = playerPos + glm::ivec3{x,y,z};
							if (pos.x >= 0 && pos.y >= 0 && pos.z >= 0 && glm::distance(glm::dvec3(pos)*STAR_CHUNK_SIZE, galaxySnapshot.positionInGalaxyLY) < STAR_MAX_VISIBLE_DISTANCE*2 && galaxyChunks.count(pos) == 0) {
								galaxyChunks[pos].GenerateStars(pos*glm::ivec3(STAR_CHUNK_SIZE));
								goto Continue;
							}
						}
					}
				}
			Continue:
				for (auto it = galaxyChunks.begin(); it != galaxyChunks.end();) {
					auto&[pos, chunk] = *it;
					if (glm::distance(glm::dvec3(pos)*STAR_CHUNK_SIZE, galaxySnapshot.positionInGalaxyLY) > STAR_MAX_VISIBLE_DISTANCE*2) {
						chunk.Free();
						it = galaxyChunks.erase(it);
					} else {
						chunk.Update(glm::dvec3(pos)*STAR_CHUNK_SIZE - glm::dvec3(galaxySnapshot.positionInGalaxyLY));
						++it;
					}
				}
			}
			{// Celestials
				if (GalaxyGenerator::GetStarSystemPresence(galaxySnapshot.galacticPosition)) {
					const auto& starSystem = GalaxyGenerator::GetStarSystem(galaxySnapshot.galacticPosition);
					glm::dvec3 offset = starSystem->GetOffsetLY() - M2LY(galaxySnapshot.positionInStarSystem);
					for (auto& level1 : starSystem->GetCentralCelestialBodies()) if (level1) {
						glm::dvec3 level1PositionInOrbit = offset + M2LY(level1->GetPositionInOrbit(galaxySnapshot.timestamp));
						planetsDebug[level1->galacticPosition.rawValue].Update(level1PositionInOrbit, level1.get());
						for (auto& level2 : level1->GetChildren()) {
							glm::dvec3 level2PositionInOrbit = level1PositionInOrbit + M2LY(level2->GetPositionInOrbit(galaxySnapshot.timestamp));
							planetsDebug[level2->galacticPosition.rawValue].Update(level2PositionInOrbit, level2.get());
							for (auto& level3 : level2->GetChildren()) {
								glm::dvec3 level3PositionInOrbit = level2PositionInOrbit + M2LY(level3->GetPositionInOrbit(galaxySnapshot.timestamp));
								planetsDebug[level3->galacticPosition.rawValue].Update(level3PositionInOrbit, level3.get());
							}
						}
					}
				}
				for (auto it = planetsDebug.begin(); it != planetsDebug.end();) {
					auto&[pos, planet] = *it;
					if (GalacticPosition(pos).posInGalaxy != galaxySnapshot.galacticPosition.posInGalaxy) {
						planet.Free();
						it = planetsDebug.erase(it);
					} else {
						++it;
					}
				}
			}
		}
		
	}

	V4D_MODULE_FUNC(void, SecondaryRenderUpdate2, VkCommandBuffer commandBuffer) {
		for (auto&[pos, chunk] : galaxyChunks) {
			chunk.Push(commandBuffer);
		}
		
		{// Wait for BLAS to finish before building TLAS
			VkMemoryBarrier memoryBarrier {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				nullptr,// pNext
				VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT,// VkAccessFlags srcAccessMask
				VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,// VkAccessFlags dstAccessMask
			};
			r->renderingDevice->CmdPipelineBarrier(
				commandBuffer, 
				VK_PIPELINE_STAGE_TRANSFER_BIT, 
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 
				0, 
				1, &memoryBarrier, 
				0, 0, 
				0, 0
			);
		}
		
		galaxyBackgroundRenderPass.Begin(r->renderingDevice, commandBuffer, *img_background, {{.0,.0,.0,.0}});
			if (GALAXY_FADE) {
				GalaxyPushConstant pushConstant {};
				pushConstant.screenSize = img_background->width;
				pushConstant.brightnessFactor = 1.0;
				galaxyFadeShader->Execute(r->renderingDevice, commandBuffer, 1, &pushConstant);
			}
			for (auto&[pos, chunk] : galaxyChunks) {
				chunk.Draw(commandBuffer);
			}
			for (auto&[id, planet] : planetsDebug) {
				planet.Draw(commandBuffer);
			}
		galaxyBackgroundRenderPass.End(r->renderingDevice, commandBuffer);
	}
	
#pragma endregion
	
};
