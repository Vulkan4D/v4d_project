#include <v4d.h>
#include <V4D_Mod.h>
#include "v4d/game/Entity.h"

#include "utilities/io/Logger.h"
#include "utilities/graphics/vulkan/RenderPass.h"
#include "utilities/graphics/vulkan/StagingBuffer.hpp"

#include "GalaxyGenerator.h"
#include "Celestial.h"
#include "StarSystem.h"

#include "celestials/Planet.h"
#include "TerrainGeneratorLib.h"


#include "utilities/scene/GltfModelLoader.h"
v4d::scene::GltfModelLoader cake {V4D_MODULE_ASSET_PATH("V4D_galaxy4d", "resources/cake.glb")};


GalacticPosition GetDefaultGalacticPosition() {
	GalacticPosition defaultGalacticPosition;
	// defaultGalacticPosition.SetReferenceFrame(135983499432086); // very large trinary system with over 300 bodies
	defaultGalacticPosition = 2950733266538646; // earth-like planet
	// defaultGalacticPosition.SetReferenceFrame(139712601895775); // 8 stars
	// defaultGalacticPosition.SetReferenceFrame(134436245599421); // very small system
	return defaultGalacticPosition;
}

glm::dvec4 GetDefaultWorldPosition() {
	return {/*PositionOnPlanet*/0.4,-0.8,1, /*altitude*/5};
}


#include "../V4D_flycam/common.hh"
PlayerView* playerView = nullptr;
double playerAccelerateUp = 0;
double playerAccelerateDown = 0;
double playerAccelerateRight = 0;
double playerAccelerateLeft = 0;
double playerAccelerateForward = 0;
double playerAccelerateBackward = 0;
Entity::Id clientSidePlayerEntityID = -1;


#pragma region Global Pointers

	V4D_Mod* mainRenderModule = nullptr;
	V4D_Mod* mainMultiplayerModule = nullptr;
	v4d::graphics::Window* window = nullptr;
	v4d::graphics::Renderer* r = nullptr;
	v4d::scene::Scene* scene = nullptr;
	std::shared_ptr<v4d::networking::ListeningServer> server = nullptr;
	std::shared_ptr<v4d::networking::OutgoingConnection> client = nullptr;

#pragma endregion

