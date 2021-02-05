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


///////////////////////////////

struct ActionKey {
	RenderableGeometryEntity* entity = nullptr;
	float value = 0;
	enum class Key {NONE = 0, TRANSLATION_X, TRANSLATION_Y, TRANSLATION_Z, ROTATION_X, ROTATION_Y, ROTATION_Z, FRICTION} key;
};

struct AnimatorAction {
	float value = 0;
	std::vector<ActionKey> keys {};
	
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

// struct AnimationKeyFrame {
// 	double start;
// 	double end;
// 	float from;
// 	float to;
// 	enum class Interpolation {INSTANT, LINEAR, SMOOTH} interpolation;
// };

struct Animation {
	// using AnimationMap = std::unordered_map<AnimatorAction*, std::map<double/*time*/, float/*value*/>>;
	// AnimationMap startKeyFrames {};
	// AnimationMap loopKeyFrames {};
	// AnimationMap endKeyFrames {};
	// double startDuration = 0;
	// double loopStart = 0;
	// double endDuration = 0;
	
	void Start() {
		startTime = v4d::Timer::GetCurrentTimestamp();
		endTime = 0;
	}
	void End() {
		startTime = 0;
		endTime = v4d::Timer::GetCurrentTimestamp();
	}
	
	double startTime = 0;
	double endTime = 0;
	
	float speed = 1.0;
	
	// void Run(const AnimationMap& animationMap, double time, bool loop = false) {
	// 	for (auto&[action, keyFrames] : animationMap) {
	// 		double curTime = loop? (time % std::prev(keyFrames.end())->first) : time;
	// 		for (auto&[t, value] : keyFrames) {
	// 			if (curTime <= t) {
	// 				float w = glm::clamp(glm::smoothstep(1.0, 0.01, t - curTime) + 0.1, 0.0f, 1.0f); // my own magic mix interpolant
	// 				action->value = glm::mix(action->value, keyFrameValue, w);
	// 				break;
	// 			}
	// 		}
	// 	}
	// }
	
	void MayRun(double currentTimestamp) {
		// if (startTime > 0) {
		// 	double animationTime = (currentTimestamp - startTime) * speed;
		// 	if (animationTime <= startDuration) {
		// 		Run(startKeyFrames, animationTime);
		// 	}
		// 	if (animationTime >= loopStart) {
		// 		Run(loopKeyFrames, animationTime, true);
		// 	}
		// } else if (endTime > 0) {
		// 	double animationTime = (currentTimestamp - endTime) * speed;
		// 	if (animationTime <= endDuration) {
		// 		Run(endKeyFrames, animationTime);
		// 	} else {
		// 		endTime = 0;
		// 	}
		// }
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
	
	const glm::dmat4 spine = glm::translate(glm::dmat4(1), glm::dvec3{0,+.15, 0});
	const glm::dmat4 r_hip = glm::translate(glm::dmat4(1), glm::dvec3{+.2,-.15, 0});
	const glm::dmat4 r_knee = glm::translate(glm::dmat4(1), glm::dvec3{0,-.2, 0});
	const glm::dmat4 r_ankle = glm::translate(glm::dmat4(1), glm::dvec3{0,-.2, 0});
	const glm::dmat4 l_hip = glm::translate(glm::dmat4(1), glm::dvec3{-.2,-.15, 0});
	const glm::dmat4 l_knee = glm::translate(glm::dmat4(1), glm::dvec3{0,-.2, 0});
	const glm::dmat4 l_ankle = glm::translate(glm::dmat4(1), glm::dvec3{0,-.2, 0});
	
	std::unordered_map<std::string, AnimatorAction> actions {};
	std::unordered_map<std::string, Animation> animations {};
	
	Avatar(v4d::scene::NetworkGameObject::Id objId) {
		RenderableGeometryEntity::Material material {};{
			material.visibility.roughness = 127;
			material.visibility.metallic = 0;
			material.visibility.baseColor = {255,255,255,255};
		}
		
		{// Create Renderable Entities
			root = RenderableGeometryEntity::Create(THIS_MODULE, objId);
			auto rootPhysics = root->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC);
			rootPhysics->angularFactor = {0,0,0};
			root->generator = [material](RenderableGeometryEntity* entity, Device* device){
				entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
				entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.2, -.15, -.2), glm::vec3(+.2, +.15, +.2)});
			};
		
