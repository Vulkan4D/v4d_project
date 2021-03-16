#pragma once
#include "../Celestial.h"

class Star : public Celestial {
	using Celestial::Celestial;
	virtual CelestialType GetType() const override {return CelestialType::Star;}
};

class BrownDwarf : public Star {
	using Star::Star;
	virtual CelestialType GetType() const override {return CelestialType::BrownDwarf;}
};

class HyperGiant : public Star {
	using Star::Star;
	virtual CelestialType GetType() const override {return CelestialType::HyperGiant;}
};
