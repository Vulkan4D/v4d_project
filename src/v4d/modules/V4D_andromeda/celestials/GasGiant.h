#pragma once
#include "../Celestial.h"

class GasGiant : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::GasGiant;}
};
