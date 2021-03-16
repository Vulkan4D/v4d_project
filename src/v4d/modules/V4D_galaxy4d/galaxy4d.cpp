#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"
#include "actions.hh"
#include "../V4D_flycam/common.hh"
#include "../V4D_raytracing/camera_options.hh"
#include "../V4D_multiplayer/ServerSideObjects.hh"
#include "../V4D_multiplayer/ClientSideObjects.hh"

GltfModelLoader cake {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/cake.glb")};

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

float terrainNegationSphereRadiusPower = 1;

struct TerrainDigSphere {
	float radius;
	void operator() (RenderableGeometryEntity* entity, Device* device){
		RenderableGeometryEntity::Material mat {};
		mat.visibility.roughness = 254;
		mat.visibility.baseColor = {255,255,255,255};
		entity->Allocate(device, "V4D_galaxy4d:terrain.dig.sphere")->material = mat;
		entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
	}
};

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
				
				if (key == "ball") {
					auto dir = stream->Read<DVector3>();
					std::lock_guard lock(serverSideObjects->mutex);
					// Launch ball
					auto ball = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::Ball);
					NetworkGameObjectPtr playerObj = serverSideObjects->players.at(client->id);
					ball->SetTransform(glm::translate(glm::dmat4(1), glm::dvec3{dir.x, dir.y, dir.z} * 5.0) * playerObj->GetTransform());
					ball->SetVelocity(glm::dvec3{dir.x, dir.y, dir.z}*20.0);
					ball->isDynamic = true;
					ball->physicsClientID = client->id;
				}
				else if (key == "glassBall") {
					auto dir = stream->Read<DVector3>();
					std::lock_guard lock(serverSideObjects->mutex);
					// Launch glass ball
					auto ball = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::GlassBall);
					NetworkGameObjectPtr playerObj = serverSideObjects->players.at(client->id);
					ball->SetTransform(glm::translate(glm::dmat4(1), glm::dvec3{dir.x, dir.y, dir.z} * 5.0) * playerObj->GetTransform());
					ball->SetVelocity(glm::dvec3{dir.x, dir.y, dir.z}*10.0);
					ball->isDynamic = true;
					ball->physicsClientID = client->id;
				}
				else if (key == "TerrainDigSphere") {
					auto dir = stream->Read<DVector3>();
					float radius = stream->Read<float>();
					glm::dvec3 pos = glm::dvec3{dir.x, dir.y, dir.z} * double(radius)/2.0;
					NetworkGameObjectPtr playerObj = serverSideObjects->players.at(client->id);
					auto transform = glm::translate(playerObj->GetTransform(), pos);
					glm::i32vec4 sphereHash = glm::i32vec4{glm::round(transform[3].x*20), glm::round(transform[3].y*20), glm::round(transform[3].z*20), glm::round(radius)};
					static std::unordered_set<glm::i32vec4> existingSpheres {};
					std::lock_guard lock(serverSideObjects->mutex);
					if (!existingSpheres.count(sphereHash)) {
						existingSpheres.emplace(sphereHash);
						// Dig in terrain
						auto digSphere = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::TerrainDigSphere);
						digSphere->entityData = radius;
						digSphere->SetTransform(transform);
						digSphere->isDynamic = false;
					}
				}
				else if (key == "balls") {
					std::lock_guard lock(serverSideObjects->mutex);
					auto dir = stream->Read<DVector3>();
					// Launch 10 balls
					for (int i = 0; i < 10; ++i) {
						auto ball = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::Ball);
						NetworkGameObjectPtr playerObj = serverSideObjects->players.at(client->id);
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
					NetworkGameObjectPtr playerObj = serverSideObjects->players.at(client->id);
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
					NetworkGameObjectPtr playerObj = serverSideObjects->players.at(client->id);
					drone->SetTransform(glm::translate(glm::dmat4(1), glm::dvec3{dir.x, dir.y, dir.z} * 5.0) * playerObj->GetTransform());
					drone->isDynamic = false;
					drone->physicsClientID = client->id;
				}
				else if (key == "glass") {
					auto dir = stream->Read<DVector3>();
					std::lock_guard lock(serverSideObjects->mutex);
					auto drone = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::Glass);
					NetworkGameObjectPtr playerObj = serverSideObjects->players.at(client->id);
					drone->SetTransform(glm::translate(glm::dmat4(1), glm::dvec3{dir.x, dir.y, dir.z} * 5.0) * playerObj->GetTransform());
					drone->isDynamic = false;
					drone->physicsClientID = client->id;
				}
				else if (key == "clear") {
					std::lock_guard lock(serverSideObjects->mutex);
					NetworkGameObjectPtr playerObj = serverSideObjects->players.at(client->id);
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
	
	V4D_MODULE_FUNC(void, SendStreamCustomGameObjectData, v4d::scene::NetworkGameObjectPtr obj, v4d::data::WriteOnlyStream& stream) {
		if (obj->type == OBJECT_TYPE::TerrainDigSphere) {
			try {
				float radius = std::any_cast<float>(obj->entityData);
				// Data over network
				stream.Write(radius);
			} catch(std::bad_any_cast& e){}
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
	
	V4D_MODULE_FUNC(void, ReceiveStreamCustomGameObjectData, v4d::scene::NetworkGameObjectPtr obj, v4d::data::ReadOnlyStream& stream) {
		if (obj->type == OBJECT_TYPE::TerrainDigSphere) {
			// Data over network
			obj->entityData = stream.Read<float>();
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
					}
					scene->camera.worldPosition = worldPosition;
					playerView->SetInitialViewDirection(forwardVector, upVector, true);
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
	
	V4D_MODULE_FUNC(void, ConfigureShaders) {
		mainRenderModule->AddRayTracingHitShader(THIS_MODULE, "terrain.dig.sphere");
	}
	
	V4D_MODULE_FUNC(void, DrawUi2) {
		#ifdef _ENABLE_IMGUI
			ImGui::Checkbox("Player visible", &playerVisible);
			ImGui::SliderFloat("Terrain dig sphere radius (10 to the power of)", &terrainNegationSphereRadiusPower, 0.0f, 7.0f);
			
			// {
			// 	ImGui::Separator();
				
			// 	using namespace glm;

			// 	const double pi = 3.14159265359;
			// 	const dvec3 wavelengthsRGB = dvec3(680, 550, 440)/1e9;
			// 	const double wavelength = 680 / 1e9;
				
			// 	// const double BoltzmannConstant = 1.380649e-23;
			// 	// double temperatureK = 250;
			// 	// double gravityAccelerationMS2 = 9.8;
			// 	//double moleculeMassKg = ;
			// 	// static double scaleHeightKM = 8.5;// (BoltzmannConstant * temperatureK) / (moleculeMassKg * gravityAccelerationMS2);
			// 	// static double altitudeKM = 2;
				
			// 	static double iorOfAir = 1.0003;
			// 	static double molecularDensityAtSeaLevel = 1.225;

			// 	// ImGui::InputDouble("scaleHeightKM", &scaleHeightKM);
			// 	// ImGui::InputDouble("altitudeKM", &altitudeKM);
			// 	ImGui::InputDouble("iorOfAir", &iorOfAir); 
			// 	ImGui::InputDouble("molecularDensityAtSeaLevel", &molecularDensityAtSeaLevel);
				
			// 	dvec3 beta_rayleigh = (8.0 * pi*pi*pi * pow(iorOfAir*iorOfAir - 1.0, 2.0)) / (3.0 * molecularDensityAtSeaLevel * wavelengthsRGB*wavelengthsRGB*wavelengthsRGB*wavelengthsRGB) ;// * exp(-altitudeKM/scaleHeightKM);
			// 	double beta_rayleigh_r = (8.0 * pi*pi*pi * pow(iorOfAir*iorOfAir - 1.0, 2.0)) / (3.0 * molecularDensityAtSeaLevel * wavelength*wavelength*wavelength*wavelength);
				
			// 	std::ostringstream streamObj;
			// 	streamObj << beta_rayleigh.r << ", " << beta_rayleigh.g << ", " << beta_rayleigh.b << " ::: " << beta_rayleigh_r;
				
			// 	ImGui::Text(streamObj.str().c_str());
			// }
			
			
		#endif
	}
	
	V4D_MODULE_FUNC(void, RenderFrame_BeforeUpdate) {
		if (auto cameraParent = scene->cameraParent.lock(); cameraParent) {
			cameraParent->rayTracingMask = playerVisible? RAY_TRACED_ENTITY_DEFAULT : 0;
		}
	}
	
	#pragma endregion
	
	#pragma region GameObjects
	
	V4D_MODULE_FUNC(void, CreateGameObject, v4d::scene::NetworkGameObjectPtr obj) {
		switch (obj->type) {
			case OBJECT_TYPE::TerrainDigSphere:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				entity->rayTracingMask = RAY_TRACED_ENTITY_TERRAIN_NEGATE;
				obj->renderableGeometryEntityInstance = entity;
				obj->entityData = 0.0f;
			}break;
		}
	}
	V4D_MODULE_FUNC(void, AddGameObjectToScene, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		switch (obj->type) {
			case OBJECT_TYPE::Player:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				entity->generator = cake;
				entity->Remove_physics();
			}break;
			case OBJECT_TYPE::Ball:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				float radius = 0.5f;
				auto physics = entity->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 1.0f);
				physics->SetSphereCollider(radius);
				physics->friction = 0.6;
				physics->bounciness = 0.7;
				entity->generator = [radius](RenderableGeometryEntity* entity, Device* device){
					RenderableGeometryEntity::Material mat {};
					mat.visibility.textures[0] = Renderer::sbtOffsets["call:tex_checker"];
					mat.visibility.texFactors[0] = 255;
					mat.visibility.roughness = 0;
					mat.visibility.metallic = 1;
					entity->Allocate(device, "V4D_raytracing:aabb_sphere")->material = mat;
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
					entity->Add_meshVertexColorU8()->AllocateBuffers(device, {127,127,127,255});
				};
			}break;
			case OBJECT_TYPE::GlassBall:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				float radius = 0.5f;
				auto physics = entity->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 1.0f);
				physics->SetSphereCollider(radius);
				physics->friction = 0.6;
				physics->bounciness = 0.7;
				entity->generator = [radius](RenderableGeometryEntity* entity, Device* device){
					RenderableGeometryEntity::Material mat {};
					mat.visibility.indexOfRefraction = 1.1 * 50;
					mat.visibility.roughness = 0;
					entity->Allocate(device, "V4D_raytracing:aabb_sphere")->material = mat;
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
					entity->Add_meshVertexColorU8()->AllocateBuffers(device, {255,255,255,16});
				};
			}break;
			case OBJECT_TYPE::TerrainDigSphere:{
				if (auto entity = obj->renderableGeometryEntityInstance.lock(); entity) {
					try {
						float radius = std::any_cast<float>(obj->entityData);
						entity->generator = TerrainDigSphere{radius};
					} catch(std::bad_any_cast& e){}
				}
			}break;
			case OBJECT_TYPE::Light:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				float radius = 2;
				auto physics = entity->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 5.0f);
				physics->SetSphereCollider(radius);
				physics->friction = 0.6;
				physics->bounciness = 0.7;
				entity->generator = [radius](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "V4D_raytracing:aabb_sphere.light")->material.visibility.emission = 100000.0f;
					entity->rayTracingMask = RAY_TRACED_ENTITY_LIGHT;
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
					entity->Add_meshVertexColorU8()->AllocateBuffers(device, {255,255,255,255});
					entity->Add_lightSource(glm::vec3{0,0,0}, glm::vec3{1}, radius, 100000.0f);
				};
			}break;
			case OBJECT_TYPE::Drone:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				entity->generator = cake;
			}break;
			case OBJECT_TYPE::Glass:{
				auto entity = RenderableGeometryEntity::Create(THIS_MODULE, obj->id);
				obj->renderableGeometryEntityInstance = entity;
				entity->Add_physics(PhysicsInfo::RigidBodyType::STATIC, 1.0f)->SetBoxCollider({2.0f, 2.0f, 0.01f});
				entity->generator = [](RenderableGeometryEntity* entity, Device* device){
					RenderableGeometryEntity::Material mat {};
					mat.visibility.indexOfRefraction = 1.02 * 50;
					mat.visibility.roughness = 0;
					entity->Allocate(device, "V4D_raytracing:default")->material = mat;
					entity->Add_meshVertexPosition()->AllocateBuffersFromList(device, {
						{-2.0,-2.0, 0.0},
						{ 2.0,-2.0, 0.0},
						{ 2.0, 2.0, 0.0},
						{-2.0, 2.0, 0.0},
					});
					entity->Add_meshVertexNormal()->AllocateBuffersFromList(device, {
						{ 0.0, 0.0, 1.0},
						{ 0.0, 0.0, 1.0},
						{ 0.0, 0.0, 1.0},
						{ 0.0, 0.0, 1.0},
					});
					entity->Add_meshVertexColorU8()->AllocateBuffersFromList(device, {
						{255, 255, 255, 16},
						{255, 255, 255, 16},
						{255, 255, 255, 16},
						{255, 255, 255, 16},
					});
					entity->Add_meshIndices16()->AllocateBuffersFromList(device, {
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
				case GLFW_KEY_H:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("glassBall");
						stream << DVector3{playerView->viewForward.x, playerView->viewForward.y, playerView->viewForward.z};
					ClientEnqueueAction(stream);
				}break;
				case GLFW_KEY_O:{
					v4d::data::WriteOnlyStream stream(64);
						stream << networking::action::TEST_OBJ;
						stream << std::string("TerrainDigSphere");
						stream << DVector3{playerView->viewForward.x, playerView->viewForward.y, playerView->viewForward.z};
						stream << std::pow(10.0f, terrainNegationSphereRadiusPower);
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
