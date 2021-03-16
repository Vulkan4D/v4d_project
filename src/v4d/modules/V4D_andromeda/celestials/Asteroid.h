#pragma once
#include "../Celestial.h"

class Asteroid : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::Asteroid;}
};
