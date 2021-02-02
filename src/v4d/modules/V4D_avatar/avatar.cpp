#define _V4D_MODULE
#include <v4d.h>
#include "../V4D_flycam/common.hh"
#include "../V4D_multiplayer/ServerSideObjects.hh"
#include "../V4D_multiplayer/ClientSideObjects.hh"
#include "../V4D_raytracing/camera_options.hh"

namespace OBJECT_TYPE {
	const uint32_t Avatar = 0;
}

namespace networking::action {
	typedef uint8_t Action;
	// const Action EXTENDED_ACTION = 0;

	// Action stream Only (TCP)
	const Action ASSIGN_PLAYER_OBJ = 0; // + (objectID) = 4
	
	const Action SPAWN_AVATAR = 101; // + (string) = variable size
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

struct Avatar {
	std::shared_ptr<RenderableGeometryEntity> root = nullptr;
	std::shared_ptr<RenderableGeometryEntity> torso = nullptr;
	std::shared_ptr<RenderableGeometryEntity> r_upperleg = nullptr;
	std::shared_ptr<RenderableGeometryEntity> l_upperleg = nullptr;
	std::shared_ptr<RenderableGeometryEntity> r_lowerleg = nullptr;
	std::shared_ptr<RenderableGeometryEntity> l_lowerleg = nullptr;
	std::shared_ptr<RenderableGeometryEntity> r_foot = nullptr;
	std::shared_ptr<RenderableGeometryEntity> l_foot = nullptr;
	
	const glm::dmat4 spine = glm::translate(glm::dmat4(1), glm::dvec3{ 0,+.1, 0});
	const glm::dmat4 r_hip = glm::translate(glm::dmat4(1), glm::dvec3{+.2,-.2, 0});
	const glm::dmat4 r_knee = glm::translate(glm::dmat4(1), glm::dvec3{0,-.4, 0});
	const glm::dmat4 r_ankle = glm::translate(glm::dmat4(1), glm::dvec3{0,-.4, 0});
	const glm::dmat4 l_hip = glm::translate(glm::dmat4(1), glm::dvec3{-.2,-.2, 0});
	const glm::dmat4 l_knee = glm::translate(glm::dmat4(1), glm::dvec3{0,-.4, 0});
	const glm::dmat4 l_ankle = glm::translate(glm::dmat4(1), glm::dvec3{0,-.4, 0});
	
