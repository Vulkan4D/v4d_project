#pragma once
#include "../Celestial.h"
#include "../PlanetRenderer/PlanetTerrain.h"

struct TerrainType {
	double restitution = 0.4;
	double friction = 0.8;
	//...
};

class Planet : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::Planet;}
	
	virtual void RenderUpdate(glm::dvec3 position, glm::dvec3 cameraPosition, double sizeInScreen) const override;
	
	mutable std::optional<double> _terrainHeightVariation = std::nullopt;
	mutable std::optional<double> _atmosphereThickness = std::nullopt;
	mutable std::optional<double> _terrainRadius = std::nullopt;
	mutable std::optional<double> _atmosphereRadius = std::nullopt;
	mutable std::shared_ptr<PlanetTerrain> _planetTerrain = nullptr;
	
public:
	virtual double GetTerrainHeightVariation() const;
	virtual double GetAtmosphereThickness() const;
	virtual double GetTerrainRadius() const;
	virtual double GetAtmosphereRadius() const;
	virtual std::shared_ptr<PlanetTerrain> GetPlanetTerrain() const;
	
	inline virtual double GetTerrainHeightAtPos(const glm::dvec3& normalizedPos) const {
		if (!PlanetTerrain::generatorFunction) return GetTerrainRadius();
		return GetTerrainRadius() + PlanetTerrain::generatorFunction(normalizedPos, GetTerrainRadius(), GetTerrainHeightVariation());
	}
	inline virtual TerrainType GetTerrainTypeAtPos(const glm::dvec3& normalizedPos) const {
		return {}; //TODO
	}

};
