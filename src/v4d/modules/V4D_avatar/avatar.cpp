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













struct AnimationKeyFrame {
	RenderableGeometryEntity* entity = nullptr;
	float value = 0;
	enum class Action {NONE = 0, TRANSLATION_X, TRANSLATION_Y, TRANSLATION_Z, ROTATION_X, ROTATION_Y, ROTATION_Z, FRICTION} action;
};

struct Animation {
	float value = 0;
	std::vector<AnimationKeyFrame> keys {};
	
	void operator= (double v) {
		value = (float)v;
	}
	void operator= (float v) {
		value = v;
	}
	void operator= (int v) {
		value = (float)v;
	}
	void operator= (bool v) {
		value = v? 1.0f : 0.0f;
	}
};

struct Avatar {
	std::shared_ptr<RenderableGeometryEntity> root = nullptr;
	std::shared_ptr<RenderableGeometryEntity> torso = nullptr;
	std::shared_ptr<RenderableGeometryEntity> r_upperleg = nullptr;
	std::shared_ptr<RenderableGeometryEntity> l_upperleg = nullptr;
	std::shared_ptr<RenderableGeometryEntity> r_lowerleg = nullptr;
	std::shared_ptr<RenderableGeometryEntity> l_lowerleg = nullptr;
	std::shared_ptr<RenderableGeometryEntity> r_foot = nullptr;
	std::shared_ptr<RenderableGeometryEntity> l_foot = nullptr;
	
	const glm::dmat4 spine = glm::translate(glm::dmat4(1), glm::dvec3{ 0,+.15, 0});
	const glm::dmat4 r_hip = glm::translate(glm::dmat4(1), glm::dvec3{+.2,-.15, 0});
	const glm::dmat4 r_knee = glm::translate(glm::dmat4(1), glm::dvec3{0,-.2, 0});
	const glm::dmat4 r_ankle = glm::translate(glm::dmat4(1), glm::dvec3{0,-.2, 0});
	const glm::dmat4 l_hip = glm::translate(glm::dmat4(1), glm::dvec3{-.2,-.15, 0});
	const glm::dmat4 l_knee = glm::translate(glm::dmat4(1), glm::dvec3{0,-.2, 0});
	const glm::dmat4 l_ankle = glm::translate(glm::dmat4(1), glm::dvec3{0,-.2, 0});
	
	std::unordered_map<std::string, Animation> animations {};
	