	Avatar(v4d::scene::NetworkGameObject::Id objId) {

		RenderableGeometryEntity::Material material {};
		material.visibility.roughness = 127;
		material.visibility.metallic = 0;
		material.visibility.baseColor = {255,255,255,255};
		
		// Ass
		{
			root = RenderableGeometryEntity::Create(THIS_MODULE, objId);
			auto rootPhysics = root->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 20);
			rootPhysics->angularFactor = 0;
			rootPhysics->SetBoxCollider({.1,.1,.1});
			root->generator = [material](RenderableGeometryEntity* entity, Device* device){
				entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
				entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.2, -.2, -.2), glm::vec3(+.2, +.1, +.2)});
			};
		
			// Torso
			{
				torso = RenderableGeometryEntity::Create(THIS_MODULE, objId);
				torso->SetInitialTransform(spine, root);
				auto physics = torso->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 40);
				physics->jointParent = rootPhysics->uniqueId;
				physics->localJointPoint = glm::dmat4(1);
				physics->parentJointPoint = spine;
				physics->jointTranslationLimitsY = {-0.04, 0.04};
				physics->jointRotationLimitsY = {-glm::radians(30.0f), +glm::radians(30.0f)};
				physics->jointRotationLimitsX = {-glm::radians(10.0f), +glm::radians(10.0f)};
				physics->jointRotationLimitsZ = {-glm::radians(5.0f), +glm::radians(5.0f)};
				physics->SetBoxCollider({.1,.1,.1});
				torso->generator = [material](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.3, 0, -.2), glm::vec3(+.3, +.4, +.2)});
				};
			}
			
			// Right leg
			{
				r_upperleg = RenderableGeometryEntity::Create(THIS_MODULE, objId);
				r_upperleg->SetInitialTransform(r_hip, root);
				auto rightUpperLegPhysics = r_upperleg->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 8);
				rightUpperLegPhysics->jointParent = rootPhysics->uniqueId;
				rightUpperLegPhysics->localJointPoint = glm::dmat4(1);
				rightUpperLegPhysics->parentJointPoint = r_hip;
				rightUpperLegPhysics->jointRotationLimitsX = {0.0f, +glm::radians(90.0f)};
				rightUpperLegPhysics->jointRotationLimitsZ = {0.0f, +glm::radians(70.0f)};
				rightUpperLegPhysics->jointRotationLimitsY = {-glm::radians(10.0f), +glm::radians(10.0f)};
				rightUpperLegPhysics->SetBoxCollider({.1,.1,.1});
				r_upperleg->generator = [material](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.4, -.1), glm::vec3(+.1, 0, +.1)});
				};
				{
					r_lowerleg = RenderableGeometryEntity::Create(THIS_MODULE, objId);
					r_lowerleg->SetInitialTransform(r_knee, r_upperleg);
					auto rightLowerLegPhysics = r_lowerleg->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 6);
					rightLowerLegPhysics->jointParent = rightUpperLegPhysics->uniqueId;
					rightLowerLegPhysics->localJointPoint = glm::dmat4(1);
					rightLowerLegPhysics->parentJointPoint = r_knee;
					rightLowerLegPhysics->jointRotationLimitsX = {-glm::radians(90.0f), 0.0f};
					rightLowerLegPhysics->SetBoxCollider({.1,.1,.1});
					r_lowerleg->generator = [material](RenderableGeometryEntity* entity, Device* device){
						entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
						entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.4, -.1), glm::vec3(+.1, 0, +.1)});
					};
					{
						r_foot = RenderableGeometryEntity::Create(THIS_MODULE, objId);
						r_foot->SetInitialTransform(r_ankle, r_lowerleg);
						auto physics = r_foot->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 5);
						physics->jointParent = rightLowerLegPhysics->uniqueId;
						physics->localJointPoint = glm::dmat4(1);
						physics->parentJointPoint = r_ankle;
						physics->jointRotationLimitsX = {-glm::radians(20.0f), +glm::radians(20.0f)};
						physics->SetBoxCollider({.1,.1,.1});
						r_foot->generator = [material](RenderableGeometryEntity* entity, Device* device){
							entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
							entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.1, -.2), glm::vec3(+.1, 0, +.1)});
						};
					}
				}
			}
			
			// Left leg
			{
				l_upperleg = RenderableGeometryEntity::Create(THIS_MODULE, objId);
				l_upperleg->SetInitialTransform(l_hip, root);
				auto leftUpperLegPhysics = l_upperleg->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 8);
				leftUpperLegPhysics->jointParent = rootPhysics->uniqueId;
				leftUpperLegPhysics->localJointPoint = glm::dmat4(1);
				leftUpperLegPhysics->parentJointPoint = l_hip;
				leftUpperLegPhysics->jointRotationLimitsX = {0.0f, +glm::radians(90.0f)};
				leftUpperLegPhysics->jointRotationLimitsZ = {-glm::radians(70.0f), 0.0f};
				leftUpperLegPhysics->jointRotationLimitsY = {-glm::radians(10.0f), +glm::radians(10.0f)};
				leftUpperLegPhysics->SetBoxCollider({.1,.1,.1});
				l_upperleg->generator = [material](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.4, -.1), glm::vec3(+.1, 0, +.1)});
				};
				{
					l_lowerleg = RenderableGeometryEntity::Create(THIS_MODULE, objId);
					l_lowerleg->SetInitialTransform(l_knee, l_upperleg);
					auto leftLowerLegPhysics = l_lowerleg->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 6);
					leftLowerLegPhysics->jointParent = leftUpperLegPhysics->uniqueId;
					leftLowerLegPhysics->localJointPoint = glm::dmat4(1);
					leftLowerLegPhysics->parentJointPoint = l_knee;
					leftLowerLegPhysics->jointRotationLimitsX = {-glm::radians(90.0f), 0.0f};
					leftLowerLegPhysics->SetBoxCollider({.1,.1,.1});
					l_lowerleg->generator = [material](RenderableGeometryEntity* entity, Device* device){
						entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
						entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.4, -.1), glm::vec3(+.1, 0, +.1)});
					};
					{
						l_foot = RenderableGeometryEntity::Create(THIS_MODULE, objId);
						l_foot->SetInitialTransform(l_ankle, l_lowerleg);
						auto physics = l_foot->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 5);
						physics->jointParent = leftLowerLegPhysics->uniqueId;
						physics->localJointPoint = glm::dmat4(1);
						physics->parentJointPoint = l_ankle;
						physics->jointRotationLimitsX = {-glm::radians(20.0f), +glm::radians(20.0f)};
						physics->SetBoxCollider({.1,.1,.1});
						l_foot->generator = [material](RenderableGeometryEntity* entity, Device* device){
							entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
							entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.1, -.2), glm::vec3(+.1, 0, +.1)});
						};
					}
				}
			}
			
		}
		
	}
	
	~Avatar() {
		// Torso
		if (torso) {
			torso->Destroy();
			torso.reset();
		}
		
		// Right leg
		if (r_foot) {
			r_foot->Destroy();
			r_foot.reset();
		}
		if (r_lowerleg) {
			r_lowerleg->Destroy();
			r_lowerleg.reset();
		}
		if (r_upperleg) {
			r_upperleg->Destroy();
			r_upperleg.reset();
		}
		
		// Left leg
		if (l_foot) {
			l_foot->Destroy();
			l_foot.reset();
		}
		if (l_lowerleg) {
			l_lowerleg->Destroy();
			l_lowerleg.reset();
		}
		if (l_upperleg) {
			l_upperleg->Destroy();
			l_upperleg.reset();
		}
		
		// Ass
		if (root) {
			root->Destroy();
			root.reset();
		}
	}
};

