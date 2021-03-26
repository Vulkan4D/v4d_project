#include <v4d.h>
#include <V4D_Mod.h>
#include "../V4D_flycam/common.hh"
#include "../V4D_multiplayer/ServerSideObjects.hh"
#include "../V4D_multiplayer/ClientSideObjects.hh"
#include "../V4D_raytracing/camera_options.hh"

namespace OBJECT_TYPE {
	const uint32_t Player = 0;
	const uint32_t Ball = 1;
	const uint32_t Light = 2;
	const uint32_t GlassBall = 3;
}

namespace networking::action {
	typedef uint8_t Action;
	// const Action EXTENDED_ACTION = 0;

	// Action stream Only (TCP)
	const Action ASSIGN_PLAYER_OBJ = 0; // + (objectID) = 4
	
	const Action TEST_OBJ = 101; // + (string) = variable size
}

using namespace v4d::scene;
using namespace v4d::networking;
using namespace networking::action;
using namespace v4d::graphics;

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

void ClientEnqueueAction(v4d::data::WriteOnlyStream& stream) {
	std::lock_guard lock(clientActionQueueMutex);
	clientActionQueue.emplace(stream);
}

void CreateCornellBox(RenderableGeometryEntity* entity, Device* device) {
	RenderableGeometryEntity::Material mat {};
	mat.visibility.roughness = 1.0;
	
	entity->Allocate(device, "V4D_raytracing:default")->material = mat;
	
	auto meshIndices = entity->Add_meshIndices16();
	auto meshVertexPosition = entity->Add_meshVertexPosition();
	auto meshVertexNormal = entity->Add_meshVertexNormal();
	auto meshVertexColor = entity->Add_meshVertexColorU8();
	auto physics = entity->Add_physics(PhysicsInfo::RigidBodyType::STATIC);
	
	meshIndices->AllocateBuffersFromList(device, {
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4,
		8, 9, 10, 10, 11, 8,
		13, 12, 14, 14, 12, 15,
		16, 17, 18, 18, 17, 19,
		20, 21, 22, 22, 21, 23,
		25, 24, 26, 26, 27, 25,
	});
	
	meshVertexPosition->AllocateBuffersFromList(device, {
		//
		{-5.0,-5.0, -2.0},
		{ 5.0,-5.0, -2.0},
		{ 5.0, 5.0, -2.0},
		{-5.0, 5.0, -2.0},
		//
		{-5.0,-5.0,-8.0},
		{ 5.0,-5.0,-8.0},
		{ 5.0, 5.0,-8.0},
		{-5.0, 5.0,-8.0},
		// bottom white
		{-40.0,-40.0,-20.0},
		{ 40.0,-40.0,-20.0},
		{ 40.0, 40.0,-20.0},
		{-40.0, 40.0,-20.0},
		// top yellow
		{-40.0,-40.0, 40.0},
		{ 40.0,-40.0, 40.0},
		{ 40.0, 40.0, 40.0},
		{-40.0, 40.0, 40.0},
		// left red
		{ 40.0, 40.0,-20.0},
		{ 40.0,-40.0,-20.0},
		{ 40.0, 40.0, 40.0},
		{ 40.0,-40.0, 40.0},
		// back blue
		{ 40.0,-40.0, 40.0},
		{ 40.0,-40.0,-20.0},
		{-40.0,-40.0, 40.0},
		{-40.0,-40.0,-20.0},
		// right green
		{-40.0, 40.0,-20.0},
		{-40.0,-40.0,-20.0},
		{-40.0, 40.0, 40.0},
		{-40.0,-40.0, 40.0},
	});
	
	meshVertexNormal->AllocateBuffersFromList(device, {
		//
		{ 0.0, 0.0, 1.0},
		{ 0.0, 0.0, 1.0},
		{ 0.0, 0.0, 1.0},
		{ 0.0, 0.0, 1.0},
		//
		{ 0.0, 0.0, 1.0},
		{ 0.0, 0.0, 1.0},
		{ 0.0, 0.0, 1.0},
		{ 0.0, 0.0, 1.0},
		// bottom white
		{ 0.0, 0.0, 1.0},
		{ 0.0, 0.0, 1.0},
		{ 0.0, 0.0, 1.0},
		{ 0.0, 0.0, 1.0},
		// top yellow
		{ 0.0, 0.0,-1.0},
		{ 0.0, 0.0,-1.0},
		{ 0.0, 0.0,-1.0},
		{ 0.0, 0.0,-1.0},
		// left red
		{-1.0, 0.0, 0.0},
		{-1.0, 0.0, 0.0},
		{-1.0, 0.0, 0.0},
		{-1.0, 0.0, 0.0},
		// back blue
		{ 0.0, 1.0, 0.0},
		{ 0.0, 1.0, 0.0},
		{ 0.0, 1.0, 0.0},
		{ 0.0, 1.0, 0.0},
		// right green
		{ 1.0, 0.0, 0.0},
		{ 1.0, 0.0, 0.0},
		{ 1.0, 0.0, 0.0},
		{ 1.0, 0.0, 0.0},
	});
	
	meshVertexColor->AllocateBuffersFromList(device, {
		//
		{255, 0, 0, 255},
		{0, 255, 0, 255},
		{0, 0, 255, 255},
		{0, 255, 255, 255},
		//
		{255, 0, 0, 255},
		{0, 255, 0, 255},
		{0, 0, 255, 255},
		{0, 255, 255, 255},
		// bottom white
		{255,255,255, 255},
		{255,255,255, 255},
		{255,255,255, 255},
		{255,255,255, 255},
		// top yellow
		{255,255,0, 255},
		{255,255,0, 255},
		{255,255,0, 255},
		{255,255,0, 255},
		// left red
		{255,0,0, 255},
		{255,0,0, 255},
		{255,0,0, 255},
		{255,0,0, 255},
		// back blue
		{0,0,255, 255},
		{0,0,255, 255},
		{0,0,255, 255},
		{0,0,255, 255},
		// right green
		{0,255,0, 255},
		{0,255,0, 255},
		{0,255,0, 255},
		{0,255,0, 255},
	});
	
	physics->SetMeshCollider(meshIndices->data, meshIndices->count);
}

