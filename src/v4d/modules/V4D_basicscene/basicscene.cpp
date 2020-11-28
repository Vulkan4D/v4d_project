#define _V4D_MODULE
#include <v4d.h>
#include "../V4D_flycam/common.hh"
#include "../V4D_multiplayer/ServerSideObjects.hh"
#include "../V4D_multiplayer/ClientSideObjects.hh"

namespace OBJECT_TYPE {
	const uint32_t Player = 0;
	const uint32_t Ball = 1;
	const uint32_t Light = 2;
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

void ClientEnqueueAction(v4d::data::WriteOnlyStream& stream) {
	std::lock_guard lock(clientActionQueueMutex);
	clientActionQueue.emplace(stream);
}

void CreateCornellBox(ObjectInstance* obj) {
	obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;

	auto geom1 = obj->AddGeometry(28, 42);
	
	geom1->SetVertex(0,  /*pos*/{-5.0,-5.0, -2.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0, 0.0, 0.0, 1.0});
	geom1->SetVertex(1,  /*pos*/{ 5.0,-5.0, -2.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0, 1.0, 0.0, 1.0});
	geom1->SetVertex(2,  /*pos*/{ 5.0, 5.0, -2.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0, 0.0, 1.0, 1.0});
	geom1->SetVertex(3,  /*pos*/{-5.0, 5.0, -2.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0, 1.0, 1.0, 1.0});
	//
	geom1->SetVertex(4,  /*pos*/{-5.0,-5.0,-8.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0, 0.0, 0.0, 1.0});
	geom1->SetVertex(5,  /*pos*/{ 5.0,-5.0,-8.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0, 1.0, 0.0, 1.0});
	geom1->SetVertex(6,  /*pos*/{ 5.0, 5.0,-8.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0, 0.0, 1.0, 1.0});
	geom1->SetVertex(7,  /*pos*/{-5.0, 5.0,-8.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0, 1.0, 1.0, 1.0});
	
	// bottom white
	geom1->SetVertex(8,  /*pos*/{-80.0,-80.0,-20.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
	geom1->SetVertex(9,  /*pos*/{ 80.0,-80.0,-20.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
	geom1->SetVertex(10, /*pos*/{ 80.0, 80.0,-20.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
	geom1->SetVertex(11, /*pos*/{-80.0, 80.0,-20.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
	
	// top yellow
	geom1->SetVertex(12, /*pos*/{-80.0,-80.0, 40.0}, /*normal*/{ 0.0, 0.0,-1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,0.0, 1.0});
	geom1->SetVertex(13, /*pos*/{ 80.0,-80.0, 40.0}, /*normal*/{ 0.0, 0.0,-1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,0.0, 1.0});
	geom1->SetVertex(14, /*pos*/{ 80.0, 80.0, 40.0}, /*normal*/{ 0.0, 0.0,-1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,0.0, 1.0});
	geom1->SetVertex(15, /*pos*/{-80.0, 80.0, 40.0}, /*normal*/{ 0.0, 0.0,-1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,0.0, 1.0});
	
	// left red
	geom1->SetVertex(16, /*pos*/{ 80.0, 80.0,-20.0}, /*normal*/{-1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,0.0,0.0, 0.5});
	geom1->SetVertex(17, /*pos*/{ 80.0,-80.0,-20.0}, /*normal*/{-1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,0.0,0.0, 0.5});
	geom1->SetVertex(18, /*pos*/{ 80.0, 80.0, 40.0}, /*normal*/{-1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,0.0,0.0, 0.5});
	geom1->SetVertex(19, /*pos*/{ 80.0,-80.0, 40.0}, /*normal*/{-1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,0.0,0.0, 0.5});
	
	// back blue
	geom1->SetVertex(20, /*pos*/{ 80.0,-80.0, 40.0}, /*normal*/{ 0.0, 1.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,0.0,1.0, 1.0});
	geom1->SetVertex(21, /*pos*/{ 80.0,-80.0,-20.0}, /*normal*/{ 0.0, 1.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,0.0,1.0, 1.0});
	geom1->SetVertex(22, /*pos*/{-80.0,-80.0, 40.0}, /*normal*/{ 0.0, 1.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,0.0,1.0, 1.0});
	geom1->SetVertex(23, /*pos*/{-80.0,-80.0,-20.0}, /*normal*/{ 0.0, 1.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,0.0,1.0, 1.0});
	
	// right green
	geom1->SetVertex(24, /*pos*/{-80.0, 80.0,-20.0}, /*normal*/{ 1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,1.0,0.0, 1.0});
	geom1->SetVertex(25, /*pos*/{-80.0,-80.0,-20.0}, /*normal*/{ 1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,1.0,0.0, 1.0});
	geom1->SetVertex(26, /*pos*/{-80.0, 80.0, 40.0}, /*normal*/{ 1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,1.0,0.0, 1.0});
	geom1->SetVertex(27, /*pos*/{-80.0,-80.0, 40.0}, /*normal*/{ 1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,1.0,0.0, 1.0});
	
	geom1->SetIndices({
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4,
		8, 9, 10, 10, 11, 8,
		//
		13, 12, 14, 14, 12, 15,
		16, 17, 18, 18, 17, 19,
		20, 21, 22, 22, 21, 23,
		25, 24, 26, 26, 27, 25,
	});
	
}

V4D_MODULE_CLASS(V4D_Mod) {
	
	#pragma region Init
	
	V4D_MODULE_FUNC(void, ModuleLoad) {
		// Load Dependencies
		playerView = (PlayerView*)V4D_Mod::LoadModule("V4D_flycam")->ModuleGetCustomPtr(0);
		mainRenderModule = V4D_Mod::LoadModule(APP_MAIN_RENDER_MODULE);
		mainMultiplayerModule = V4D_Mod::LoadModule(APP_MAIN_MULTIPLAYER_MODULE);
		playerView->SetInitialPositionAndView({0,0,0}, {0,1,0}, {0,0,1});
		playerView->useFreeFlyCam = false;
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
					scene->cameraParent = obj->objectInstance;
					obj->objectInstance->rayTracingMaskRemoved |= GEOMETRY_ATTR_PRIMARY_VISIBLE;
					obj->objectInstance->SetGeometriesDirty();
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
		v4d::scene::Geometry::globalBuffers.objectBuffer.Extend(10000);
		v4d::scene::Geometry::globalBuffers.geometryBuffer.Extend(10000);
		v4d::scene::Geometry::globalBuffers.lightBuffer.Extend(1024);
		
		// Cornell boxes
		scene->AddObjectInstance()->Configure(CreateCornellBox, {0,250,-30}, 180.0);
		scene->AddObjectInstance()->Configure(CreateCornellBox, {200,250,-30}, 120.0);
		scene->AddObjectInstance()->Configure(CreateCornellBox, {-200,250,-30}, -120.0);
		for (int i = 0; i < 100; ++i)
			scene->AddObjectInstance()->Configure(CreateCornellBox, {0,500,-30 + (i*90)}, 180.0);
			
		// Ground (static)
		scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
			auto plane = obj->AddGeometry(4, 6);
			// plane->colliderType = Geometry::ColliderType::STATIC_PLANE;
			plane->SetVertex(0, /*pos*/{-80000.0,-80000.0, 0.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
			plane->SetVertex(1, /*pos*/{ 80000.0,-80000.0, 0.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
			plane->SetVertex(2, /*pos*/{ 80000.0, 80000.0, 0.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
			plane->SetVertex(3, /*pos*/{-80000.0, 80000.0, 0.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
			plane->SetIndices({
				0, 1, 2, 2, 3, 0,
			});
		}, {0,0,-200});
		
		// Lights (static)
		scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 20, 100000000);
		}, {10,-2000,10});
		scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 200, 1000000000);
		}, {10,-500,1000});
		scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 2, 10000);
		}, {10,270,10});
		scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 1, 10000);
		}, {210,270,-20});
		scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 1, 10000);
		}, {-190,270,-15});
		
		// Red Sphere (static)
		scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
			obj->mass = 10;
			obj->SetSphereGeometry("sphere", 50, {1,0,0, 1});
		}, {60,300,500});
	}
	
	V4D_MODULE_FUNC(void, UnloadScene) {
		for (auto obj : scene->objectInstances) {
			scene->RemoveObjectInstance(obj);
		}
		scene->objectInstances.clear();
		scene->ClenupObjectInstancesGeometries();
	}
	
	V4D_MODULE_FUNC(void, AddGameObjectToScene, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		switch (obj->type) {
			case OBJECT_TYPE::Player:{
				(obj->objectInstance = scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
					auto cube = obj->AddProceduralGeometry("aabb", 1);
						cube->SetProceduralVertex(0, glm::vec3(-0.5, -0.5, -0.5), glm::vec3(0.5, 0.5, 0.5), glm::vec4(0,1,0.5,1), 0);
						// cube->rayTracingMask = GEOMETRY_ATTR_PRIMARY_VISIBLE | GEOMETRY_ATTR_COLLIDER | GEOMETRY_ATTR_REFLECTION_VISIBLE;
				});
				obj->objectInstance->rigidbodyType = ObjectInstance::RigidBodyType::NONE;
			}break;
			case OBJECT_TYPE::Ball:{
				(obj->objectInstance = scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
					obj->SetSphereGeometry("sphere", 1, {0.5,0.5,0.5, 1});
				});
				obj->objectInstance->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
				obj->objectInstance->mass = 1;
			}break;
			case OBJECT_TYPE::Light:{
				(obj->objectInstance = scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
					obj->SetSphereLightSource("light", 2, 100000);
				});
				obj->objectInstance->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
				obj->objectInstance->mass = 1;
			}break;
		}
	}
	
	#pragma endregion
	
	#pragma region Rendering
	
	V4D_MODULE_FUNC(void, DrawUiDebug2) {
		#ifdef _ENABLE_IMGUI
			ImGui::Text("%d objects", Geometry::globalBuffers.nbAllocatedObjects);
		#endif
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