std::shared_ptr<Avatar> avatar = nullptr;

V4D_MODULE_CLASS(V4D_Mod) {
	
	#pragma region Init
	
	V4D_MODULE_FUNC(void, ModuleLoad) {
		// Load Dependencies
		playerView = (PlayerView*)V4D_Mod::LoadModule("V4D_flycam")->ModuleGetCustomPtr(0);
		mainRenderModule = V4D_Mod::LoadModule(APP_MAIN_RENDER_MODULE);
		mainMultiplayerModule = V4D_Mod::LoadModule(APP_MAIN_MULTIPLAYER_MODULE);
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
	
	// V4D_MODULE_FUNC(void, ServerIncomingClient, IncomingClientPtr client) {
	// 	LOG("Server: IncomingClient " << client->id)
	// 	NetworkGameObject::Id playerObjId;
	// 	{
	// 		std::lock_guard lock(serverSideObjects->mutex);
	// 		auto obj = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::Avatar);
	// 		obj->physicsClientID = client->id;
	// 		obj->isDynamic = true;
	// 		obj->clientIterations[client->id] = 0;
	// 		serverSideObjects->players[client->id] = obj;
	// 		playerObjId = obj->id;
	// 	}
	// 	v4d::data::WriteOnlyStream stream(sizeof(ASSIGN_PLAYER_OBJ) + sizeof(playerObjId));
	// 		stream << ASSIGN_PLAYER_OBJ;
	// 		stream << playerObjId;
	// 	std::lock_guard lock(serverActionQueueMutex);
	// 	serverActionQueuePerClient[client->id].emplace(stream);
	// }
	
	V4D_MODULE_FUNC(void, ServerReceiveAction, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		auto action = stream->Read<Action>();
		switch (action) {
			
			case SPAWN_AVATAR:{
				NetworkGameObjectPtr playerObj = serverSideObjects->players.at(client->id);
				auto dir = stream->Read<DVector3>();
				std::lock_guard lock(serverSideObjects->mutex);
				// Spawn avatar
				auto ball = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::Avatar);
				ball->SetTransform(glm::translate(glm::dmat4(1), glm::dvec3{dir.x, dir.y, dir.z} * 5.0) * playerObj->GetTransform());
				ball->isDynamic = true;
				ball->physicsClientID = client->id;
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
		
		// Set position
		const glm::dvec3 sun1Position = {-1.496e+11,0, 0};
		auto worldPosition = glm::dvec3{-493804, -7.27024e+06, 3.33978e+06};
		auto forwardVector = glm::normalize(sun1Position);
		auto upVector = glm::normalize(worldPosition);
		auto rightVector = glm::cross(forwardVector, upVector);
		forwardVector = glm::cross(upVector, rightVector);
		worldPosition -= upVector*2.0;
		
		// Ground
		RenderableGeometryEntity::Create(v4d::modular::ModuleID(0,0), 0/*objId*/)
			->SetInitialTransform(glm::inverse(glm::lookAt(worldPosition, forwardVector, upVector)))
			->generator = [](RenderableGeometryEntity* entity, Device* device){
				RenderableGeometryEntity::Material mat {};
					// mat.visibility.textures[0] = 2;//Renderer::sbtOffsets["call:tex_checker"];
					// mat.visibility.texFactors[0] = 255;
					mat.visibility.baseColor = {255,0,0,255};
					mat.visibility.metallic = 255;
					mat.visibility.roughness = 20;
				
				// entity->Allocate(device)->material = mat;
				// entity->Add_meshVertexPosition()->AllocateBuffersFromList(device, {
				// 	{-200.0,-200.0, 0.0},
				// 	{ 200.0,-200.0, 0.0},
				// 	{ 200.0, 200.0, 0.0},
				// 	{-200.0, 200.0, 0.0},
				// });
				// entity->Add_meshVertexNormal()->AllocateBuffersFromList(device, {
				// 	{ 0.0, 0.0, 1.0},
				// 	{ 0.0, 0.0, 1.0},
				// 	{ 0.0, 0.0, 1.0},
				// 	{ 0.0, 0.0, 1.0},
				// });
				// entity->Add_physics(PhysicsInfo::RigidBodyType::STATIC)->SetMeshCollider(entity->Add_meshIndices16()->AllocateBuffersFromList(device, {
				// 	0, 1, 2, 2, 3, 0,
				// }), 6);
				
				entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = mat;
				entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-20, -20, -1), glm::vec3(+20, +20, +1)});
				entity->Add_physics(PhysicsInfo::RigidBodyType::STATIC)->SetBoxCollider({20,20,1});
				
			};
		
	}
	
	V4D_MODULE_FUNC(void, AddGameObjectToScene, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		switch (obj->type) {
			case OBJECT_TYPE::Avatar:{
				avatar = std::make_shared<Avatar>(obj->id);
				obj->renderableGeometryEntityInstance = avatar->root;
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
				// Spawn Avatar
				case GLFW_KEY_ENTER:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::SPAWN_AVATAR;
						stream << DVector3{playerView->viewForward.x, playerView->viewForward.y, playerView->viewForward.z};
					ClientEnqueueAction(stream);
				}break;
			}
		}
	}
	
	#pragma endregion
	
	V4D_MODULE_FUNC(void, DrawUi) {
		#ifdef _ENABLE_IMGUI
			ImGui::SetNextWindowSize({400, 400}, ImGuiCond_FirstUseEver);
			ImGui::Begin("Avatar testing");
				if (avatar && avatar->root) {
					auto lock = avatar->root->GetLock();
					
					auto testForces = [](std::string name, std::shared_ptr<v4d::graphics::RenderableGeometryEntity> entity){
						if (entity) {
							if (std::unique_lock<std::recursive_mutex> l = entity->GetLock(); l) {
								if (auto physics = entity->physics.Lock(); physics) {
									ImGui::Separator();
									
									if (physics->jointTranslationLimitsX.min < physics->jointTranslationLimitsX.max) {
										ImGui::SliderFloat((name + " Translation X").c_str(), &physics->jointTranslationTarget.x, physics->jointTranslationLimitsX.min, physics->jointTranslationLimitsX.max);
									}
									if (physics->jointTranslationLimitsY.min < physics->jointTranslationLimitsY.max) {
										ImGui::SliderFloat((name + " Translation Y").c_str(), &physics->jointTranslationTarget.y, physics->jointTranslationLimitsY.min, physics->jointTranslationLimitsY.max);
									}
									if (physics->jointTranslationLimitsZ.min < physics->jointTranslationLimitsZ.max) {
										ImGui::SliderFloat((name + " Translation Z").c_str(), &physics->jointTranslationTarget.z, physics->jointTranslationLimitsZ.min, physics->jointTranslationLimitsZ.max);
									}
									
									if (physics->jointRotationLimitsX.min < physics->jointRotationLimitsX.max) {
										ImGui::SliderFloat((name + " Rotation X").c_str(), &physics->jointRotationTarget.x, physics->jointRotationLimitsX.min, physics->jointRotationLimitsX.max);
									}
									if (physics->jointRotationLimitsY.min < physics->jointRotationLimitsY.max) {
										ImGui::SliderFloat((name + " Rotation Y").c_str(), &physics->jointRotationTarget.y, physics->jointRotationLimitsY.min, physics->jointRotationLimitsY.max);
									}
									if (physics->jointRotationLimitsZ.min < physics->jointRotationLimitsZ.max) {
										ImGui::SliderFloat((name + " Rotation Z").c_str(), &physics->jointRotationTarget.z, physics->jointRotationLimitsZ.min, physics->jointRotationLimitsZ.max);
									}
								}
							}
						}
					};
					
					testForces("Torso", avatar->torso);
					testForces("Right upper leg", avatar->r_upperleg);
					testForces("Left upper leg", avatar->l_upperleg);
					testForces("Right lower leg", avatar->r_lowerleg);
					testForces("Left lower leg", avatar->l_lowerleg);
					testForces("Right foot", avatar->r_foot);
					testForces("Left foot", avatar->l_foot);
					
				}
			ImGui::End();
		#endif
	}
	
};