#pragma region Networking

	namespace networking::action {
		typedef uint8_t Action;
		// const Action EXTENDED_ACTION = 0;

		// Action stream Only (TCP)
		const Action ASSIGN_PLAYER_OBJ = 0;
		
		// Burst streams only (TCP & UDP)
		const Action SYNC_PLAYER_MOTION = 1;

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
		glm::i64vec3 positionInStarSystem;
		glm::dvec3 positionInGalaxyLY;
		glm::dvec3 cameraPosition;
		double timestamp;
		
		mutable std::mutex mu;
		std::unique_lock<std::mutex> Lock() const {return std::unique_lock<std::mutex>(mu);}
		void Set(const GalacticPosition& galacticPosition, const glm::i64vec3& positionInStarSystem, const glm::dvec3& positionInGalaxyLY, const glm::dvec3& cameraPosition, const double& timestamp) {
			std::lock_guard lock(mu);
			this->galacticPosition = galacticPosition;
			this->positionInStarSystem = positionInStarSystem;
			this->positionInGalaxyLY = positionInGalaxyLY;
			this->cameraPosition = cameraPosition;
			this->timestamp = timestamp;
		}
		GalaxySnapshot() = default;
		GalaxySnapshot(const GalaxySnapshot& snapshot) {
			std::scoped_lock lock(mu, snapshot.mu);
			this->galacticPosition = snapshot.galacticPosition;
			this->positionInStarSystem = snapshot.positionInStarSystem;
			this->positionInGalaxyLY = snapshot.positionInGalaxyLY;
			this->cameraPosition = snapshot.cameraPosition;
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
		float minViewDistance = 0.5;
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
			pushConstant.brightnessFactor = 0.002;
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
									{x+offset.x, y+offset.y, z+offset.z, 1.0},
									color
								};
							}
						}
					}
				}
			}
		}
		void DrawDotInCubemap(VkCommandBuffer commandBuffer) {
			galaxyBackgroundShader->SetData(buffer.GetDeviceLocalBuffer(), count);
			galaxyBackgroundShader->Execute(r->renderingDevice, commandBuffer, 1, &pushConstant);
		}
	};
	struct RenderableCelestial {
		// For cubemap dot
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
		void Update(glm::dvec3 position, glm::dvec3 cameraPosition, Celestial* celestial) {
			
			glm::dvec3 worldPosition = glm::dvec3(position);
			
			pushConstant.brightnessFactor = 1.0;
			pushConstant.minViewDistance = 0;
			pushConstant.maxViewDistance = 0;
			pushConstant.relativePosition = glm::vec4(position - cameraPosition, 2.0);
			(*this)->position = glm::dvec4(0);
			(*this)->color = glm::vec4(0);
			
			double sizeInScreen = scene->camera.GetFixedBoundingSizeInScreen(position - celestial->GetRadius(), position + celestial->GetRadius());
			if (sizeInScreen > 0.001) {
				celestial->RenderUpdate(position, cameraPosition, sizeInScreen);
			} else {
				celestial->RenderDelete();
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
		}
		void DrawDotInCubemap(VkCommandBuffer commandBuffer) {
			galaxyBackgroundShader->SetData(buffer.buffer, 1);
			galaxyBackgroundShader->Execute(r->renderingDevice, commandBuffer, 1, &pushConstant);
		}
	};
	std::unordered_map<uint64_t, RenderableCelestial> renderableCelestials {};
	std::unordered_map<glm::ivec3, GalaxyChunk> galaxyChunks {};

#pragma endregion


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
		playerView->canMovePosition = false;
		
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
	
	V4D_MODULE_FUNC(void, InitServer, std::shared_ptr<v4d::networking::ListeningServer> _srv) {
		server = _srv;
	}
	
	V4D_MODULE_FUNC(void, InitClient, std::shared_ptr<v4d::networking::OutgoingConnection> _c) {
		client = _c;
	}
	
	V4D_MODULE_FUNC(void, CreateVulkanResources2, Device* device) {
		// Chunk Generator
		PlanetTerrain::renderingDevice = r->renderingDevice;
		TerrainGeneratorLib::Start();
		PlanetTerrain::StartChunkGenerator();
	}
	
	V4D_MODULE_FUNC(void, DestroyVulkanResources2, Device* device) {
		// Chunk Generator
		PlanetTerrain::EndChunkGenerator();
		TerrainGeneratorLib::Stop();
		for (auto&[id,terrain] : PlanetTerrain::terrains) if (terrain) {
			terrain->RemoveBaseChunks();
			terrain = nullptr;
		}
		PlanetTerrain::terrains.clear();
		PlanetTerrain::renderingDevice = nullptr;
		GalaxyGenerator::ClearCache();
	}
	
#pragma endregion
	
#pragma region Input
	
	V4D_MODULE_FUNC(std::string, InputCallbackName) {return THIS_MODULE;}
	
	V4D_MODULE_FUNC(void, InputKeyCallback, int key, int scancode, int action, int mods) {
		if (action != GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) || key == GLFW_KEY_ESCAPE)
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
				&& !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)
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
	
	V4D_MODULE_FUNC(void, ServerSendActions, v4d::io::SocketPtr stream, v4d::networking::IncomingClientPtr client) {
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
	
	// V4D_MODULE_FUNC(void, StreamSendEntityData, int64_t entityUniqueID, uint64_t type, v4d::data::WriteOnlyStream& stream) {}
	// V4D_MODULE_FUNC(void, StreamReceiveEntityData, int64_t entityUniqueID, uint64_t type, v4d::data::ReadOnlyStream& stream) {}
	
	V4D_MODULE_FUNC(void, ServerIncomingClient, v4d::networking::IncomingClientPtr client) {
		GalacticPosition defaultPosition = GetDefaultGalacticPosition();
		glm::dvec3 worldPosition = GetDefaultWorldPosition();
		
		LOG("Server: IncomingClient " << client->id)
		Entity::Id playerEntityId;
		ServerSidePlayer::Ptr player = ServerSidePlayer::Create(client->id);
		ServerSideEntity::Ptr entity = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::Player, defaultPosition.rawValue);
		playerEntityId = entity->GetID();
		entity->isDynamic = true;
		entity->clientIterations[client->id] = 0;
		player->parentEntityId = playerEntityId;
		entity->Add_rigidbody(10.0)->boundingRadius = 0.4;
		entity->colliders.emplace_back(0.4);
		
		// Set player position
		if (defaultPosition.IsCelestial()) {
			auto celestial = GalaxyGenerator::GetCelestial(defaultPosition);
			Planet* planet = dynamic_cast<Planet*>(celestial.get());
			if (planet) {
				worldPosition = glm::normalize(worldPosition) * (planet->GetPlanetTerrain()->GetHeightMap(glm::normalize(worldPosition)) + GetDefaultWorldPosition().w);
			}
		}
		auto forwardVector = glm::dvec3(0,1,0);
		auto upVector = glm::normalize(worldPosition);
		auto rightVector = glm::cross(forwardVector, upVector);
		worldPosition += rightVector * (double)client->id;
		entity->position = worldPosition;
		
		entity->Activate();
		
		v4d::data::WriteOnlyStream stream(sizeof(networking::action::Action) + sizeof(playerEntityId));
			stream << networking::action::ASSIGN_PLAYER_OBJ;
			stream << playerEntityId;
			stream << v4d::Timer::GetCurrentTimestamp();
			stream << worldPosition;
			stream << forwardVector;
			stream << upVector;
			stream << defaultPosition.IsCelestial();
		
		ServerEnqueueAction(client->id, stream);
	}
	
	V4D_MODULE_FUNC(void, ServerReceiveAction, v4d::io::SocketPtr stream, v4d::networking::IncomingClientPtr client) {
		auto action = stream->Read<networking::action::Action>();
		switch (action) {
			
			default: 
				LOG_ERROR("Server ReceiveAction UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	V4D_MODULE_FUNC(void, ClientReceiveAction, v4d::io::SocketPtr stream) {
		auto action = stream->Read<networking::action::Action>();
		switch (action) {
			case networking::action::ASSIGN_PLAYER_OBJ:{ // assign object to client for camera
				auto id = stream->Read<Entity::Id>();
				double timestamp = stream->Read<double>();
				auto worldPosition = stream->Read<glm::dvec3>();
				auto forwardVector = stream->Read<glm::dvec3>();
				auto upVector = stream->Read<glm::dvec3>();
				bool isReferenceCelestial = stream->Read<bool>();
				
				// LOG_DEBUG("Client ReceiveAction ASSIGN_PLAYER_OBJ for entity id " << id)
				if (ClientSideEntity::Ptr entity = ClientSideEntity::Get(id); entity) {
					if (v4d::graphics::RenderableGeometryEntity::Ptr renderableEntity = entity->renderableGeometryEntityInstance.lock(); renderableEntity) {
						scene->cameraParent = renderableEntity;
						scene->timestamp = timestamp;
					}
					clientSidePlayerEntityID = id;
					scene->camera.worldPosition = worldPosition;
					playerView->camSpeed = isReferenceCelestial? 10.0 : LY2M(0.1);
					playerView->SetInitialViewDirection(forwardVector, upVector, true);
				}
			}break;
			default: 
				LOG_ERROR("Client ReceiveAction UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	V4D_MODULE_FUNC(void, ServerReceiveBurst, v4d::io::SocketPtr stream, v4d::networking::IncomingClientPtr client) {
		using namespace networking::action;
		auto action = stream->Read<Action>();
		switch (action) {
			case SYNC_PLAYER_MOTION:{
				auto id = stream->Read<Entity::Id>();
				auto orientation = stream->Read<Entity::Orientation>();
				auto acceleration = stream->Read<typeof(playerView->velocity)>();
				if (ServerSidePlayer::Ptr player = ServerSidePlayer::Get(client->id); player && player->parentEntityId == id) {
					if (ServerSideEntity::Ptr entity = ServerSideEntity::Get(id); entity) {
						entity->orientation = orientation;
						if (auto rigidbody = entity->rigidbody.Lock(); rigidbody) {
							rigidbody->linearAcceleration = acceleration;
							rigidbody->angularVelocity = {0,0,0};
						}
					}
				}
			}break;
			
			default: 
				LOG_ERROR("Server ReceiveBurst UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	V4D_MODULE_FUNC(void, ClientSendBursts, v4d::io::SocketPtr stream) {
		using namespace networking::action;
		if (clientSidePlayerEntityID != -1) {
			if (ClientSideEntity::Ptr playerEntity = ClientSideEntity::Get(clientSidePlayerEntityID); playerEntity) {
				if (auto renderableEntity = playerEntity->renderableGeometryEntityInstance.lock(); renderableEntity) {
					// Look Direction and Acceleration
					stream->Begin();
						*stream << SYNC_PLAYER_MOTION;
						*stream << clientSidePlayerEntityID;
						*stream << (Entity::Orientation)glm::quat_cast(renderableEntity->GetWorldTransform());
						*stream << playerView->velocity; // this is the acceleration in this case
					stream->End();
				}
			}
		}
	}
	
	
#pragma endregion

#pragma region Entities
	
	V4D_MODULE_FUNC(void, CreateEntity, int64_t entityUniqueID, uint64_t type) {
		switch (type) {
			case OBJECT_TYPE::Player:{
				if (ClientSideEntity::Ptr entity = ClientSideEntity::Get(entityUniqueID); entity) {
					auto renderableEntity = v4d::graphics::RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
					entity->renderableGeometryEntityInstance = renderableEntity;
					renderableEntity->generator = [](auto* renderableEntity, auto* device){};
					renderableEntity->generator = cake;
				}
			}break;
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
		
		// PlanetRenderer
		mainRenderModule->AddRayTracingHitShader(THIS_MODULE, "planet.terrain");
		mainRenderModule->AddRayTracingHitShader(THIS_MODULE, "atmosphere");
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
		
		for (auto&[id, planet] : renderableCelestials) {
			planet.Free();
		}
		renderableCelestials.clear();
	}
	
	V4D_MODULE_FUNC(void, RenderFrame_BeforeUpdate) {
		if (auto cameraParent = scene->cameraParent.lock(); cameraParent) {
			GalacticPosition galacticPosition {cameraParent->parentId};
			
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
						cameraParent->parentId = galacticPosition.rawValue;
						LOG(galacticPosition.posInGalaxy_x << " " << galacticPosition.posInGalaxy_y << " " << galacticPosition.posInGalaxy_z)
					}
				}
				glm::i64vec3 playerPositionInStarSystem = scene->camera.originOffset;
				glm::dvec3 cameraPosition = scene->camera.worldPosition;
				glm::dvec3 playerPositionInGalaxyLY = glm::dvec3(galacticPosition.posInGalaxy_x, galacticPosition.posInGalaxy_y, galacticPosition.posInGalaxy_z) + M2LY(glm::dvec3(playerPositionInStarSystem) + cameraPosition);
				currentGalaxySnapshot.Set(galacticPosition, playerPositionInStarSystem, playerPositionInGalaxyLY, cameraPosition, playerPositionTimestamp);
			} else {
				LOG_ERROR("Galactic Position is INVALID")
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
					glm::i64vec3 offset = glm::i64vec3(LY2M(starSystem->GetOffsetLY())) - galaxySnapshot.positionInStarSystem;
					for (auto& level1 : starSystem->GetCentralCelestialBodies()) if (level1) {
						glm::i64vec3 level1PositionInOrbit = offset + glm::i64vec3(level1->GetPositionInOrbit(galaxySnapshot.timestamp));
						renderableCelestials[level1->galacticPosition.rawValue].Update(level1PositionInOrbit, galaxySnapshot.cameraPosition, level1.get());
						for (auto& level2 : level1->GetChildren()) {
							glm::i64vec3 level2PositionInOrbit = level1PositionInOrbit + glm::i64vec3(level2->GetPositionInOrbit(galaxySnapshot.timestamp));
							renderableCelestials[level2->galacticPosition.rawValue].Update(level2PositionInOrbit, galaxySnapshot.cameraPosition, level2.get());
							for (auto& level3 : level2->GetChildren()) {
								glm::i64vec3 level3PositionInOrbit = level2PositionInOrbit + glm::i64vec3(level3->GetPositionInOrbit(galaxySnapshot.timestamp));
								renderableCelestials[level3->galacticPosition.rawValue].Update(level3PositionInOrbit, galaxySnapshot.cameraPosition, level3.get());
							}
						}
					}
				}
				for (auto it = renderableCelestials.begin(); it != renderableCelestials.end();) {
					auto&[pos, planet] = *it;
					if (GalacticPosition(pos).posInGalaxy != galaxySnapshot.galacticPosition.posInGalaxy) {
						planet.Free();
						it = renderableCelestials.erase(it);
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
				chunk.DrawDotInCubemap(commandBuffer);
			}
			for (auto&[id, planet] : renderableCelestials) {
				planet.DrawDotInCubemap(commandBuffer);
			}
		galaxyBackgroundRenderPass.End(r->renderingDevice, commandBuffer);
	}
	
#pragma endregion
	
};
