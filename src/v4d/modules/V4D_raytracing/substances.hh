#pragma once

#include <v4d.h>
#include "elements.hh"

struct ElementComposition {
	uint8_t element;
	float ratio;
	ElementComposition(uint8_t atomicNumber, float ratio = 1.0f) : element(atomicNumber), ratio(ratio) {}
	ElementComposition(const char* symbol, float ratio = 1.0f) : element(Element::Get(symbol)->atomicNumber), ratio(ratio) {}
};

struct Substance {
	std::vector<ElementComposition> composition;
	glm::vec4 color;
	float metallic;
	float roughness;
	float IOR;
	std::string_view baseMap;
	float agingFactor;
	std::string_view agingMap;
	float oxydationFactor;
	std::string_view oxydationMap;
	float wearAndTearFactor;
	std::string_view wearAndTearMap;
	float burnFactor;
	std::string_view burnMap;
	
	static std::unordered_map<std::string, Substance> substances;
};

// Callable shaders for:
	// 9 metal base maps / surface finish
		// tex_noisy
		// tex_grainy
		// tex_bumped
		// tex_brushed
		// tex_hammered
		// tex_polished
		// tex_galvanized
		// tex_perforated
		// tex_diamond
	// 2 other base maps
		// tex_carbon_fiber
		// tex_ceramic
	// 4 metal oxydation maps
		// tex_oxydation_iron
		// tex_oxydation_copper
		// tex_oxydation_aluminum
		// tex_oxydation_silver
	// 3 wear and tear maps
		// tex_scratches_metal
		// tex_scratches_plastic
		// tex_scratches_glass
	// 1 aging map
		// tex_cracked_rubber
		// tex_cracked_ceramic

// Glass may be tinted
// Any other material when painted: (vec4 rgb=albedo, a=roughness) [metallic is reset to 0]
// When painted, oxydation map is masked with wearAndTear map

// Common elements found in atmosphere: Nitrogen, Carbon, Oxygen, Hydrogen
// Common elements found in surface rocks: Oxygen, Silicon, Aluminum, Iron, Calcium, Sodium, Potassium, Magnesium, Sulphur
// Common elements mined by digging: Copper, Nickel, Zinc, Tin, Carbon, Lithium, Chromium, Hydrogen, Nitrogen
// Less common elements mined deeper: Neodymium, Titanium, Thorium, Silver, Gold, Tungsten, Lead, Uranium, Argon, Neon, Xenon, Helium

// Oxydation and Aging is global to the object
// WearAndTear and Burn is global + local with 16 spots