			// Torso
			{
				torso = RenderableGeometryEntity::Create(THIS_MODULE, objId);
				auto physics = torso->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC);
				physics->jointParent = rootPhysics->uniqueId;
				physics->localJointPoint = glm::translate(glm::dmat4(1), glm::dvec3{0,-.2,0});
				physics->parentJointPoint = spine;
				physics->jointMotor = true;
				torso->SetInitialTransform(physics->parentJointPoint * glm::inverse(physics->localJointPoint), root);
				torso->generator = [material](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.3, -.2, -.2), glm::vec3(+.3, +.2, +.2)});
				};
			}
			
			// Right leg
			{
				r_upperleg = RenderableGeometryEntity::Create(THIS_MODULE, objId);
				auto rightUpperLegPhysics = r_upperleg->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC);
				rightUpperLegPhysics->jointParent = rootPhysics->uniqueId;
				rightUpperLegPhysics->localJointPoint = glm::translate(glm::dmat4(1), glm::dvec3{0,+.2,0});
				rightUpperLegPhysics->parentJointPoint = r_hip;
				rightUpperLegPhysics->jointMotor = true;
				r_upperleg->SetInitialTransform(rightUpperLegPhysics->parentJointPoint * glm::inverse(rightUpperLegPhysics->localJointPoint), root);
				r_upperleg->generator = [material](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.2, -.1), glm::vec3(+.1, +.2, +.1)});
				};
				{
					r_lowerleg = RenderableGeometryEntity::Create(THIS_MODULE, objId);
					auto rightLowerLegPhysics = r_lowerleg->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC);
					rightLowerLegPhysics->jointParent = rightUpperLegPhysics->uniqueId;
					rightLowerLegPhysics->localJointPoint = glm::translate(glm::dmat4(1), glm::dvec3{0,+.2,0});
					rightLowerLegPhysics->parentJointPoint = r_knee;
					rightLowerLegPhysics->jointMotor = true;
					r_lowerleg->SetInitialTransform(rightLowerLegPhysics->parentJointPoint * glm::inverse(rightLowerLegPhysics->localJointPoint), r_upperleg);
					r_lowerleg->generator = [material](RenderableGeometryEntity* entity, Device* device){
						entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
						entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.2, -.1), glm::vec3(+.1, +.2, +.1)});
					};
					{
						r_foot = RenderableGeometryEntity::Create(THIS_MODULE, objId);
						auto physics = r_foot->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC);
						physics->jointParent = rightLowerLegPhysics->uniqueId;
						physics->localJointPoint = glm::translate(glm::dmat4(1), glm::dvec3{0,+.05,+.05});
						physics->parentJointPoint = r_ankle;
						physics->jointMotor = true;
						r_foot->SetInitialTransform(physics->parentJointPoint * glm::inverse(physics->localJointPoint), r_lowerleg);
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
				auto leftUpperLegPhysics = l_upperleg->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC);
				leftUpperLegPhysics->jointParent = rootPhysics->uniqueId;
				leftUpperLegPhysics->localJointPoint = glm::translate(glm::dmat4(1), glm::dvec3{0,+.2,0});
				leftUpperLegPhysics->parentJointPoint = l_hip;
				leftUpperLegPhysics->jointMotor = true;
				l_upperleg->SetInitialTransform(leftUpperLegPhysics->parentJointPoint * glm::inverse(leftUpperLegPhysics->localJointPoint), root);
				l_upperleg->generator = [material](RenderableGeometryEntity* entity, Device* device){
					entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
					entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.2, -.1), glm::vec3(+.1, +.2, +.1)});
				};
				{
					l_lowerleg = RenderableGeometryEntity::Create(THIS_MODULE, objId);
					auto leftLowerLegPhysics = l_lowerleg->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC);
					leftLowerLegPhysics->jointParent = leftUpperLegPhysics->uniqueId;
					leftLowerLegPhysics->localJointPoint = glm::translate(glm::dmat4(1), glm::dvec3{0,+.2,0});
					leftLowerLegPhysics->parentJointPoint = l_knee;
					leftLowerLegPhysics->jointMotor = true;
					l_lowerleg->SetInitialTransform(leftLowerLegPhysics->parentJointPoint * glm::inverse(leftLowerLegPhysics->localJointPoint), l_upperleg);
					l_lowerleg->generator = [material](RenderableGeometryEntity* entity, Device* device){
						entity->Allocate(device, "V4D_raytracing:aabb_cube")->material = material;
						entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-.1, -.2, -.1), glm::vec3(+.1, +.2, +.1)});
					};
					{
						l_foot = RenderableGeometryEntity::Create(THIS_MODULE, objId);
						auto physics = l_foot->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC);
						physics->jointParent = leftLowerLegPhysics->uniqueId;
						physics->localJointPoint = glm::translate(glm::dmat4(1), glm::dvec3{0,+.05,+.05});
						physics->parentJointPoint = l_ankle;
						physics->jointMotor = true;
						l_foot->SetInitialTransform(physics->parentJointPoint * glm::inverse(physics->localJointPoint), l_lowerleg);
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
		// Run animations
		double currentTimestamp = v4d::Timer::GetCurrentTimestamp();
		for (auto&[name, animation] : animations) {
			animation.MayRun(currentTimestamp);
		}
		
		// Prepare actions
		std::unordered_map<RenderableGeometryEntity*, std::unordered_map<ActionKey::Key, std::vector<std::tuple<float/*keyWeight*/, float/*keyValue*/>>>> animationsByEntityAndKey {};
		for (auto&[name, action] : actions) if (action.value >= 0) {
			for (auto& key : action.keys) {
				animationsByEntityAndKey[key.entity][key.key].push_back({action.value, key.value});
			}
		}
		
		// Interpolate actions
		for (auto&[entity, actions] : animationsByEntityAndKey) {
			for (auto&[action, keys] : actions) {
				float totalWeight = 0;
				float value = 0;
				
				for (auto&[keyWeight, keyValue] : keys) {
					value = glm::mix(value, keyValue, keyWeight / (totalWeight + 1));
					totalWeight += keyWeight;
				}
				
				auto physics = entity->physics.Lock();
				
				switch(action) {
					case ActionKey::Key::TRANSLATION_X:
						physics->jointTranslationTarget.x = value;
						physics->jointIsDirty = true;
						break;
					case ActionKey::Key::TRANSLATION_Y:
						physics->jointTranslationTarget.y = value;
						physics->jointIsDirty = true;
						break;
					case ActionKey::Key::TRANSLATION_Z:
						physics->jointTranslationTarget.z = value;
						physics->jointIsDirty = true;
						break;
					case ActionKey::Key::ROTATION_X:
						physics->jointRotationTarget.x = glm::radians(value);
						physics->jointIsDirty = true;
						break;
					case ActionKey::Key::ROTATION_Y:
						physics->jointRotationTarget.y = glm::radians(value);
						physics->jointIsDirty = true;
						break;
					case ActionKey::Key::ROTATION_Z:
						physics->jointRotationTarget.z = glm::radians(value);
						physics->jointIsDirty = true;
						break;
					case ActionKey::Key::FRICTION:
						physics->friction = value;
						break;
				}
			}
		}
	}
	
	~Avatar() {
		animations.clear();
		actions.clear();
		
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

struct AvatarConfigFile : public v4d::io::ConfigFile {
	CONFIGFILE_STRUCT(AvatarConfigFile)
	void ReadConfig() override {
		std::lock_guard lock(avatarLock);
		if (avatar) {
			avatar->animations.clear();
			avatar->actions.clear();
			ReadFromINI([](std::stringstream section, std::vector<ConfLineStream>& configs){
				
				if (section.eof()) return;
				
				std::string type;
				section >> type;
				
				auto getEntityByName = [](const std::string& name) -> RenderableGeometryEntity* {
						 if (name == "root")		return avatar->root.get();
					else if (name == "torso")		return avatar->torso.get();
					else if (name == "r_upperleg") 	return avatar->r_upperleg.get();
					else if (name == "l_upperleg") 	return avatar->l_upperleg.get();
					else if (name == "r_lowerleg") 	return avatar->r_lowerleg.get();
					else if (name == "l_lowerleg") 	return avatar->l_lowerleg.get();
					else if (name == "r_foot") 		return avatar->r_foot.get();
					else if (name == "l_foot") 		return avatar->l_foot.get();
					return nullptr;
				};
				
				if (type == "") {
					//void
				}
				else if (type == "OBJECT") {
					if (section.eof()) return;
					
					std::string name;
					section >> name;
					
					// Entity
					RenderableGeometryEntity* entity = getEntityByName(name);
					if (!entity) throw std::runtime_error("");
					auto physics = entity->physics.Lock();
					
					for (auto& conf : configs) {
						std::string line = conf.value.str();
						try {
							std::string param;
							conf.name >> param;
						
							if (param == "MASS") {
								conf.value >> physics->mass;
							} else if (param == "COLLIDER") {
								std::string type;
								conf.value >> type;
								if (type == "BOX") {
									float x,y,z;
									conf.value >> x >> y >> z;
									physics->SetBoxCollider({x,y,z});
								} else if (type == "SPHERE") {
									float radius;
									conf.value >> radius;
									physics->SetSphereCollider(radius);
								} else {
									//TODO mesh collider
								}
							} else if (param == "FRICTION") {
								conf.value >> physics->friction;
							} else if (param == "BOUNCINESS") {
								conf.value >> physics->bounciness;
							} else if (param == "ANGULAR_FACTOR") {
								conf.value >> physics->angularFactor.x >> physics->angularFactor.y >> physics->angularFactor.z;
							} else if (param == "ANGULAR_DAMPING") {
								conf.value >> physics->angularDamping;
								physics->physicsDirty = true;
							} else if (param == "JOINT_TRANSLATION_X") {
								conf.value >> physics->jointTranslationLimitsX.min >> physics->jointTranslationLimitsX.max;
								conf.value >> physics->jointTranslationMaxForce.x;
								conf.value >> physics->jointTranslationVelocity.x;
								physics->jointIsDirty = true;
							} else if (param == "JOINT_TRANSLATION_Y") {
								conf.value >> physics->jointTranslationLimitsY.min >> physics->jointTranslationLimitsY.max;
								conf.value >> physics->jointTranslationMaxForce.y;
								conf.value >> physics->jointTranslationVelocity.y;
								physics->jointIsDirty = true;
							} else if (param == "JOINT_TRANSLATION_Z") {
								conf.value >> physics->jointTranslationLimitsZ.min >> physics->jointTranslationLimitsZ.max;
								conf.value >> physics->jointTranslationMaxForce.z;
								conf.value >> physics->jointTranslationVelocity.z;
								physics->jointIsDirty = true;
							} else if (param == "JOINT_ROTATION_X") {
								float min, max;
								conf.value >> min >> max;
								physics->jointRotationLimitsX = {glm::radians(min), glm::radians(max)};
								conf.value >> physics->jointRotationMaxForce.x;
								conf.value >> physics->jointRotationVelocity.x;
								physics->jointIsDirty = true;
							} else if (param == "JOINT_ROTATION_Y") {
								float min, max;
								conf.value >> min >> max;
								physics->jointRotationLimitsY = {glm::radians(min), glm::radians(max)};
								conf.value >> physics->jointRotationMaxForce.y;
								conf.value >> physics->jointRotationVelocity.y;
								physics->jointIsDirty = true;
							} else if (param == "JOINT_ROTATION_Z") {
								float min, max;
								conf.value >> min >> max;
								physics->jointRotationLimitsZ = {glm::radians(min), glm::radians(max)};
								conf.value >> physics->jointRotationMaxForce.z;
								conf.value >> physics->jointRotationVelocity.z;
								physics->jointIsDirty = true;
							} else {
								throw std::runtime_error("");
							}
								
						} catch (...) {
							LOG_ERROR("Error reading object line: " << line)
						}
					}
					
					
				}
				else if (type == "ACTION") {
					if (section.eof()) return;
					
					std::string actionName;
					section >> actionName;
					
					AnimatorAction action {};
					
					for (auto& conf : configs) {
						std::string line = conf.value.str();
						try {
							std::string name;
							conf.name >> name;
							
							// Entity
							RenderableGeometryEntity* entity = getEntityByName(name);
							if (!entity) throw std::runtime_error("");
							
							do {
								ActionKey actionKey;
								actionKey.entity = entity;
								
								// Key
								std::string key;
								conf.value >> key;
									 if (key == "TRANSLATION_X") actionKey.key = ActionKey::Key::TRANSLATION_X;
								else if (key == "TRANSLATION_Y") actionKey.key = ActionKey::Key::TRANSLATION_Y;
								else if (key == "TRANSLATION_Z") actionKey.key = ActionKey::Key::TRANSLATION_Z;
								else if (key == "ROTATION_X") 	actionKey.key = ActionKey::Key::ROTATION_X;
								else if (key == "ROTATION_Y") 	actionKey.key = ActionKey::Key::ROTATION_Y;
								else if (key == "ROTATION_Z") 	actionKey.key = ActionKey::Key::ROTATION_Z;
								else if (key == "FRICTION") 	actionKey.key = ActionKey::Key::FRICTION;
								else {
									throw std::runtime_error("");
								}
								
								// Value
								conf.value >> actionKey.value;
								
								action.keys.push_back(actionKey);
							} while (!conf.value.eof());
						} catch (...) {
							LOG_ERROR("Error reading action line: " << line)
						}
					}
					
					avatar->actions[actionName] = action;
				}
				else if (type == "ANIMATION") {
					
				}
				
			});
		}
	}
	void WriteConfig() override {}
};

auto avatarConfig = AvatarConfigFile::Instance(V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/avatar.ini"), 1000);


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
				avatarConfig->ReadConfig();
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
					
					{ImGui::Text("JOINTS");
						
						auto testJoints = [](std::string name, std::shared_ptr<v4d::graphics::RenderableGeometryEntity> entity){
							if (entity) {
								if (std::unique_lock<std::recursive_mutex> l = entity->GetLock(); l) {
									if (auto physics = entity->physics.Lock(); physics) {
										ImGui::Separator();
										
										if (physics->jointTranslationLimitsX.min < physics->jointTranslationLimitsX.max) {
											if (ImGui::SliderFloat((name + " Translation X").c_str(), &physics->jointTranslationTarget.x, physics->jointTranslationLimitsX.min, physics->jointTranslationLimitsX.max)) {
												physics->jointIsDirty = true;
											}
										}
										if (physics->jointTranslationLimitsY.min < physics->jointTranslationLimitsY.max) {
											if (ImGui::SliderFloat((name + " Translation Y").c_str(), &physics->jointTranslationTarget.y, physics->jointTranslationLimitsY.min, physics->jointTranslationLimitsY.max)) {
												physics->jointIsDirty = true;
											}
										}
										if (physics->jointTranslationLimitsZ.min < physics->jointTranslationLimitsZ.max) {
											if (ImGui::SliderFloat((name + " Translation Z").c_str(), &physics->jointTranslationTarget.z, physics->jointTranslationLimitsZ.min, physics->jointTranslationLimitsZ.max)) {
												physics->jointIsDirty = true;
											}
										}
										
										if (physics->jointRotationLimitsX.min < physics->jointRotationLimitsX.max) {
											if (ImGui::SliderFloat((name + " Rotation X").c_str(), &physics->jointRotationTarget.x, physics->jointRotationLimitsX.min, physics->jointRotationLimitsX.max)) {
												physics->jointIsDirty = true;
											}
										}
										if (physics->jointRotationLimitsY.min < physics->jointRotationLimitsY.max) {
											if (ImGui::SliderFloat((name + " Rotation Y").c_str(), &physics->jointRotationTarget.y, physics->jointRotationLimitsY.min, physics->jointRotationLimitsY.max)) {
												physics->jointIsDirty = true;
											}
										}
										if (physics->jointRotationLimitsZ.min < physics->jointRotationLimitsZ.max) {
											if (ImGui::SliderFloat((name + " Rotation Z").c_str(), &physics->jointRotationTarget.z, physics->jointRotationLimitsZ.min, physics->jointRotationLimitsZ.max)) {
												physics->jointIsDirty = true;
											}
										}
									}
								}
							}
						};
						
						testJoints("Torso", avatar->torso);
						testJoints("Right upper leg", avatar->r_upperleg);
						testJoints("Left upper leg", avatar->l_upperleg);
						testJoints("Right lower leg", avatar->r_lowerleg);
						testJoints("Left lower leg", avatar->l_lowerleg);
						testJoints("Right foot", avatar->r_foot);
						testJoints("Left foot", avatar->l_foot);
					}
	
					ImGui::Separator();
					
					{ImGui::Text("ACTIONS");
						for (auto&[name, action] : avatar->actions) {
							ImGui::SliderFloat(name.c_str(), &action.value, -0.1f, 1.0f);
						}
					}
					
					ImGui::Separator();
					
					{ImGui::Text("ANIMATIONS");
						float posY = ImGui::GetCursorPosY();
						for (auto&[name, animation] : avatar->animations) {
							ImGui::SetCursorPos({5, posY});
							ImGui::SetNextItemWidth(90);
							if (animation.startTime == 0) {
								if (ImGui::Button((std::string("> ") + name).c_str())) animation.Start();
							} else {
								if (ImGui::Button((std::string("X ") + name).c_str())) animation.End();
							}
							ImGui::SetCursorPos({100, posY});
							ImGui::SetNextItemWidth(80);
							ImGui::SliderFloat((name + " speed").c_str(), &animation.speed, 0.2f, 5.0f);
							posY += 20;
						}
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
