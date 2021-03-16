#pragma once
#include "../Celestial.h"

class Planet : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::Planet;}
};
