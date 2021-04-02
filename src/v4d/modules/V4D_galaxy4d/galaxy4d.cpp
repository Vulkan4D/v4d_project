#include <v4d.h>
#include <V4D_Mod.h>
#include "v4d/game/Entity.h"

#include "utilities/io/Logger.h"
#include "utilities/scene/GltfModelLoader.h"

#include <unordered_set>

#include "common.hh"
#include "actions.hh"
#include "../V4D_flycam/common.hh"
#include "../V4D_raytracing/camera_options.hh"

using namespace v4d::scene;
using namespace v4d::networking;
using namespace networking::action;
using namespace v4d::modular;
using namespace v4d::graphics;
using namespace v4d::graphics::Mesh;

GltfModelLoader cake {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/cake.glb")};

V4D_Mod* mainRenderModule = nullptr;
V4D_Mod* mainMultiplayerModule = nullptr;
std::shared_ptr<ListeningServer> server = nullptr;
std::shared_ptr<OutgoingConnection> client = nullptr;

std::recursive_mutex clientActionQueueMutex;
std::queue<v4d::data::Stream> clientActionQueue {};
PlayerView* playerView = nullptr;

Scene* scene = nullptr;
Renderer* r = nullptr;

bool playerVisible = true;

// struct TerrainDigSphere {
// 	float radius;
// 	void operator() (RenderableGeometryEntity* entity, Device* device){
// 		RenderableGeometryEntity::Material mat {};
// 		mat.visibility.roughness = 254;
// 		mat.visibility.baseColor = {255,255,255,255};
// 		entity->Allocate(device, "V4D_galaxy4d:terrain.dig.sphere")->material = mat;
// 		entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
// 	}
// };

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
	}
	
	V4D_MODULE_FUNC(void, InitClient, std::shared_ptr<OutgoingConnection> _c) {
		client = _c;
	}
	
	V4D_MODULE_FUNC(void, LoadScene, Scene* _s) {
		scene = _s;
	}
	
	V4D_MODULE_FUNC(void, ServerReceiveAction, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		auto action = stream->Read<Action>();
		switch (action) {
			
			case TEST_OBJ:{
				auto key = stream->Read<std::string>();
				
				if (key == "ball") {
					auto dir = stream->Read<glm::vec3>();
					// Launch ball
					ServerSidePlayer::Ptr player;
					ServerSideEntity::Ptr playerEntity;
					if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
						ServerSideEntity::Ptr ball = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::Ball, playerEntity->referenceFrame, playerEntity->referenceFrameExtra);
						ball->position = playerEntity->position + glm::dvec3{dir.x, dir.y, dir.z} * 4.0;
						auto rigidbody = ball->Add_rigidbody(Rigidbody::SphereInertia(1.0/*mass*/, 0.5/*radius*/));
						rigidbody->boundingRadius = 0.5;
						ball->colliders.emplace_back(rigidbody->boundingRadius);
						if (auto playerRigidbody = playerEntity->rigidbody.Lock(); playerRigidbody) {
							rigidbody->linearVelocity += playerRigidbody->linearVelocity;
						}
						rigidbody->ApplyForce(dir*500.0f /*, {-0.1,0,0.1}*/ );
						ball->isDynamic = true;
						ball->Activate();
					}
				}
				else if (key == "glassBall") {
					auto dir = stream->Read<glm::vec3>();
					// Launch glass ball
					ServerSidePlayer::Ptr player;
					ServerSideEntity::Ptr playerEntity;
					if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
						ServerSideEntity::Ptr ball = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::GlassBall, playerEntity->referenceFrame, playerEntity->referenceFrameExtra);
						ball->position = playerEntity->position + glm::dvec3{dir.x, dir.y, dir.z} * 4.0;
						auto rigidbody = ball->Add_rigidbody(Rigidbody::SphereInertia(0.2/*mass*/, 0.5/*radius*/));
						rigidbody->boundingRadius = 0.5;
						ball->colliders.emplace_back(rigidbody->boundingRadius);
						if (auto playerRigidbody = playerEntity->rigidbody.Lock(); playerRigidbody) {
							rigidbody->linearVelocity += playerRigidbody->linearVelocity;
						}
						ball->isDynamic = true;
						ball->Activate();
					}
				}
				// else if (key == "TerrainDigSphere") {
				// 	auto dir = stream->Read<glm::vec3>();
				// 	float radius = stream->Read<float>();
				// 	ServerSidePlayer::Ptr player;
				// ServerSideEntity::Ptr playerEntity;
				// if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
				// 	glm::dvec3 pos = glm::dvec3{dir.x, dir.y, dir.z} * double(radius)/2.0;
				// 	auto transform = glm::translate(playerObj->GetTransform(), pos);
				// 	glm::i32vec4 sphereHash = glm::i32vec4{glm::round(transform[3].x*20), glm::round(transform[3].y*20), glm::round(transform[3].z*20), glm::round(radius)};
				// 	static std::unordered_set<glm::i32vec4> existingSpheres {};
				// 	if (!existingSpheres.count(sphereHash)) {
				// 		existingSpheres.emplace(sphereHash);
				// 		// Dig in terrain
				// 		auto digSphere = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::TerrainDigSphere, playerEntity->referenceFrame, playerEntity->referenceFrameExtra);
				// 		digSphere->entityData = radius;
				// 		digSphere->SetTransform(transform);
				// 		digSphere->isDynamic = false;
				// 	}
				// }
				// }
				else if (key == "balls") {
					auto dir = stream->Read<glm::vec3>();
					// // Launch 10 balls
					// ServerSidePlayer::Ptr player;
					// ServerSideEntity::Ptr playerEntity;
					// if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
					// 	glm::dvec3 playerVelocity {0,0,0};
					// 	if (auto playerRigidbody = playerEntity->rigidbody.Lock(); playerRigidbody) {
					// 		playerVelocity = playerRigidbody->linearVelocity;
					// 	}
					// 	for (int i = 0; i < 10; ++i) {
					// 		ServerSideEntity::Ptr ball = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::Ball, playerEntity->referenceFrame, playerEntity->referenceFrameExtra);
					// 		ball->position = playerEntity->position + glm::dvec3{dir.x, dir.y, dir.z} * 4.0;
					// 		auto rigidbody = ball->Add_rigidbody(Rigidbody::SphereInertia(1.0/*mass*/, 0.5/*radius*/));
					// 		rigidbody->boundingRadius = 0.5;
					// 		rigidbody->linearVelocity += playerVelocity;
					// 		ball->isDynamic = true;
					// 		ball->Activate();
					// 	}
					// }
				}
				else if (key == "light") {
					auto dir = stream->Read<glm::vec3>();
					// Launch light
					ServerSidePlayer::Ptr player;
					ServerSideEntity::Ptr playerEntity;
					if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
						ServerSideEntity::Ptr ball = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::Light, playerEntity->referenceFrame, playerEntity->referenceFrameExtra);
						ball->position = playerEntity->position + glm::dvec3{dir.x, dir.y, dir.z} * 5.0;
						auto rigidbody = ball->Add_rigidbody(Rigidbody::SphereInertia(5.0/*mass*/, 2.0/*radius*/));
						rigidbody->boundingRadius = 2.0;
						ball->colliders.emplace_back(rigidbody->boundingRadius);
						if (auto playerRigidbody = playerEntity->rigidbody.Lock(); playerRigidbody) {
							rigidbody->linearVelocity += playerRigidbody->linearVelocity;
						}
						ball->isDynamic = true;
						ball->Activate();
					}
				}
				else if (key == "drone") {
					auto dir = stream->Read<glm::vec3>();
					// Launch drone
					ServerSidePlayer::Ptr player;
					ServerSideEntity::Ptr playerEntity;
					if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
						auto drone = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::Drone, playerEntity->referenceFrame, playerEntity->referenceFrameExtra);
						drone->position = playerEntity->position + glm::dvec3{dir.x, dir.y, dir.z} * 5.0;
						drone->isDynamic = false;
						drone->Activate();
					}
				}
				else if (key == "glass") {
					auto dir = stream->Read<glm::vec3>();
					ServerSidePlayer::Ptr player;
					ServerSideEntity::Ptr playerEntity;
					if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
						auto drone = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::Glass, playerEntity->referenceFrame, playerEntity->referenceFrameExtra);
						drone->position = playerEntity->position + glm::dvec3{dir.x, dir.y, dir.z} * 5.0;
						drone->isDynamic = false;
						drone->Activate();
					}
				}
				else if (key == "clear") {
					ServerSidePlayer::Ptr player;
					ServerSideEntity::Ptr playerEntity;
					if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
						ServerSideEntity::ForEach([&playerEntity](ServerSideEntity::Ptr entity){
							if (playerEntity->GetID() != entity->GetID()) {
								entity->active = false;
							}
						});
					}
				}
			}break;
		
			default: 
				LOG_ERROR("Server ReceiveAction UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	// V4D_MODULE_FUNC(void, OnCollisionHit, int64_t entityUniqueID, uint64_t type, glm::dvec3 contactPoint, double contactSpeed) {
	// 	LOG("Collision hit " << entityUniqueID << ": contactSpeed: " << contactSpeed)
	// }
	
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
	
	#pragma endregion
	
	#pragma region Rendering
	
	V4D_MODULE_FUNC(void, ConfigureShaders) {
		mainRenderModule->AddRayTracingHitShader(THIS_MODULE, "terrain.dig.sphere");
	}
	
	V4D_MODULE_FUNC(void, DrawUi2) {
		#ifdef _ENABLE_IMGUI
			ImGui::Checkbox("Player visible", &playerVisible);
			
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
	
	V4D_MODULE_FUNC(void, CreateEntity, int64_t entityUniqueID, uint64_t type) {
		ClientSideEntity::Ptr entity = ClientSideEntity::Get(entityUniqueID);
		if (!entity) return;
		switch (type) {
			case OBJECT_TYPE::Player:{
				auto renderableEntity = RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
				entity->renderableGeometryEntityInstance = renderableEntity;
				renderableEntity->generator = cake;
				renderableEntity->Remove_physics();
			}break;
			case OBJECT_TYPE::Ball:{
				auto renderableEntity = RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
				entity->renderableGeometryEntityInstance = renderableEntity;
				float radius = 0.5f;
				// auto physics = renderableEntity->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 1.0f);
				// physics->SetSphereCollider(radius);
				// physics->friction = 0.6;
				// physics->bounciness = 0.7;
				renderableEntity->generator = [radius](RenderableGeometryEntity* renderableEntity, Device* device){
					RenderableGeometryEntity::Material mat {};
					mat.visibility.textures[0] = Renderer::sbtOffsets["call:tex_checker"];
					mat.visibility.texFactors[0] = 255;
					mat.visibility.roughness = 0;
					mat.visibility.metallic = 1;
					renderableEntity->Allocate(device, "V4D_raytracing:aabb_sphere")->material = mat;
					renderableEntity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
					renderableEntity->Add_meshVertexColorU8()->AllocateBuffers(device, {127,127,127,255});
				};
			}break;
			
			case OBJECT_TYPE::GlassBall:{
				auto renderableEntity = RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
				entity->renderableGeometryEntityInstance = renderableEntity;
				float radius = 0.5f;
				// auto physics = renderableEntity->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 1.0f);
				// physics->SetSphereCollider(radius);
				// physics->friction = 0.6;
				// physics->bounciness = 0.7;
				renderableEntity->generator = [radius](RenderableGeometryEntity* renderableEntity, Device* device){
					RenderableGeometryEntity::Material mat {};
					mat.visibility.indexOfRefraction = 1.1 * 50;
					mat.visibility.roughness = 0;
					renderableEntity->Allocate(device, "V4D_raytracing:aabb_sphere")->material = mat;
					renderableEntity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
					renderableEntity->Add_meshVertexColorU8()->AllocateBuffers(device, {255,255,255,16});
				};
			}break;
			case OBJECT_TYPE::Light:{
				auto renderableEntity = RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
				entity->renderableGeometryEntityInstance = renderableEntity;
				float radius = 2;
				float intensity = 100000.0f;
				// auto physics = renderableEntity->Add_physics(PhysicsInfo::RigidBodyType::DYNAMIC, 5.0f);
				// physics->SetSphereCollider(radius);
				// physics->friction = 0.6;
				// physics->bounciness = 0.7;
				renderableEntity->generator = [radius,intensity](RenderableGeometryEntity* renderableEntity, Device* device){
					renderableEntity->Allocate(device, "V4D_raytracing:aabb_sphere.light")->material.visibility.emission = intensity;
					renderableEntity->rayTracingMask = RAY_TRACED_ENTITY_LIGHT;
					renderableEntity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
					renderableEntity->Add_meshVertexColorU8()->AllocateBuffers(device, {255,255,255,255});
					renderableEntity->Add_lightSource(glm::vec3{0,0,0}, glm::vec3{1}, radius, intensity);
				};
			}break;
			case OBJECT_TYPE::Drone:{
				auto renderableEntity = RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
				entity->renderableGeometryEntityInstance = renderableEntity;
				renderableEntity->generator = cake;
			}break;
			case OBJECT_TYPE::Glass:{
				auto renderableEntity = RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
				entity->renderableGeometryEntityInstance = renderableEntity;
				renderableEntity->Add_physics(PhysicsInfo::RigidBodyType::STATIC, 1.0f)->SetBoxCollider({2.0f, 2.0f, 0.01f});
				renderableEntity->generator = [](RenderableGeometryEntity* renderableEntity, Device* device){
					RenderableGeometryEntity::Material mat {};
					mat.visibility.indexOfRefraction = 1.02 * 50;
					mat.visibility.roughness = 0;
					renderableEntity->Allocate(device, "V4D_raytracing:default")->material = mat;
					renderableEntity->Add_meshVertexPosition()->AllocateBuffersFromList(device, {
						{-2.0,-2.0, 0.0},
						{ 2.0,-2.0, 0.0},
						{ 2.0, 2.0, 0.0},
						{-2.0, 2.0, 0.0},
					});
					renderableEntity->Add_meshVertexNormal()->AllocateBuffersFromList(device, {
						{ 0.0, 0.0, 1.0},
						{ 0.0, 0.0, 1.0},
						{ 0.0, 0.0, 1.0},
						{ 0.0, 0.0, 1.0},
					});
					renderableEntity->Add_meshVertexColorU8()->AllocateBuffersFromList(device, {
						{255, 255, 255, 48},
						{255, 255, 255, 48},
						{255, 255, 255, 48},
						{255, 255, 255, 48},
					});
					renderableEntity->Add_meshIndices16()->AllocateBuffersFromList(device, {
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
						stream << glm::vec3{playerView->viewForward};
					ClientEnqueueAction(stream);
				}break;
				case GLFW_KEY_H:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("glassBall");
						stream << glm::vec3{playerView->viewForward};
					ClientEnqueueAction(stream);
				}break;
				case GLFW_KEY_N:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("balls");
						stream << glm::vec3{playerView->viewForward};
					ClientEnqueueAction(stream);
				}break;
				case GLFW_KEY_L:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("light");
						stream << glm::vec3{playerView->viewForward};
					ClientEnqueueAction(stream);
				}break;
				case GLFW_KEY_INSERT:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("drone");
						stream << glm::vec3{playerView->viewForward};
					ClientEnqueueAction(stream);
				}break;
				case GLFW_KEY_G:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("glass");
						stream << glm::vec3{playerView->viewForward};
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
