#include <v4d.h>

#include "GalaxyGenerator.h"
#include "Celestial.h"
#include "StarSystem.h"

#include "noise_functions.hpp"

int starsystem(uint32_t x, uint32_t y, uint32_t z) {
	std::string tabs {""};
	auto displayCelestialInfo = [&tabs](Celestial* celestial) {
		switch (celestial->GetType()) {
			case CelestialType::BinaryCenter:
				std::cout << tabs << "Binary System" << "\n\n";
				return;
			case CelestialType::HyperGiant:
				std::cout << tabs << "HyperGiant" << "\n";
				break;
			case CelestialType::Star:
				if (celestial->GetMass() > 1e31) {
					std::cout << tabs << "Giant Star" << "\n";
				} else if (celestial->GetMass() > 1e30) {
					std::cout << tabs << "Main sequence Star" << "\n";
				} else {
					std::cout << tabs << "Dwarf Star" << "\n";
				}
				break;
			case CelestialType::BrownDwarf:
				std::cout << tabs << "BrownDwarf" << "\n";
				break;
			case CelestialType::Planet:
				if (celestial->GetMass() > 1e25) {
					std::cout << tabs << "Giant Planet" << "\n";
				} else if (celestial->GetMass() > 1e24) {
					std::cout << tabs << "Earth-like Planet" << "\n";
				} else if (celestial->GetMass() > 1e23) {
					std::cout << tabs << "Small Planet" << "\n";
				} else {
					std::cout << tabs << "Dwarf Planet" << "\n";
				}
				break;
			case CelestialType::GasGiant:
				std::cout << tabs << "GasGiant" << "\n";
				break;
			case CelestialType::BlackHole:
				std::cout << tabs << "BlackHole" << "\n";
				break;
			case CelestialType::Asteroid:
				std::cout << tabs << "Asteroid" << "\n";
				break;
			case CelestialType::SuperMassiveBlackHole:
				std::cout << tabs << "SuperMassiveBlackHole" << "\n";
				break;
		}
		
		std::cout << tabs << "Radius: " << (celestial->GetRadius()/1000.0) << " km\n";
		std::cout << tabs << "Mass: " << celestial->GetMass() << " kg\n";
		std::cout << tabs << "Orbit Distance: " << M2AU(celestial->GetOrbitDistance()) << " AU / " << (celestial->GetOrbitDistance()/1000.0) << " km\n";
		
		std::cout << "\n";
	};
	
	GalacticPosition galacticPosition;
	galacticPosition.SetReferenceFrame(x, y, z);
	
	if (GalaxyGenerator::GetStarSystemPresence(galacticPosition)) {
		int count = 0;
		const StarSystem& starSystem(galacticPosition);
		std::cout << "Found a system with a radius of " << M2LY(starSystem.GetRadius()) << " LY / " << M2AU(starSystem.GetRadius()) << " AU\n";
		for (auto& level1 : starSystem.GetCentralCelestialBodies()) if (level1) {
			tabs = "";
			displayCelestialInfo(level1.get());
			++count;
			for (auto& level2 : level1->GetChildren()) {
				tabs = "\t";
				displayCelestialInfo(level2.get());
				++count;
				for (auto& level3 : level2->GetChildren()) {
					tabs = "\t\t";
					displayCelestialInfo(level3.get());
					++count;
				}
			}
			
			std::cout << "\n";
		}
		std::cout << "Total of " << count << " celestial bodies found.\n\n";
	} else {
		std::cout << "No system found\n";
	}
	return 0;
}