V4D_MODULE_CLASS(V4D_Mod) {
	
	#pragma region Init
	
	V4D_MODULE_FUNC(void, ModuleLoad) {
		// Load Dependencies
		playerView = (PlayerView*)V4D_Mod::LoadModule("V4D_flycam")->ModuleGetCustomPtr(0);
		mainRenderModule = V4D_Mod::LoadModule(APP_MAIN_RENDER_MODULE);
		mainMultiplayerModule = V4D_Mod::LoadModule(APP_MAIN_MULTIPLAYER_MODULE);
		playerView->SetInitialViewDirection({0,1,0}, {0,0,1});
		playerView->useFreeFlyCam = false;
	}
	
	V4D_MODULE_FUNC(void, InitRenderer, Renderer* _r) {
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
	
	#pragma region Server
	
	V4D_MODULE_FUNC(void, ServerIncomingClient, IncomingClientPtr client) {
		LOG("Server: IncomingClient " << client->id)
		NetworkGameObject::Id playerObjId;
		{
			std::lock_guard lock(serverSideObjects->mutex);
			auto obj = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::Player);
			obj->physicsClientID = client->id;
			obj->isDynamic = true;
			obj->clientIterations[client->id] = 0;
			serverSideObjects->players[client->id] = obj;
			playerObjId = obj->id;
		}
		v4d::data::WriteOnlyStream stream(sizeof(ASSIGN_PLAYER_OBJ) + sizeof(playerObjId));
			stream << ASSIGN_PLAYER_OBJ;
			stream << playerObjId;
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
				else if (key == "glassBall") {
					auto dir = stream->Read<DVector3>();
					std::lock_guard lock(serverSideObjects->mutex);
					// Launch ball
					auto ball = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::GlassBall);
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
				// LOG_DEBUG("Client ReceiveAction ASSIGN_PLAYER_OBJ for obj id " << id)
				try {
					std::lock_guard lock(clientSideObjects->mutex);
					auto obj = clientSideObjects->objects.at(id);
					if (auto entity = obj->renderableGeometryEntityInstance.lock(); entity) {
						scene->cameraParent = entity;
					}
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
	
	#pragma region Scene
	
	V4D_MODULE_FUNC(void, LoadScene, Scene* _s) {
		scene = _s;
		
		// Cornell boxes
		RenderableGeometryEntity::Create(v4d::modular::ModuleID(0,0), 0/*objId*/)
			->SetInitialTransform(glm::rotate(glm::translate(glm::dmat4(1), {0,250,-30}), glm::radians( 180.0), {0,0,1}))->generator = CreateCornellBox;
		RenderableGeometryEntity::Create(v4d::modular::ModuleID(0,0), 0/*objId*/)
			->SetInitialTransform(glm::rotate(glm::translate(glm::dmat4(1), {200,250,-30}), glm::radians( 120.0), {0,0,1}))->generator = CreateCornellBox;
		RenderableGeometryEntity::Create(v4d::modular::ModuleID(0,0), 0/*objId*/)
			->SetInitialTransform(glm::rotate(glm::translate(glm::dmat4(1), {-200,250,-30}), glm::radians(-120.0), {0,0,1}))->generator = CreateCornellBox;
			
		// Ball
		RenderableGeometryEntity::Create(v4d::modular::ModuleID(0,0), 0/*objId*/)
			->SetInitialTransform(glm::rotate(glm::translate(glm::dmat4(1), {5,250,-30}), glm::radians(0.0), {0,0,1}))
			->generator = [](RenderableGeometryEntity* entity, Device* device){
				float radius = 2;
				entity->Allocate(device, "V4D_raytracing:aabb_sphere");
				entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
				entity->Add_meshVertexColorU8()->AllocateBuffers(device, {180,180,180,255});
				entity->Add_physics(PhysicsInfo::RigidBodyType::STATIC)->SetSphereCollider(radius);
			};
			
		// Cube
		RenderableGeometryEntity::Create(v4d::modular::ModuleID(0,0), 0/*objId*/)
			->SetInitialTransform(glm::rotate(glm::translate(glm::dmat4(1), {-5,250,-30}), glm::radians(0.0), {0,0,1}))
			->generator = [](RenderableGeometryEntity* entity, Device* device){
				entity->Allocate(device, "V4D_raytracing:aabb_cube");
				entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-2), glm::vec3(2)});
				entity->Add_meshVertexColorU8()->AllocateBuffers(device, {204,0,0,255});
				entity->Add_physics(PhysicsInfo::RigidBodyType::STATIC)->SetBoxCollider(glm::vec3(2));
			};
			
		// Light sources
		RenderableGeometryEntity::Create(v4d::modular::ModuleID(0,0), 0/*objId*/)
			->SetInitialTransform(glm::rotate(glm::translate(glm::dmat4(1), {10,-500,1000}), glm::radians(0.0), {0,0,1}))
			->generator = [](RenderableGeometryEntity* entity, Device* device){
				float radius = 200;
				entity->Allocate(device, "V4D_raytracing:aabb_sphere.light");
				entity->rayTracingMask = RAY_TRACED_ENTITY_LIGHT;
				entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
				entity->Add_meshVertexColorF32()->AllocateBuffers(device, {1000000000.0f,1000000000.0f,1000000000.0f,1000000000.0f});
				entity->Add_lightSource(glm::vec3{0,0,0}, glm::vec3{1}, radius, 1000000000.0f);
				entity->Add_physics(PhysicsInfo::RigidBodyType::STATIC)->SetSphereCollider(radius);
			};
		RenderableGeometryEntity::Create(v4d::modular::ModuleID(0,0), 0/*objId*/)
			->SetInitialTransform(glm::rotate(glm::translate(glm::dmat4(1), {10,-2000,11}), glm::radians(0.0), {0,0,1}))
			->generator = [](RenderableGeometryEntity* entity, Device* device){
				float radius = 20;
				entity->Allocate(device, "V4D_raytracing:aabb_sphere.light");
				entity->rayTracingMask = RAY_TRACED_ENTITY_LIGHT;
				entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
				entity->Add_meshVertexColorF32()->AllocateBuffers(device, {100000000.0f,100000000.0f,100000000.0f,100000000.0f});
				entity->Add_lightSource(glm::vec3{0,0,0}, glm::vec3{1}, radius, 100000000.0f);
				entity->Add_physics(PhysicsInfo::RigidBodyType::STATIC)->SetSphereCollider(radius);
			};
		RenderableGeometryEntity::Create(v4d::modular::ModuleID(0,0), 0/*objId*/)
			->SetInitialTransform(glm::rotate(glm::translate(glm::dmat4(1), {2,260,-30}), glm::radians(0.0), {0,0,1}))
			->generator = [](RenderableGeometryEntity* entity, Device* device){
				float radius = 0.5f;
				entity->Allocate(device, "V4D_raytracing:aabb_sphere.light");
				entity->rayTracingMask = RAY_TRACED_ENTITY_LIGHT;
				entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
				entity->Add_meshVertexColorF32()->AllocateBuffers(device, {10000.0f,10000.0f,10000.0f,10000.0f});
				entity->Add_physics(PhysicsInfo::RigidBodyType::STATIC)->SetSphereCollider(radius);
				entity->Add_lightSource(glm::vec3{0,0,0}, glm::vec3{1}, radius, 10000.0f);
			};
		
		// Ground
		RenderableGeometryEntity::Create(v4d::modular::ModuleID(0,0), 0/*objId*/)
			->SetInitialTransform(glm::rotate(glm::translate(glm::dmat4(1), {0,0,-200}), glm::radians(0.0), {0,0,1}))
			->generator = [](RenderableGeometryEntity* entity, Device* device){
				entity->Allocate(device, "V4D_raytracing:default");
				entity->Add_meshVertexPosition();
				entity->Add_meshVertexNormal();
				entity->Add_meshVertexColorU8();
				entity->Add_meshIndices16();
				auto meshVertexPosition = entity->meshVertexPosition.Lock();
				meshVertexPosition->AllocateBuffersFromList(device, {
					{-1000.0,-1000.0, 0.0},
					{ 1000.0,-1000.0, 0.0},
					{ 1000.0, 1000.0, 0.0},
					{-1000.0, 1000.0, 0.0},
				});
				auto meshVertexNormal = entity->meshVertexNormal.Lock();
				meshVertexNormal->AllocateBuffersFromList(device, {
					{ 0.0, 0.0, 1.0},
					{ 0.0, 0.0, 1.0},
					{ 0.0, 0.0, 1.0},
					{ 0.0, 0.0, 1.0},
				});
				auto meshVertexColor = entity->meshVertexColorU8.Lock();
				meshVertexColor->AllocateBuffersFromList(device, {
					{255,255,255, 255},
					{255,255,255, 255},
					{255,255,255, 255},
					{255,255,255, 255},
				});
				auto meshIndices = entity->meshIndices16.Lock();
				meshIndices->AllocateBuffersFromList(device, {
					0, 1, 2, 2, 3, 0,
				});
				entity->Add_physics(PhysicsInfo::RigidBodyType::STATIC)->SetMeshCollider(meshIndices->data, meshIndices->count);
			};
		
	}
	
	V4D_MODULE_FUNC(void, AddGameObjectToScene, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		switch (obj->type) {
			case OBJECT_TYPE::Player:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				// entity->generator = [](RenderableGeometryEntity* entity, Device* device){
				// 	entity->Allocate(device, "V4D_raytracing:aabb_cube");
				// 	entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-0.5), glm::vec3(0.5)});
				// 	entity->Add_meshVertexColorU8()->AllocateBuffers(device, {0,255,127, 255});
				// };
			}break;
			case OBJECT_TYPE::Ball:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				float radius = 0.5f;
				entity->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 1.0f)->SetSphereCollider(radius);
				entity->generator = [radius](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "V4D_raytracing:aabb_sphere");
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
					entity->Add_meshVertexColorU8()->AllocateBuffers(device, {127,127,127,255});
				};
			}break;
			case OBJECT_TYPE::GlassBall:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				float radius = 0.5f;
				entity->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 1.0f)->SetSphereCollider(radius);
				entity->generator = [radius](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "V4D_raytracing:aabb_sphere");
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
					entity->Add_meshVertexColorU8()->AllocateBuffers(device, {127,127,127,1});
				};
			}break;
			case OBJECT_TYPE::Light:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				float radius = 2;
				entity->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 5.0f)->SetSphereCollider(radius);
				entity->generator = [radius](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "V4D_raytracing:aabb_sphere.light");
					entity->rayTracingMask = RAY_TRACED_ENTITY_LIGHT;
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
					entity->Add_meshVertexColorF32()->AllocateBuffers(device, {100000.0f,100000.0f,100000.0f,100000.0f});
					entity->Add_lightSource(glm::vec3{0,0,0}, glm::vec3{1}, radius, 100000.0f);
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
				&& (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) || key == GLFW_KEY_ESCAPE)
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
				case GLFW_KEY_H:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("glassBall");
						stream << DVector3{playerView->viewForward.x, playerView->viewForward.y, playerView->viewForward.z};
					ClientEnqueueAction(stream);
				}break;
				case GLFW_KEY_N:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("balls");
						stream << DVector3{playerView->viewForward.x, playerView->viewForward.y, playerView->viewForward.z};
					ClientEnqueueAction(stream);
				}break;
				case GLFW_KEY_L:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("light");
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
