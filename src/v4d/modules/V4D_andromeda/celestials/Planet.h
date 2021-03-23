#pragma once
#include "../Celestial.h"

class Planet : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::Planet;}
	
	virtual void RenderUpdate(glm::dvec3 position, glm::dvec3 cameraPosition, double sizeInScreen) const override;
	
	mutable std::optional<double> _terrainHeightVariation = std::nullopt;
	mutable std::optional<double> _atmosphereThickness = std::nullopt;
	mutable std::optional<double> _terrainRadius = std::nullopt;
	mutable std::optional<double> _atmosphereRadius = std::nullopt;
	
	virtual double GetTerrainHeightVariation() const;
	virtual double GetAtmosphereThickness() const;
	virtual double GetTerrainRadius() const;
	virtual double GetAtmosphereRadius() const;

};
