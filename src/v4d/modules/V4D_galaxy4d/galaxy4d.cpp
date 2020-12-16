#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"
#include "actions.hh"
#include "../V4D_flycam/common.hh"
#include "../V4D_raytracing/camera_options.hh"
#include "../V4D_multiplayer/ServerSideObjects.hh"
#include "../V4D_multiplayer/ClientSideObjects.hh"

ObjModelLoader droneModel {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/cake.obj"), V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources")};

using namespace v4d::scene;
using namespace v4d::networking;
using namespace networking::action;
using namespace v4d::modular;
using namespace v4d::graphics;
using namespace v4d::graphics::Mesh;

V4D_Mod* mainRenderModule = nullptr;
V4D_Mod* mainMultiplayerModule = nullptr;
std::shared_ptr<ListeningServer> server = nullptr;
std::shared_ptr<OutgoingConnection> client = nullptr;

std::recursive_mutex serverActionQueueMutex;
std::unordered_map<uint64_t /* clientID */, std::queue<v4d::data::Stream>> serverActionQueuePerClient {};
ServerSideObjects* serverSideObjects = nullptr;

std::recursive_mutex clientActionQueueMutex;
std::queue<v4d::data::Stream> clientActionQueue {};
ClientSideObjects* clientSideObjects = nullptr;
PlayerView* playerView = nullptr;

Scene* scene = nullptr;
Renderer* r = nullptr;

bool playerVisible = true;

void ClientEnqueueAction(v4d::data::WriteOnlyStream& stream) {
	std::lock_guard lock(clientActionQueueMutex);
	clientActionQueue.emplace(stream);
}

