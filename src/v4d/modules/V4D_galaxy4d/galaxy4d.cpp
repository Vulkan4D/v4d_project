#include <v4d.h>
#include <V4D_Mod.h>

#include "v4d/game/Entity.h"
#include "v4d/game/ServerSideEntity.hpp"
#include "v4d/game/ClientSideEntity.hpp"
#include "v4d/game/ServerSidePlayer.hpp"
#include "v4d/game/EntityConfigFile.h"
#include "v4d/game/Collider.hpp"

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
// GltfModelLoader avatar {V4D_MODULE_ASSET_PATH("G4D_models", "resources/avatar.glb")};
auto avatarConfig = EntityConfigFile::Instance(V4D_MODULE_ASSET_PATH("G4D_models", "resources/avatar.ini"));

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
		avatarConfig->Load();
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
						ball->position = playerEntity->position + glm::dvec3{dir.x, dir.y, dir.z} * 2.0;
						auto rigidbody = ball->Add_rigidbody(Rigidbody::SphereInertia(1.0/*mass*/, 0.5/*radius*/));
						rigidbody->boundingRadius = 0.5;
						ball->colliders.emplace("root", std::make_unique<SphereCollider>(rigidbody->boundingRadius));
						if (auto playerRigidbody = playerEntity->rigidbody.Lock(); playerRigidbody) {
							rigidbody->linearVelocity += playerRigidbody->linearVelocity;
						}
						rigidbody->ApplyForce(dir*500.0f /*, {-0.1,0,0.1}*/ );
						ball->SetDynamic();
						ball->Activate();
					}
				}
				
				
				else if (key == "capsule") {
					auto dir = stream->Read<glm::vec3>();
					// Launch capsule
					ServerSidePlayer::Ptr player;
					ServerSideEntity::Ptr playerEntity;
					if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
						ServerSideEntity::Ptr capsule = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::Capsule, playerEntity->referenceFrame, playerEntity->referenceFrameExtra);
						capsule->position = playerEntity->position + glm::dvec3{dir.x, dir.y, dir.z} * 2.0;
						capsule->orientation = playerEntity->orientation;
						auto rigidbody = capsule->Add_rigidbody(Rigidbody::CylinderInertia(1.0/*mass*/, 0.2/*radiusXY*/, 1.0/*heightZ*/));
						rigidbody->boundingRadius = 0.8;
						capsule->colliders.emplace("root", std::make_unique<CapsuleCollider>(glm::dvec3(0), glm::dmat3(1), 1.0/*length*/, 0.2/*radius*/));
						if (auto playerRigidbody = playerEntity->rigidbody.Lock(); playerRigidbody) {
							rigidbody->linearVelocity += playerRigidbody->linearVelocity;
						}
						rigidbody->ApplyForce(dir*500.0f /*, {-0.1,0,0.1}*/ );
						capsule->SetDynamic();
						capsule->Activate();
					}
				}
				else if (key == "cylinder") {
					auto dir = stream->Read<glm::vec3>();
					// Launch cylinder
					ServerSidePlayer::Ptr player;
					ServerSideEntity::Ptr playerEntity;
					if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
						ServerSideEntity::Ptr cylinder = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::Cylinder, playerEntity->referenceFrame, playerEntity->referenceFrameExtra);
						cylinder->position = playerEntity->position + glm::dvec3{dir.x, dir.y, dir.z} * 2.0;
						cylinder->orientation = playerEntity->orientation;
						auto rigidbody = cylinder->Add_rigidbody(Rigidbody::CylinderInertia(1.0/*mass*/, 0.2/*radiusXY*/, 1.0/*heightZ*/));
						rigidbody->boundingRadius = 0.7;
						cylinder->colliders.emplace("root", std::make_unique<CylinderCollider>(glm::dvec3(0), glm::dmat3(1), 1.0/*length*/, 0.2/*radius*/));
						if (auto playerRigidbody = playerEntity->rigidbody.Lock(); playerRigidbody) {
							rigidbody->linearVelocity += playerRigidbody->linearVelocity;
						}
						rigidbody->ApplyForce(dir*500.0f /*, {-0.1,0,0.1}*/ );
						cylinder->SetDynamic();
						cylinder->Activate();
					}
				}
				else if (key == "cone") {
					auto dir = stream->Read<glm::vec3>();
					// Launch cone
					ServerSidePlayer::Ptr player;
					ServerSideEntity::Ptr playerEntity;
					if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
						ServerSideEntity::Ptr cone = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::Cone, playerEntity->referenceFrame, playerEntity->referenceFrameExtra);
						cone->position = playerEntity->position + glm::dvec3{dir.x, dir.y, dir.z} * 2.0;
						cone->orientation = playerEntity->orientation;
						// auto rigidbody = cone->Add_rigidbody(Rigidbody::ConeInertia(1.0/*mass*/, 0.4/*radiusXY*/, 1.0/*heightZ*/));
						// rigidbody->boundingRadius = 0.7;
						// cone->colliders.emplace("root", 1.0/*length*/, 0.4/*radiusA*/, 0.1/*radiusB*/);
						// if (auto playerRigidbody = playerEntity->rigidbody.Lock(); playerRigidbody) {
						// 	rigidbody->linearVelocity += playerRigidbody->linearVelocity;
						// }
						// rigidbody->ApplyForce(dir*500.0f /*, {-0.1,0,0.1}*/ );
						// cone->SetDynamic();
						cone->Activate();
					}
				}
				else if (key == "ring") {
					auto dir = stream->Read<glm::vec3>();
					// Launch ring
					ServerSidePlayer::Ptr player;
					ServerSideEntity::Ptr playerEntity;
					if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
						ServerSideEntity::Ptr ring = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::Ring, playerEntity->referenceFrame, playerEntity->referenceFrameExtra);
						ring->position = playerEntity->position + glm::dvec3{dir.x, dir.y, dir.z} * 4.0;
						ring->orientation = playerEntity->orientation;
						// auto rigidbody = ring->Add_rigidbody(Rigidbody::RingInertia(1.0/*mass*/, 0.7/*innerRadiusXY*/, 0.8/*outerRadiusXY*/, 2.4/*heightZ*/));
						// rigidbody->boundingRadius = 2.0;
						// ring->colliders.emplace("root", 2.4/*length*/, 1.8/*radiusA*/, 0.8/*radiusB*/, 0.02/*thickness*/);
						// if (auto playerRigidbody = playerEntity->rigidbody.Lock(); playerRigidbody) {
						// 	rigidbody->linearVelocity += playerRigidbody->linearVelocity;
						// }
						// rigidbody->ApplyForce(dir*500.0f /*, {-0.1,0,0.1}*/ );
						// ring->SetDynamic();
						ring->Activate();
					}
				}
				
				
				else if (key == "box") {
					auto dir = stream->Read<glm::vec3>();
					// Launch box
					ServerSidePlayer::Ptr player;
					ServerSideEntity::Ptr playerEntity;
					if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
						ServerSideEntity::Ptr box = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::Box, playerEntity->referenceFrame, playerEntity->referenceFrameExtra);
						box->position = playerEntity->position + glm::dvec3{dir.x, dir.y, dir.z} * 10.0;
						box->orientation = playerEntity->orientation;
						auto rigidbody = box->Add_rigidbody(Rigidbody::BoxInertia(10.0/*mass*/, 4,4,2));
						rigidbody->boundingRadius = 3;
						box->colliders.emplace("root", std::make_unique<BoxCollider>(glm::dvec3(0), glm::dmat3(1), glm::dvec3{2.0, 2.0, 1}));
						if (auto playerRigidbody = playerEntity->rigidbody.Lock(); playerRigidbody) {
							rigidbody->linearVelocity += playerRigidbody->linearVelocity;
						}
						// rigidbody->ApplyForce(dir*500.0f /*, {-0.1,0,0.1}*/ );
						box->SetDynamic();
						box->Activate();
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
						ball->colliders.emplace("root", std::make_unique<SphereCollider>(rigidbody->boundingRadius));
						if (auto playerRigidbody = playerEntity->rigidbody.Lock(); playerRigidbody) {
							rigidbody->linearVelocity += playerRigidbody->linearVelocity;
						}
						ball->SetDynamic();
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
				// 		digSphere->SetDynamic(false);
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
					// 		ball->SetDynamic();
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
						ball->colliders.emplace("root", std::make_unique<SphereCollider>(rigidbody->boundingRadius));
						if (auto playerRigidbody = playerEntity->rigidbody.Lock(); playerRigidbody) {
							rigidbody->linearVelocity += playerRigidbody->linearVelocity;
						}
						ball->SetDynamic();
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
						drone->orientation = playerEntity->orientation;
						drone->SetDynamic(false);
						drone->Activate();
					}
				}
				else if (key == "avatar") {
					auto dir = stream->Read<glm::vec3>();
					// Launch avatar
					ServerSidePlayer::Ptr player;
					ServerSideEntity::Ptr playerEntity;
					if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
						auto avatar = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::Avatar, playerEntity->referenceFrame, playerEntity->referenceFrameExtra);
						avatar->position = playerEntity->position + glm::dvec3{dir.x, dir.y, dir.z} * 4.0;
						avatar->orientation = playerEntity->orientation;
						avatar->SetDynamic(false);
						avatar->Activate();
					}
				}
				else if (key == "glass") {
					auto dir = stream->Read<glm::vec3>();
					ServerSidePlayer::Ptr player;
					ServerSideEntity::Ptr playerEntity;
					if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
						auto glass = ServerSideEntity::Create(-1, THIS_MODULE, OBJECT_TYPE::Glass, playerEntity->referenceFrame, playerEntity->referenceFrameExtra);
						glass->position = playerEntity->position + glm::dvec3{dir.x, dir.y, dir.z} * 5.0;
						glass->Add_rigidbody(Rigidbody::BoxInertia(10, 4.0, 4.0, 0.2))->boundingRadius = 2;
						
						// Box collider
						glass->colliders.emplace("root", std::make_unique<BoxCollider>(glm::dvec3(0), glm::dmat3(1), glm::dvec3{+2.0,+2.0, 0.1}));
						
						// // Triangle colliders
						// glass->colliders.emplace_back(glm::dvec3{-2.0,-2.0, 0.0}, glm::dvec3{ 2.0,-2.0, 0.0}, glm::dvec3{ 2.0, 2.0, 0.0}, glm::dvec3{0,0,0});
						// glass->colliders.emplace_back(glm::dvec3{ 2.0, 2.0, 0.0}, glm::dvec3{-2.0, 2.0, 0.0}, glm::dvec3{-2.0,-2.0, 0.0}, glm::dvec3{0,0,0});
						
						// // Sphere colliders
						// glass->colliders.emplace_back(glm::dvec3{-2.0,-2.0, 0.0}, 0.2);
						// glass->colliders.emplace_back(glm::dvec3{+2.0,-2.0, 0.0}, 0.2);
						// glass->colliders.emplace_back(glm::dvec3{+2.0,+2.0, 0.0}, 0.2);
						// glass->colliders.emplace_back(glm::dvec3{-2.0,+2.0, 0.0}, 0.2);
						
						glass->SetDynamic();
						glass->Activate();
					}
				}
				else if (key == "clear") {
					ServerSidePlayer::Ptr player;
					ServerSideEntity::Ptr playerEntity;
					if ((player = ServerSidePlayer::Get(client->id)) && (playerEntity = player->GetServerSideEntity())) {
						ServerSideEntity::ForEach([&playerEntity](ServerSideEntity::Ptr entity){
							if (playerEntity->GetID() != entity->GetID()) {
								entity->Deactivate();
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
	
	// V4D_MODULE_FUNC(void, ConfigureShaders) {
	// 	mainRenderModule->AddRayTracingHitShader(THIS_MODULE, "terrain.dig.sphere");
	// }
	
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
			playerView->useCameraParentOffset = playerVisible;
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
				entity->renderableGeometryEntityInstances["root"] = renderableEntity;
				renderableEntity->generator = cake;
				renderableEntity->Remove_physics();
			}break;
			case OBJECT_TYPE::Ball:{
				auto renderableEntity = RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
				entity->renderableGeometryEntityInstances["root"] = renderableEntity;
				float radius = 0.5f;
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
			
			
			case OBJECT_TYPE::Capsule:{
				auto renderableEntity = RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
				entity->renderableGeometryEntityInstances["root"] = renderableEntity;
				float length = 1.0f;
				float radius = 0.2f;
				float halfLength = length / 2.0 + radius;
				renderableEntity->generator = [halfLength,radius](RenderableGeometryEntity* renderableEntity, Device* device){
					RenderableGeometryEntity::Material mat {};
					mat.visibility.roughness = 0;
					mat.visibility.metallic = 1;
					renderableEntity->Allocate(device, "V4D_raytracing:aabb_capsule")->material = mat;
					renderableEntity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius, -radius, -halfLength), glm::vec3(+radius, +radius, +halfLength)});
					renderableEntity->Add_meshVertexColorU8()->AllocateBuffers(device, {127,127,127,255});
				};
			}break;
			case OBJECT_TYPE::Cylinder:{
				auto renderableEntity = RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
				entity->renderableGeometryEntityInstances["root"] = renderableEntity;
				float length = 1.0f;
				float radius = 0.2f;
				float halfLength = length / 2.0;
				renderableEntity->generator = [halfLength,radius](RenderableGeometryEntity* renderableEntity, Device* device){
					RenderableGeometryEntity::Material mat {};
					mat.visibility.roughness = 0;
					mat.visibility.metallic = 1;
					renderableEntity->Allocate(device, "V4D_raytracing:aabb_cylinder")->material = mat;
					renderableEntity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius, -radius, -halfLength), glm::vec3(+radius, +radius, +halfLength)});
					renderableEntity->Add_meshVertexColorU8()->AllocateBuffers(device, {127,127,127,255});
				};
			}break;
			case OBJECT_TYPE::Cone:{
				auto renderableEntity = RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
				entity->renderableGeometryEntityInstances["root"] = renderableEntity;
				float length = 1.0f;
				float radius = 0.4f;
				float radiusB = 0.1f;
				float halfLength = length / 2.0;
				renderableEntity->generator = [halfLength,radius,radiusB](RenderableGeometryEntity* renderableEntity, Device* device){
					RenderableGeometryEntity::Material mat {};
					mat.visibility.roughness = 0;
					mat.visibility.metallic = 1;
					renderableEntity->Allocate(device, "V4D_raytracing:aabb_cone")->material = mat;
					renderableEntity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius, -radius, -halfLength), glm::vec3(+radius, +radius, +halfLength)});
					renderableEntity->Add_meshVertexColorU8()->AllocateBuffers(device, {127,127,127,255});
					renderableEntity->Add_meshCustomData()->AllocateBuffers(device, glm::f32{radiusB});
				};
			}break;
			
			case OBJECT_TYPE::Ring:{
				auto renderableEntity = RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
				entity->renderableGeometryEntityInstances["root"] = renderableEntity;
				float length = 2.4f;
				float radius = 1.8f;
				float radiusB = 0.8f;
				float halfLength = length / 2.0;
				renderableEntity->generator = [halfLength,radius,radiusB](RenderableGeometryEntity* renderableEntity, Device* device){
					RenderableGeometryEntity::Material mat {};
					mat.visibility.roughness = 0;
					mat.visibility.metallic = 1;
					renderableEntity->Allocate(device, "V4D_raytracing:aabb_ring")->material = mat;
					renderableEntity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius, -radius, -halfLength), glm::vec3(+radius, +radius, +halfLength)});
					renderableEntity->Add_meshVertexColorU8()->AllocateBuffers(device, {127,127,127,255});
					renderableEntity->Add_meshCustomData()->AllocateBuffers(device, glm::f32{radiusB});
				};
			}break;
			
			
			case OBJECT_TYPE::Box:{
				auto renderableEntity = RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
				entity->renderableGeometryEntityInstances["root"] = renderableEntity;
				renderableEntity->generator = [](RenderableGeometryEntity* renderableEntity, Device* device){
					RenderableGeometryEntity::Material mat {};
					mat.visibility.textures[0] = Renderer::sbtOffsets["call:tex_bumped"];
					mat.visibility.texFactors[0] = 127;
					mat.visibility.roughness = 0;
					mat.visibility.metallic = 1;
					renderableEntity->Allocate(device, "V4D_raytracing:aabb_cube")->material = mat;
					renderableEntity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-2, -2, -1), glm::vec3(+2, +2, +1)});
					renderableEntity->Add_meshVertexColorU8()->AllocateBuffers(device, {127,127,127,255});
				};
			}break;
			
			case OBJECT_TYPE::GlassBall:{
				auto renderableEntity = RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
				entity->renderableGeometryEntityInstances["root"] = renderableEntity;
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
				entity->renderableGeometryEntityInstances["root"] = renderableEntity;
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
				entity->renderableGeometryEntityInstances["root"] = renderableEntity;
				renderableEntity->generator = cake;
				renderableEntity->Remove_physics();
			}break;
			case OBJECT_TYPE::Avatar:{
				avatarConfig->GenerateRenderables(entity);
			}break;
			case OBJECT_TYPE::Glass:{
				auto renderableEntity = RenderableGeometryEntity::Create(THIS_MODULE, entityUniqueID);
				entity->renderableGeometryEntityInstances["root"] = renderableEntity;
				// renderableEntity->Add_physics(PhysicsInfo::RigidBodyType::STATIC, 1.0f)->SetBoxCollider({2.0f, 2.0f, 0.01f});
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
				
				
				case GLFW_KEY_F1:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("capsule");
						stream << glm::vec3{playerView->viewForward};
					ClientEnqueueAction(stream);
				}break;
				
				case GLFW_KEY_F2:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("cylinder");
						stream << glm::vec3{playerView->viewForward};
					ClientEnqueueAction(stream);
				}break;
				
				case GLFW_KEY_F3:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("cone");
						stream << glm::vec3{playerView->viewForward};
					ClientEnqueueAction(stream);
				}break;
				
				case GLFW_KEY_F4:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("ring");
						stream << glm::vec3{playerView->viewForward};
					ClientEnqueueAction(stream);
				}break;
				
				
				
				case GLFW_KEY_N:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::TEST_OBJ;
						stream << std::string("box");
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
				// case GLFW_KEY_N:{
				// 	v4d::data::WriteOnlyStream stream(32);
				// 		stream << networking::action::TEST_OBJ;
				// 		stream << std::string("balls");
				// 		stream << glm::vec3{playerView->viewForward};
				// 	ClientEnqueueAction(stream);
				// }break;
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
						stream << std::string("avatar");
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
