#include "Planet.h"
#include "../PlanetRenderer/PlanetTerrain.h"
#include "../TerrainGeneratorLib.h"
#include "../../V4D_raytracing/camera_options.hh"

double Planet::GetTerrainHeightVariation() const {
	if (!_terrainHeightVariation.has_value()) {
		_terrainHeightVariation = 10'000; //TODO 4'000 - 12'000
	}
	return _terrainHeightVariation.value();
}
double Planet::GetAtmosphereThickness() const {
	if (!_atmosphereThickness.has_value()) {
		_atmosphereThickness = 200'000; //TODO 1'000 - 1'000'000
	}
	return _atmosphereThickness.value();
}
double Planet::GetTerrainRadius() const {
	if (!_terrainRadius.has_value()) {
		_terrainRadius = GetRadius() - std::max(GetTerrainHeightVariation(), GetAtmosphereThickness());
	}
	return _terrainRadius.value();
}
double Planet::GetAtmosphereRadius() const {
	if (!_atmosphereRadius.has_value()) {
		_atmosphereRadius = std::min(GetRadius(), GetTerrainRadius() + GetAtmosphereThickness());
	}
	return _atmosphereRadius.value();
}

std::shared_ptr<PlanetTerrain> Planet::GetPlanetTerrain() const {
	if (!_planetTerrain) {
		_planetTerrain = PlanetTerrain::terrains[GetID()];
		if (!_planetTerrain) {
			{
				std::lock_guard generatorLock(TerrainGeneratorLib::mu);
				_planetTerrain = std::make_shared<PlanetTerrain>(
					  GetRadius()
					, GetTerrainRadius()
					, GetTerrainHeightVariation()
					, GetAtmosphereRadius()
					, GetAtmosphereThickness()
					, /*visibilityDistance*/1'000'000
					, /*rayleighHeight*/GetAtmosphereThickness()/20.0 // maximum of 10% atmosphereThickness
					, /*mieHeight*/GetAtmosphereThickness()/40.0 // maximum of 10% atmosphereThickness
				);
			}
			PlanetTerrain::terrains[GetID()] = _planetTerrain;
		}
	}
	return _planetTerrain;
}

void Planet::RenderUpdate(glm::dvec3 position, glm::dvec3 cameraPosition, double sizeInScreen/* > 0.001 */) const {
	const double sizeInScreenThreshold = 0.004;
	
	auto& sphere = renderableEntities["sphere"];
	// auto& atmosphere = renderableEntities["atmosphere"];
	
	// Sphere
	if (sizeInScreen < sizeInScreenThreshold) {
		if (!sphere) {
			sphere = v4d::graphics::RenderableGeometryEntity::Create(THIS_MODULE, GetID());
			sphere->generator = [this](v4d::graphics::RenderableGeometryEntity* entity, v4d::graphics::vulkan::Device* device){
				double radius = GetRadius();
				entity->Allocate(device, "V4D_raytracing:aabb_sphere");
				entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
				entity->Add_meshVertexColorU8()->AllocateBuffers(device, {255,0,0,255});
			};
		}
		sphere->SetWorldTransform(glm::translate(glm::dmat4(1), position));
	} else {
		if (sphere) {
			sphere->Destroy();
			sphere = nullptr;
		}
	}
	
	// Terrain
	if (sizeInScreen > sizeInScreenThreshold) {
		auto terrain = GetPlanetTerrain();
		
		terrain->matrix = glm::translate(glm::dmat4(1), position);
		terrain->cameraPos = glm::inverse(terrain->matrix) * glm::dvec4(cameraPosition, 1);
		
		terrain->Update();
		
	} else {
		if (_planetTerrain) {
			_planetTerrain->RemoveBaseChunks();
			_planetTerrain = nullptr;
		}
	}
}