V4D_MODULE_CLASS(V4D_Mod) {
	V4D_MODULE_FUNC(int, RunFromConsole, const int argc, const char** argv) {
		if (argc == 4 && std::string("starsystem") == argv[0]) {
			return starsystem(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
		}
		if (argc == 2 && std::string("starsystem") == argv[0]) {
			GalacticPosition galacticPosition;
			galacticPosition.SetReferenceFrame(atol(argv[1]));
			return starsystem(galacticPosition.posInGalaxy_x, galacticPosition.posInGalaxy_y, galacticPosition.posInGalaxy_z);
		}
		
		
		
		
		
		// Stats
		
		int min_x = 100000, min_y = 2000, min_z = 100000;
		int max_x = 150000, max_y = 2100, max_z = 150000;
		
		uint64_t n = 0;
		uint64_t totalCount = 0;
		uint64_t maxCount = 0;
		uint64_t minCount = std::numeric_limits<uint64_t>::max();
		uint64_t minCountPosInGalaxy = 0;
		uint64_t maxCountPosInGalaxy = 0;
		
		uint64_t countWithOne = 0;
		uint64_t countWithTwo = 0;
		
		std::unordered_map<int, uint64_t> typeCount;
		
		uint seed = 0;
		for (int i = 0; i < 5'000'000; ++i) {
			
			GalacticPosition galacticPosition;
			galacticPosition.SetReferenceFrame(RandomInt(seed, min_x, max_x), RandomInt(seed, min_y, max_y), RandomInt(seed, min_z, max_z));
			if (GalaxyGenerator::GetStarSystemPresence(galacticPosition)) {
				int count = 0;
				const StarSystem& starSystem(galacticPosition);
				for (auto& level1 : starSystem.GetCentralCelestialBodies()) if (level1) {
					typeCount[(int)level1->GetType()]++;
					++count;
					for (auto& level2 : level1->GetChildren()) {
						typeCount[(int)level2->GetType()]++;
						++count;
						for (auto& level3 : level2->GetChildren()) {
							++count;
							typeCount[(int)level3->GetType()]++;
						}
					}
				}
				if (maxCount < count) {
					maxCount = count;
					maxCountPosInGalaxy = galacticPosition.posInGalaxy;
				}
				if (minCount > count) {
					minCount = count;
					minCountPosInGalaxy = galacticPosition.posInGalaxy;
				}
				if (count == 1) {
					++countWithOne;
				}
				if (count == 2) {
					++countWithTwo;
				}
				totalCount += count;
				++n;
			}
			
		}
		
		LOG("Within a total of " << n << " systems:")
		LOG("Avg celestials in systems: " << (totalCount / n))
		LOG("Min celestial count is " << minCount << " in system " << minCountPosInGalaxy)
		LOG("Max celestial count is " << maxCount << " in system " << maxCountPosInGalaxy)
		LOG("Star systems with only one celestial: " << countWithOne)
		LOG("Star systems with only two celestials: " << countWithTwo)
		
		for (auto[type, count] : typeCount) {
			switch ((CelestialType)type) {
				case CelestialType::BinaryCenter:
					std::cout << "Binary Systems with three orbital planes: " << count << " ... averaging " << (((double)count) / n);
					break;
				case CelestialType::HyperGiant:
					std::cout << "Hyper Giants: " << count << " ... averaging " << (((double)count) / n);
					break;
				case CelestialType::Star:
					std::cout << "Stars: " << count << " ... averaging " << (((double)count) / n);
					break;
				case CelestialType::BrownDwarf:
					std::cout << "Brown Dwarfs: " << count << " ... averaging " << (((double)count) / n);
					break;
				case CelestialType::Planet:
					std::cout << "Planets: " << count << " ... averaging " << (((double)count) / n);
					break;
				case CelestialType::GasGiant:
					std::cout << "Gas Giants: " << count << " ... averaging " << (((double)count) / n);
					break;
				case CelestialType::BlackHole:
					std::cout << "Black Holes: " << count << " ... averaging " << (((double)count) / n);
					break;
				case CelestialType::Asteroid:
					std::cout << "Asteroids: " << count << " ... averaging " << (((double)count) / n);
					break;
				case CelestialType::SuperMassiveBlackHole:
					std::cout << "Super Massive Black Holes: " << count << " ... averaging " << (((double)count) / n);
					break;
			}
			std::cout << "\n";
		}
		
		
		
		
		return 0;
	}
};
