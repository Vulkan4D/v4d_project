#pragma once
#include "../Celestial.h"

class Star : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::Star;}
	
	virtual void RenderUpdate(glm::dvec3 position, glm::dvec3 cameraPosition, double sizeInScreen) const override;

};

class BrownDwarf : public Star {
	using Star::Star;
	virtual CelestialType GetType() const override {return CelestialType::BrownDwarf;}
};

class HyperGiant : public Star {
	using Star::Star;
	virtual CelestialType GetType() const override {return CelestialType::HyperGiant;}
};