#ifdef V4D_RAYTRACING_RENDERER_MODULE
	std::unordered_map<std::string, Substance> Substance::substances {
		// Pure metals
		{"copper",			{{"Cu"},													/*color*/{0.96, 0.64, 0.54, 1.0},	/*metallic*/1, /*roughness*/0.1, /*IOR*/0.24, /*base*/"tex_polished",			/*aging*/0.0,"",						/*oxydation*/0.8,"tex_oxydation_copper",	/*wearAndTear*/0.6,"tex_scratches_metal",		/*burn*/0.0,""				}},// copper
		{"gold",			{{"Au"},													/*color*/{1.00, 0.77, 0.34, 1.0},	/*metallic*/1, /*roughness*/0.1, /*IOR*/0.18, /*base*/"tex_polished",			/*aging*/0.0,"",						/*oxydation*/0.0,"",						/*wearAndTear*/0.8,"tex_scratches_metal",		/*burn*/0.0,""				}},// gold
		{"titanium",		{{"Ti"},													/*color*/{0.54, 0.50, 0.45, 1.0},	/*metallic*/1, /*roughness*/0.2, /*IOR*/2.15, /*base*/"tex_polished",			/*aging*/0.0,"",						/*oxydation*/0.0,"",						/*wearAndTear*/1.0,"tex_scratches_metal",		/*burn*/0.0,""				}},// titanium
		{"silver",			{{"Ar"},													/*color*/{0.97, 0.96, 0.92, 1.0},	/*metallic*/1, /*roughness*/0.1, /*IOR*/0.14, /*base*/"tex_polished",			/*aging*/0.0,"",						/*oxydation*/0.7,"tex_oxydation_silver",	/*wearAndTear*/0.8,"tex_scratches_metal",		/*burn*/0.0,""				}},// silver
		{"aluminum",		{{"Al"},													/*color*/{0.91, 0.92, 0.93, 1.0},	/*metallic*/1, /*roughness*/0.1, /*IOR*/1.37, /*base*/"tex_noisy",				/*aging*/0.0,"",						/*oxydation*/0.1,"tex_oxydation_aluminum",	/*wearAndTear*/1.0,"tex_scratches_metal",		/*burn*/0.0,""				}},// aluminum
		{"iron",			{{"Fe"},													/*color*/{0.56, 0.57, 0.58, 1.0},	/*metallic*/1, /*roughness*/0.1, /*IOR*/2.91, /*base*/"tex_polished",			/*aging*/0.0,"",						/*oxydation*/1.0,"tex_oxydation_iron",		/*wearAndTear*/0.9,"tex_scratches_metal",		/*burn*/0.0,""				}},// iron
		
		// Alloys
		{"steel",			{{{"Fe", 0.99}, {"C", 0.01}},								/*color*/{0.8, 0.8, 0.8, 1.0},		/*metallic*/1, /*roughness*/0.10, /*IOR*/2.75, /*base*/"tex_noisy",				/*aging*/0.0,"",						/*oxydation*/0.5,"tex_oxydation_iron",		/*wearAndTear*/0.3,"tex_scratches_metal",		/*burn*/0.0,""				}},// steel
		{"cast_iron",		{{{"Fe", 0.97}, {"C", 0.03}},								/*color*/{0.3, 0.3, 0.3, 1.0},		/*metallic*/1, /*roughness*/0.10, /*IOR*/2.60, /*base*/"tex_grainy",			/*aging*/0.0,"",						/*oxydation*/0.8,"tex_oxydation_iron",		/*wearAndTear*/0.3,"tex_scratches_metal",		/*burn*/0.0,""				}},// cast_iron
		{"stainless_steel", {{{"Fe", 0.878}, {"Cr", 0.11}, {"C", 0.012}},				/*color*/{0.95, 0.96, 0.97, 1.0},	/*metallic*/1, /*roughness*/0.10, /*IOR*/2.50, /*base*/"tex_bumped",			/*aging*/0.0,"",						/*oxydation*/0.0,"",						/*wearAndTear*/0.3,"tex_scratches_metal",		/*burn*/0.0,""				}},// stainless_steel
		{"brass",			{{{"Cu", 0.65}, {"Zn", 0.35}},								/*color*/{0.9, 0.8, 0.75, 1.0},		/*metallic*/1, /*roughness*/0.10, /*IOR*/0.50, /*base*/"tex_noisy",				/*aging*/0.0,"",						/*oxydation*/0.5,"tex_oxydation_copper",	/*wearAndTear*/0.4,"tex_scratches_metal",		/*burn*/0.0,""				}},// brass
		{"bronze",			{{{"Cu", 0.88}, {"Sn", 0.12}},								/*color*/{0.9, 0.8, 0.5, 1.0},		/*metallic*/1, /*roughness*/0.10, /*IOR*/1.18, /*base*/"tex_noisy",				/*aging*/0.0,"",						/*oxydation*/0.5,"tex_oxydation_copper",	/*wearAndTear*/0.4,"tex_scratches_metal",		/*burn*/0.0,""				}},// bronze
		
		// Composites
		{"plastic",			{{{"H", 0.4}, {"C", 0.3}, {"O", 0.2}, {"Si", 0.1}},			/*color*/{1.0, 1.0, 1.0, 1.0},		/*metallic*/0, /*roughness*/0.50, /*IOR*/1.45, /*base*/"",						/*aging*/0.0,"",						/*oxydation*/0.0,"",						/*wearAndTear*/1.0,"tex_scratches_plastic",		/*burn*/0.0,""				}},// plastic
		{"silicone",		{{{"Si", 0.5}, {"O", 0.5}},									/*color*/{1.0, 1.0, 1.0, 1.0},		/*metallic*/0, /*roughness*/0.70, /*IOR*/1.45, /*base*/"",						/*aging*/0.0,"",						/*oxydation*/0.0,"",						/*wearAndTear*/0.0,"",							/*burn*/0.0,""				}},// silicone
		{"rubber",			{{{"H", 0.4}, {"C", 0.3}, {"O", 0.2}, {"Si", 0.1}},			/*color*/{1.0, 1.0, 1.0, 1.0},		/*metallic*/0, /*roughness*/0.50, /*IOR*/1.45, /*base*/"",						/*aging*/1.0,"tex_cracked_rubber",		/*oxydation*/0.0,"",						/*wearAndTear*/0.0,"",							/*burn*/0.0,""				}},// rubber
		{"carbon_fiber",	{{{"C", 0.9}, {"H", 0.05}, {"O", 0.05}},					/*color*/{0.5, 0.5, 0.5, 1.0},		/*metallic*/0, /*roughness*/0.20, /*IOR*/1.45, /*base*/"tex_carbon_fiber",		/*aging*/0.0,"",						/*oxydation*/0.0,"",						/*wearAndTear*/0.0,"",							/*burn*/0.0,""				}},// carbon_fiber
		
		// Others
		{"ceramic",			{{{"Si", 0.8}, {"O", 0.15}, {"Al", 0.04}, {"Fe", 0.01}},	/*color*/{0.7, 0.7, 0.7, 1.0},		/*metallic*/0, /*roughness*/1.00, /*IOR*/1.45, /*base*/"tex_ceramic",			/*aging*/0.1,"tex_cracked_ceramic",		/*oxydation*/0.0,"",						/*wearAndTear*/0.0,"",							/*burn*/0.0,""				}},// ceramic
		{"glass",			{{{"Si", 0.7}, {"O", 0.15}, {"Na", 0.1}, {"Ca", 0.05}},		/*color*/{1.0, 1.0, 1.0, 0.01},		/*metallic*/0, /*roughness*/0.01, /*IOR*/1.50, /*base*/"",						/*aging*/0.0,"",						/*oxydation*/0.0,"",						/*wearAndTear*/0.1,"tex_scratches_glass",		/*burn*/0.0,""				}},// glass
	};
#endif