V4D_MODULE_CLASS(V4D_Mod) {
	
	V4D_MODULE_FUNC(void, ModuleLoad) {
		// Load Dependencies
		playerView = (PlayerView*)V4D_Mod::LoadModule("V4D_flycam")->ModuleGetCustomPtr(0);
		mainRenderModule = V4D_Mod::LoadModule(APP_MAIN_RENDER_MODULE);
		mainMultiplayerModule = V4D_Mod::LoadModule(APP_MAIN_MULTIPLAYER_MODULE);
	}
	
	V4D_MODULE_FUNC(void, InitRenderer, Renderer* _r) {
		r = _r;
	}
	
	#pragma region Server
	
	V4D_MODULE_FUNC(void, InitServer, std::shared_ptr<ListeningServer> _srv) {
		server = _srv;
		serverSideObjects = (ServerSideObjects*)mainMultiplayerModule->ModuleGetCustomPtr(CUSTOM_PTR_SERVER_OBJECTS);
	}
	
	V4D_MODULE_FUNC(void, InitClient, std::shared_ptr<OutgoingConnection> _c) {
		client = _c;
		clientSideObjects = (ClientSideObjects*)mainMultiplayerModule->ModuleGetCustomPtr(CUSTOM_PTR_CLIENT_OBJECTS);
	}
	
	V4D_MODULE_FUNC(void, LoadScene, Scene* _s) {
		scene = _s;
		
		droneModel.Load();
	}
	
	V4D_MODULE_FUNC(void, ServerIncomingClient, IncomingClientPtr client) {
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
		
		// Set player position
		const glm::dvec3 sun1Position = {-1.496e+11,0, 0};
		auto worldPosition = glm::dvec3{-493804, -7.27024e+06, 3.33978e+06};
		auto forwardVector = glm::normalize(sun1Position);
		auto upVector = glm::normalize(worldPosition);
		auto rightVector = glm::cross(forwardVector, upVector);
		worldPosition += rightVector * (double)client->id;
		obj->SetTransform(worldPosition, forwardVector, upVector);
		
		v4d::data::WriteOnlyStream stream(sizeof(ASSIGN_PLAYER_OBJ) + sizeof(playerObjId));
			stream << ASSIGN_PLAYER_OBJ;
			stream << playerObjId;
			stream << worldPosition;
			stream << forwardVector;
			stream << upVector;
		
		std::lock_guard lock(serverActionQueueMutex);
		serverActionQueuePerClient[client->id].emplace(stream);
	}
	
	V4D_MODULE_FUNC(void, ServerReceiveAction, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		auto action = stream->Read<Action>();
		switch (action) {
			
			case TEST_OBJ:{
				auto key = stream->Read<std::string>();
				NetworkGameObjectPtr playerObj = serverSideObjects->players.at(client->id);
				
				if (key == "ball") {
					auto dir = stream->Read<DVector3>();
					std::lock_guard lock(serverSideObjects->mutex);
					// Launch ball
					auto ball = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::Ball);
					ball->SetTransform(glm::translate(glm::dmat4(1), glm::dvec3{dir.x, dir.y, dir.z} * 5.0) * playerObj->GetTransform());
					ball->SetVelocity(glm::dvec3{dir.x, dir.y, dir.z}*40.0);
					ball->isDynamic = true;
					ball->physicsClientID = client->id;
				}
				else if (key == "balls") {
					std::lock_guard lock(serverSideObjects->mutex);
					auto dir = stream->Read<DVector3>();
					// Launch 10 balls
					for (int i = 0; i < 10; ++i) {
						auto ball = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::Ball);
						ball->SetTransform(glm::translate(glm::dmat4(1), glm::dvec3{dir.x, dir.y, dir.z} * 5.0) * playerObj->GetTransform());
						ball->SetVelocity(glm::dvec3{dir.x, dir.y, dir.z}*100.0);
						ball->isDynamic = true;
						ball->physicsClientID = client->id;
					}
				}
				else if (key == "light") {
					auto dir = stream->Read<DVector3>();
					std::lock_guard lock(serverSideObjects->mutex);
					// Launch light
					auto ball = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::Light);
					ball->SetTransform(glm::translate(glm::dmat4(1), glm::dvec3{dir.x, dir.y, dir.z} * 5.0) * playerObj->GetTransform());
					ball->SetVelocity(glm::dvec3{dir.x, dir.y, dir.z}*40.0);
					ball->isDynamic = true;
					ball->physicsClientID = client->id;
				}
				else if (key == "drone") {
					auto dir = stream->Read<DVector3>();
					std::lock_guard lock(serverSideObjects->mutex);
					// Launch drone
					auto drone = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::Drone);
					drone->SetTransform(glm::translate(glm::dmat4(1), glm::dvec3{dir.x, dir.y, dir.z} * 5.0) * playerObj->GetTransform());
					drone->isDynamic = false;
					drone->physicsClientID = client->id;
				}
				else if (key == "glass") {
					auto dir = stream->Read<DVector3>();
					std::lock_guard lock(serverSideObjects->mutex);
					auto drone = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::Glass);
					drone->SetTransform(glm::translate(glm::dmat4(1), glm::dvec3{dir.x, dir.y, dir.z} * 5.0) * playerObj->GetTransform());
					drone->isDynamic = false;
					drone->physicsClientID = client->id;
				}
				else if (key == "clear") {
					std::lock_guard lock(serverSideObjects->mutex);
					for (auto& [objID, obj] : serverSideObjects->objects) if (obj->physicsClientID == client->id && obj->id != playerObj->id) {
						obj->active = false;
					}
				}
			}break;
		
			default: 
				LOG_ERROR("Server ReceiveAction UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	V4D_MODULE_FUNC(void, ServerSendActions, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		{std::scoped_lock lock(serverSideObjects->mutex, serverActionQueueMutex);
			auto& actionQueue = serverActionQueuePerClient[client->id];
			while (actionQueue.size()) {
				// LOG_DEBUG("Server SendActionFromQueue for client " << client->id)
				stream->Begin();
					stream->EmplaceStream(actionQueue.front());
				stream->End();
				actionQueue.pop();
			}
		}
	}
	
	#pragma endregion
	
	#pragma region Client
	
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
	
	V4D_MODULE_FUNC(void, ClientReceiveAction, v4d::io::SocketPtr stream) {
		auto action = stream->Read<Action>();
		switch (action) {
			case ASSIGN_PLAYER_OBJ:{ // assign object to client for camera
				auto id = stream->Read<NetworkGameObject::Id>();
				auto worldPosition = stream->Read<glm::dvec3>();
				auto forwardVector = stream->Read<glm::dvec3>();
				auto upVector = stream->Read<glm::dvec3>();
				
				// LOG_DEBUG("Client ReceiveAction ASSIGN_PLAYER_OBJ for obj id " << id)
				try {
					std::lock_guard lock(clientSideObjects->mutex);
					auto obj = clientSideObjects->objects.at(id);
					if (auto entity = obj->renderableGeometryEntityInstance.lock(); entity) {
						scene->cameraParent = entity;
						entity->rayTracingMask &= ~GEOMETRY_ATTR_PRIMARY_VISIBLE;
					}
					playerView->SetInitialPositionAndView(worldPosition, forwardVector, upVector, true);
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
	
	#pragma region Rendering
	
	V4D_MODULE_FUNC(void, DrawUi2) {
		#ifdef _ENABLE_IMGUI
			ImGui::Checkbox("Player visible", &playerVisible);
		#endif
	}
	
	V4D_MODULE_FUNC(void, BeginFrameUpdate) {
		if (auto cameraParent = scene->cameraParent.lock(); cameraParent) {
			cameraParent->rayTracingMask = playerVisible? GEOMETRY_ATTR_CAST_SHADOWS|GEOMETRY_ATTR_REFLECTION_VISIBLE|GEOMETRY_ATTR_SOLID : 0;
		}
	}
	
	#pragma endregion
	
	#pragma region GameObjects
	
	V4D_MODULE_FUNC(void, AddGameObjectToScene, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		switch (obj->type) {
			case OBJECT_TYPE::Player:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				entity->Add_physics(PhysicsInfo::RigidBodyType::STATIC, 1.0f)->SetSphereCollider(0.4f);
				entity->generator = [](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "default");
					droneModel.Generate(device, entity);
				};
			}break;
			case OBJECT_TYPE::Ball:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				float radius = 0.5f;
				entity->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 1.0f)->SetSphereCollider(radius);
				entity->generator = [radius](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "aabb_sphere");
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {{glm::vec3(-radius), glm::vec3(radius)}});
					entity->Add_meshVertexColor()->AllocateBuffers(device, {{0.5f,0.5f,0.5f,1.0f}});
				};
			}break;
			case OBJECT_TYPE::Light:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				float radius = 2;
				entity->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 5.0f)->SetSphereCollider(radius);
				entity->generator = [radius](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "aabb_sphere.light");
					entity->rayTracingMask = GEOMETRY_ATTR_PRIMARY_VISIBLE|GEOMETRY_ATTR_REFLECTION_VISIBLE;
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {{glm::vec3(-radius), glm::vec3(radius)}});
					entity->Add_meshVertexColor()->AllocateBuffers(device, {{100000.0f,100000.0f,100000.0f,100000.0f}});
					entity->Add_lightSource(glm::vec3{0,0,0}, glm::vec3{1}, radius, 100000.0f);
				};
			}break;
			case OBJECT_TYPE::Drone:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				entity->Add_physics(PhysicsInfo::RigidBodyType::STATIC, 1.0f)->SetSphereCollider(0.4f);
				entity->generator = [](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "default");
					droneModel.Generate(device, entity);
				};
			}break;
			case OBJECT_TYPE::Glass:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				entity->Add_physics(PhysicsInfo::RigidBodyType::STATIC, 1.0f)->SetBoxCollider({2.0f, 2.0f, 0.01f});
				entity->generator = [](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "glass");
					entity->Add_meshVertexPosition()->AllocateBuffers(device, {
						{-2.0,-2.0, 0.0},
						{ 2.0,-2.0, 0.0},
						{ 2.0, 2.0, 0.0},
						{-2.0, 2.0, 0.0},
					});
					entity->Add_meshVertexNormal()->AllocateBuffers(device, {
						{ 0.0, 0.0, 1.0},
						{ 0.0, 0.0, 1.0},
						{ 0.0, 0.0, 1.0},
						{ 0.0, 0.0, 1.0},
					});
					entity->Add_meshVertexColor()->AllocateBuffers(device, {
						{1.0, 1.0, 1.0, 0.01},
						{1.0, 1.0, 1.0, 0.01},
						{1.0, 1.0, 1.0, 0.01},
						{1.0, 1.0, 1.0, 0.01},
					});
					entity->Add_meshIndices()->AllocateBuffers(device, {
						0, 1, 2, 2, 3, 0,
					});
				};
			}break;
		}
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
			std::lock_guard lock(playerView->mu);
			
			switch (key) {
				// Throw stuff
				case GLFW_KEY_B:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("ball");
						stream << DVector3{playerView->viewForward.x, playerView->viewForward.y, playerView->viewForward.z};
					ClientEnqueueAction(stream);
				}break;
				// case GLFW_KEY_N:{
				// 	v4d::data::WriteOnlyStream stream(32);
				// 		stream << networking::action::TEST_OBJ;
				// 		stream << std::string("balls");
				// 		stream << DVector3{playerView->viewForward.x, playerView->viewForward.y, playerView->viewForward.z};
				// 	ClientEnqueueAction(stream);
				// }break;
				case GLFW_KEY_L:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("light");
						stream << DVector3{playerView->viewForward.x, playerView->viewForward.y, playerView->viewForward.z};
					ClientEnqueueAction(stream);
				}break;
				case GLFW_KEY_INSERT:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("drone");
						stream << DVector3{playerView->viewForward.x, playerView->viewForward.y, playerView->viewForward.z};
					ClientEnqueueAction(stream);
				}break;
				case GLFW_KEY_G:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("glass");
						stream << DVector3{playerView->viewForward.x, playerView->viewForward.y, playerView->viewForward.z};
					ClientEnqueueAction(stream);
				}break;
				case GLFW_KEY_DELETE:{
					v4d::data::WriteOnlyStream stream(8);
						stream << networking::action::TEST_OBJ;
						stream << std::string("clear");
					ClientEnqueueAction(stream);
				}break;
			}
		}
	}
	
	#pragma endregion
	
};