	Avatar(v4d::scene::NetworkGameObject::Id objId) {
		RenderableGeometryEntity::Material material {};
		material.visibility.roughness = 127;
		material.visibility.metallic = 0;
		material.visibility.baseColor = {255,255,255,255};
		
		{// Create Renderable Entities
			root = RenderableGeometryEntity::Create(THIS_MODULE, objId);
			auto rootPhysics = root->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 20);
			rootPhysics->angularFactor = 0;
			rootPhysics->bounciness = 0;
			rootPhysics->friction = 1;
			rootPhysics->SetBoxCollider({.2,.15,.2});
			root->generator = [material](RenderableGeometryEntity* entity, Device* device){
				entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
				entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.2, -.15, -.2), glm::vec3(+.2, +.15, +.2)});
			};
		
			// Torso
			{
				torso = RenderableGeometryEntity::Create(THIS_MODULE, objId);
				auto physics = torso->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 40);
				physics->jointParent = rootPhysics->uniqueId;
				physics->localJointPoint = glm::translate(glm::dmat4(1), glm::dvec3{0,-.2,0});
				physics->parentJointPoint = spine;
				torso->SetInitialTransform(physics->parentJointPoint * glm::inverse(physics->localJointPoint), root);
				physics->jointTranslationLimitsY = {-0.04, 0.04};
				physics->jointRotationLimitsY = {-glm::radians(30.0f), +glm::radians(30.0f)};
				physics->jointRotationLimitsX = {-glm::radians(10.0f), +glm::radians(10.0f)};
				physics->jointRotationLimitsZ = {-glm::radians(5.0f), +glm::radians(5.0f)};
				physics->bounciness = 0;
				physics->friction = 1;
				physics->SetBoxCollider({.3,.2,.2});
				torso->generator = [material](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.3, -.2, -.2), glm::vec3(+.3, +.2, +.2)});
				};
			}
			
			// Right leg
			{
				r_upperleg = RenderableGeometryEntity::Create(THIS_MODULE, objId);
				auto rightUpperLegPhysics = r_upperleg->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 8);
				rightUpperLegPhysics->jointParent = rootPhysics->uniqueId;
				rightUpperLegPhysics->localJointPoint = glm::translate(glm::dmat4(1), glm::dvec3{0,+.2,0});
				rightUpperLegPhysics->parentJointPoint = r_hip;
				r_upperleg->SetInitialTransform(rightUpperLegPhysics->parentJointPoint * glm::inverse(rightUpperLegPhysics->localJointPoint), root);
				rightUpperLegPhysics->jointRotationLimitsX = {-glm::radians(30.0f), +glm::radians(90.0f)};
				rightUpperLegPhysics->jointRotationLimitsZ = {0.0f, +glm::radians(70.0f)};
				rightUpperLegPhysics->jointRotationLimitsY = {-glm::radians(10.0f), +glm::radians(10.0f)};
				rightUpperLegPhysics->bounciness = 0;
				rightUpperLegPhysics->friction = 1;
				rightUpperLegPhysics->SetBoxCollider({.1,.2,.1});
				r_upperleg->generator = [material](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.2, -.1), glm::vec3(+.1, +.2, +.1)});
				};
				{
					r_lowerleg = RenderableGeometryEntity::Create(THIS_MODULE, objId);
					auto rightLowerLegPhysics = r_lowerleg->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 6);
					rightLowerLegPhysics->jointParent = rightUpperLegPhysics->uniqueId;
					rightLowerLegPhysics->localJointPoint = glm::translate(glm::dmat4(1), glm::dvec3{0,+.2,0});
					rightLowerLegPhysics->parentJointPoint = r_knee;
					r_lowerleg->SetInitialTransform(rightLowerLegPhysics->parentJointPoint * glm::inverse(rightLowerLegPhysics->localJointPoint), r_upperleg);
					rightLowerLegPhysics->jointRotationLimitsX = {-glm::radians(90.0f), 0.0f};
					rightLowerLegPhysics->bounciness = 0;
					rightLowerLegPhysics->friction = 1;
					rightLowerLegPhysics->SetBoxCollider({.1,.2,.1});
					r_lowerleg->generator = [material](RenderableGeometryEntity* entity, Device* device){
						entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
						entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.2, -.1), glm::vec3(+.1, +.2, +.1)});
					};
					{
						r_foot = RenderableGeometryEntity::Create(THIS_MODULE, objId);
						auto physics = r_foot->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 5);
						physics->jointParent = rightLowerLegPhysics->uniqueId;
						physics->localJointPoint = glm::translate(glm::dmat4(1), glm::dvec3{0,+.05,+.05});
						physics->parentJointPoint = r_ankle;
						r_foot->SetInitialTransform(physics->parentJointPoint * glm::inverse(physics->localJointPoint), r_lowerleg);
						physics->jointRotationLimitsX = {-glm::radians(20.0f), +glm::radians(20.0f)};
						physics->bounciness = 0;
						physics->friction = 1;
						physics->SetBoxCollider({+.1, +.05, +.15});
						r_foot->generator = [material](RenderableGeometryEntity* entity, Device* device){
							entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
							entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.05, -.15), glm::vec3(+.1, +.05, +.15)});
						};
					}
				}
			}
			
			// Left leg
			{
				l_upperleg = RenderableGeometryEntity::Create(THIS_MODULE, objId);
				auto leftUpperLegPhysics = l_upperleg->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 8);
				leftUpperLegPhysics->jointParent = rootPhysics->uniqueId;
				leftUpperLegPhysics->localJointPoint = glm::translate(glm::dmat4(1), glm::dvec3{0,+.2,0});
				leftUpperLegPhysics->parentJointPoint = l_hip;
				l_upperleg->SetInitialTransform(leftUpperLegPhysics->parentJointPoint * glm::inverse(leftUpperLegPhysics->localJointPoint), root);
				leftUpperLegPhysics->jointRotationLimitsX = {-glm::radians(30.0f), +glm::radians(90.0f)};
				leftUpperLegPhysics->jointRotationLimitsZ = {-glm::radians(70.0f), 0.0f};
				leftUpperLegPhysics->jointRotationLimitsY = {-glm::radians(10.0f), +glm::radians(10.0f)};
				leftUpperLegPhysics->bounciness = 0;
				leftUpperLegPhysics->friction = 1;
				leftUpperLegPhysics->SetBoxCollider({.1,.2,.1});
				l_upperleg->generator = [material](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.2, -.1), glm::vec3(+.1, +.2, +.1)});
				};
				{
					l_lowerleg = RenderableGeometryEntity::Create(THIS_MODULE, objId);
					auto leftLowerLegPhysics = l_lowerleg->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 6);
					leftLowerLegPhysics->jointParent = leftUpperLegPhysics->uniqueId;
					leftLowerLegPhysics->localJointPoint = glm::translate(glm::dmat4(1), glm::dvec3{0,+.2,0});
					leftLowerLegPhysics->parentJointPoint = l_knee;
					l_lowerleg->SetInitialTransform(leftLowerLegPhysics->parentJointPoint * glm::inverse(leftLowerLegPhysics->localJointPoint), l_upperleg);
					leftLowerLegPhysics->jointRotationLimitsX = {-glm::radians(90.0f), 0.0f};
					leftLowerLegPhysics->bounciness = 0;
					leftLowerLegPhysics->friction = 1;
					leftLowerLegPhysics->SetBoxCollider({.1,.2,.1});
					l_lowerleg->generator = [material](RenderableGeometryEntity* entity, Device* device){
						entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
						entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.2, -.1), glm::vec3(+.1, +.2, +.1)});
					};
					{
						l_foot = RenderableGeometryEntity::Create(THIS_MODULE, objId);
						auto physics = l_foot->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 5);
						physics->jointParent = leftLowerLegPhysics->uniqueId;
						physics->localJointPoint = glm::translate(glm::dmat4(1), glm::dvec3{0,+.05,+.05});
						physics->parentJointPoint = l_ankle;
						l_foot->SetInitialTransform(physics->parentJointPoint * glm::inverse(physics->localJointPoint), l_lowerleg);
						physics->jointRotationLimitsX = {-glm::radians(20.0f), +glm::radians(20.0f)};
						physics->bounciness = 0;
						physics->friction = 1;
						physics->SetBoxCollider({+.1, +.05, +.15});
						l_foot->generator = [material](RenderableGeometryEntity* entity, Device* device){
							entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
							entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.05, -.15), glm::vec3(+.1, +.05, +.15)});
						};
					}
				}
			}
			
		}
		
	}
	
	void PhysicsUpdate(double deltaTime) {
		std::unordered_map<RenderableGeometryEntity*, std::unordered_map<AnimationKeyFrame::Action, std::vector<std::tuple<float/*keyFrameWeight*/, float/*keyValue*/>>>> animationsByEntityAndAction {};
		for (auto&[name, animation] : animations) if (animation.value > 0) {
			for (auto& key : animation.keys) {
				animationsByEntityAndAction[key.entity][key.action].push_back({animation.value, key.value});
			}
		}
		// Interpolate animated values
		for (auto&[entity, actions] : animationsByEntityAndAction) {
			for (auto&[action, keys] : actions) {
				float totalWeight = 0;
				float value = 0;
				
				for (auto&[keyFrameWeight, keyValue] : keys) {
					value = glm::mix(value, keyValue, keyFrameWeight / (totalWeight + 1));
					totalWeight += keyFrameWeight;
				}
				
				switch(action) {
					case AnimationKeyFrame::Action::TRANSLATION_X:
						entity->physics.Lock()->jointTranslationTarget.x = value;
						break;
					case AnimationKeyFrame::Action::TRANSLATION_Y:
						entity->physics.Lock()->jointTranslationTarget.y = value;
						break;
					case AnimationKeyFrame::Action::TRANSLATION_Z:
						entity->physics.Lock()->jointTranslationTarget.z = value;
						break;
					case AnimationKeyFrame::Action::ROTATION_X:
						entity->physics.Lock()->jointRotationTarget.x = glm::radians(value);
						break;
					case AnimationKeyFrame::Action::ROTATION_Y:
						entity->physics.Lock()->jointRotationTarget.y = glm::radians(value);
						break;
					case AnimationKeyFrame::Action::ROTATION_Z:
						entity->physics.Lock()->jointRotationTarget.z = glm::radians(value);
						break;
					case AnimationKeyFrame::Action::FRICTION:
						entity->physics.Lock()->friction = value;
						break;
				}
			}
		}
		
	}
	
	~Avatar() {
		animations.clear();
		
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

std::recursive_mutex avatarLock;
std::shared_ptr<Avatar> avatar = nullptr;

struct AvatarAnimatorConfig : public v4d::io::ConfigFile {
	CONFIGFILE_STRUCT(AvatarAnimatorConfig)
	void ReadConfig() override {
		std::lock_guard lock(avatarLock);
		if (avatar) {
			avatar->animations.clear();
			ReadFromINI([](const std::string& section, std::vector<ConfLineStream>& configs){
				
				if (section == "") return;
				
				Animation animation {};
				
				for (auto& conf : configs) {
					std::string line = conf.value.str();
					try {
						
						// Entity
						RenderableGeometryEntity* entity = nullptr;
							 if (conf.name == "torso")			entity = avatar->torso.get();
						else if (conf.name == "r_upperleg") 	entity = avatar->r_upperleg.get();
						else if (conf.name == "l_upperleg") 	entity = avatar->l_upperleg.get();
						else if (conf.name == "r_lowerleg") 	entity = avatar->r_lowerleg.get();
						else if (conf.name == "l_lowerleg") 	entity = avatar->l_lowerleg.get();
						else if (conf.name == "r_foot") 		entity = avatar->r_foot.get();
						else if (conf.name == "l_foot") 		entity = avatar->l_foot.get();
						else {
							throw std::runtime_error("");
						}
						
						do {
							AnimationKeyFrame keyFrame;
							keyFrame.entity = entity;
							
							// Action
							std::string action;
							conf.value >> action;
								 if (action == "TRANSLATION_X") keyFrame.action = AnimationKeyFrame::Action::TRANSLATION_X;
							else if (action == "TRANSLATION_Y") keyFrame.action = AnimationKeyFrame::Action::TRANSLATION_Y;
							else if (action == "TRANSLATION_Z") keyFrame.action = AnimationKeyFrame::Action::TRANSLATION_Z;
							else if (action == "ROTATION_X") 	keyFrame.action = AnimationKeyFrame::Action::ROTATION_X;
							else if (action == "ROTATION_Y") 	keyFrame.action = AnimationKeyFrame::Action::ROTATION_Y;
							else if (action == "ROTATION_Z") 	keyFrame.action = AnimationKeyFrame::Action::ROTATION_Z;
							else if (action == "FRICTION") 		keyFrame.action = AnimationKeyFrame::Action::FRICTION;
							else {
								throw std::runtime_error("");
							}
							
							// Value
							conf.value >> keyFrame.value;
							
							animation.keys.push_back(keyFrame);
						} while (!conf.value.eof());
					} catch (...) {
						LOG_ERROR("Error reading animator line: " << line)
					}
				}
				
				avatar->animations[section] = animation;
			});
		}
	}
	void WriteConfig() override {}
};

auto avatarAnimatorConfig = AvatarAnimatorConfig::Instance(V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/animator.ini"), 1000);















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
				
				entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = mat;
				entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-20, -20, -1), glm::vec3(+20, +20, +1)});
				entity->Add_physics(PhysicsInfo::RigidBodyType::STATIC)->SetBoxCollider({20,20,1});
				
			};
		
	}
	
	V4D_MODULE_FUNC(void, AddGameObjectToScene, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		switch (obj->type) {
			case OBJECT_TYPE::Avatar:{
				std::lock_guard lock(avatarLock);
				avatar = std::make_shared<Avatar>(obj->id);
				avatarAnimatorConfig->ReadConfig();
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
				std::lock_guard lock(avatarLock);
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
					
	
					ImGui::Separator();
					ImGui::Separator();
					
					for (auto&[name, animation] : avatar->animations) {
						ImGui::SliderFloat(name.c_str(), &animation.value, 0.0f, 1.0f);
					}
					
				}
			ImGui::End();
		#endif
	}
	
	V4D_MODULE_FUNC(void, PhysicsUpdate, double deltaTime) {
		std::lock_guard lock(avatarLock);
		if (avatar) avatar->PhysicsUpdate(deltaTime);
	}
	
};
