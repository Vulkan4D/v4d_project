#pragma once
#include "../Celestial.h"

class BlackHole : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::BlackHole;}
};

class SuperMassiveBlackHole : public BlackHole {
	using BlackHole::BlackHole;
	virtual CelestialType GetType() const override {return CelestialType::SuperMassiveBlackHole;}
};
