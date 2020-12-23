#pragma once

#include <v4d.h>

struct Element {
	uint8_t atomicNumber;
	std::string_view symbol;
	std::string_view name;
	double atomicMass;
	std::vector<int8_t> valence {};
	
	// Element(uint8_t atomicNumber, std::string_view symbol, std::string_view name, double atomicMass, std::vector<int8_t> valence = {}) : atomicNumber(atomicNumber), symbol(symbol), name(name), atomicMass(atomicMass), valence(valence) {}
	
	static constexpr double ATOMIC_MASS_TO_KG = 1.6605402e-27;
	
private:
	static const std::vector<Element> elements;
	static const std::unordered_map<std::string_view, const Element*> elementsBySymbol;
public:
	static const Element* const Get(std::string_view symbol) {
		try {
			return elementsBySymbol.at(symbol);
		} catch(...) {
			return nullptr;
		}
	}
	static const Element* const Get(uint8_t atomicNumber) {
		if (atomicNumber < elements.size())
			return &elements[atomicNumber];
		return nullptr;
	}
};

// struct Molecule {
// 	static std::unordered_map<std::string_view, Molecule> molecules;
	
// 	std::vector<std::tuple<uint8_t, uint8_t>> elements;
	
// 	Molecule(std::vector<std::tuple<uint8_t, uint8_t>> elems) : elements(elems) {}
// 	Molecule() : elements({}) {}
// 	Molecule(std::string formula) : elements({}) {
// 		// Caching for faster decoding
// 		try {
// 			elements = molecules.at(formula).elements;
// 			return;
// 		} catch (...) {}
		
// 		if (formula.length() < 3) {
// 			const auto* singleElement = Element::Get(formula);
// 			if (singleElement) {
// 				elements = {{singleElement->atomicNumber, 1}};
// 				return;
// 			}
// 		}
		
// 		// Decode string formula
// 		std::string tmp = formula;
// 		std::regex expr {R"(^([A-Z][a-z]{0,2})([0-9]*)([A-Z].*$|$))"};
// 		do {
// 			std::cmatch m;
// 			if (!std::regex_match(tmp.c_str(), m, expr)) {
// 				elements.clear();
// 				return;
// 			}
// 			auto* element = Element::Get(m[1].str());
// 			if (!element) {
// 				elements.clear();
// 				return;
// 			}
// 			std::string n = m[2].str();
// 			if (n == "") n = "1";
// 			tmp = m[3].str();
// 			try {
// 				elements.emplace_back(element->atomicNumber, (uint8_t)atoi(n.c_str()));
// 			} catch (...) {
// 				elements.clear();
// 				return;
// 			}
// 		} while (tmp.length() > 0);
		
// 		// Cache molecule
// 		molecules[formula].elements = elements;
// 	}
	
// 	bool IsValid() const {
// 		return elements.size() > 0;
// 	}
	
// 	std::string Formula() const {
// 		std::stringstream str {};
// 		for (auto[e,n] : elements) {
// 			auto* elem = Element::Get(e);
// 			if (!elem) {
// 				str.clear();
// 				break;
// 			}
// 			str << elem->symbol;
// 			if (n > 1) str << n;
// 		}
// 		return str.str();
// 	}
	
// 	double Mass() const { // Kilograms
// 		double mass = 0;
// 		for (auto[e,n] : elements) {
// 			auto* elem = Element::Get(e);
// 			if (elem) {
// 				mass += elem->atomicMass * Element::ATOMIC_MASS_TO_KG * n;
// 			}
// 		}
// 		return mass;
// 	}
	
// 	double HeatCapacity() const {
// 		//TODO
// 		return 1;
// 		// J/g celcius    delta temperature = added heat / (heatCapacity * mass)
// 	}
	
// 	//TODO states ranges per temperatures and pressures, electrical/thermal conductivity, strength, ... based on temperature ...
	
// };

#ifdef V4D_RAYTRACING_RENDERER_MODULE

	const std::vector<Element> Element::elements {
		{0, "_", "", 0.0},
		{1, "H", "Hydrogen", 1.008, {+1}},
		{2, "He", "Helium", 4.003},
		{3, "Li", "Lithium", 6.941, {+1}},
		{4, "Be", "Berylium", 9.012, {+2}},
		{5, "B", "Boron", 10.811, {+3}},
		{6, "C", "Carbon", 12.011, {+4, -4}},
		{7, "N", "Nitrogen", 14.007, {+5, -3}},
		{8, "O", "Oxygen", 15.999, {-2}},
		{9, "F", "Fluorine", 18.998, {-1}},
		{10, "Ne", "Neon", 20.180},
		{11, "Na", "Sodium", 22.99, {+1}},
		{12, "Mg", "Magnesium", 24.305, {+2}},
		{13, "Al", "Aluminum", 26.982, {+3}},
		{14, "Si", "Silicon", 28.086, {+4}},
		{15, "P", "Phosphorus", 30.974, {+5, +3, -3}},
		{16, "S", "Sulfur", 32.066, {+6, -2}},
		{17, "Cl", "Chlorine", 35.453, {-1}},
		{18, "Ar", "Argon", 39.948},
		{19, "K", "Potassium", 39.098, {+1}},
		{20, "Ca", "Calcium", 40.078, {+2}},
		{21, "Sc", "Scandium", 44.956, {+3}},
		{22, "Ti", "Titanium", 47.867, {+4}},
		{23, "V", "Vanadium", 50.942, {+5, +4, +3}},
		{24, "Cr", "Chromium", 51.996, {+6, +3, +2}},
		{25, "Mn", "Manganese", 54.938, {+7, +4, +2}},
		{26, "Fe", "Iron", 55.845, {+3, +2}},
		{27, "Co", "Cobalt", 58.933, {+3, +2}},
		{28, "Ni", "Nickel", 58.693, {+2}},
		{29, "Cu", "Copper", 63.546, {+2, +1}},
		{30, "Zn", "Zinc", 65.38, {+2}},
		{31, "Ga", "Gallium", 69.723, {+3}},
		{32, "Ge", "Germanium", 72.631, {+4}},
		{33, "As", "Arsenic", 74.922, {+5, +3}},
		{34, "Se", "Selenium", 78.971, {+4, -2}},
		{35, "Br", "Bromine", 79.904, {+5, -1}},
		{36, "Kr", "Krypton", 84.798},
		{37, "Rb", "Rubidium", 84.468, {+1}},
		{38, "Sr", "Strontium", 87.62, {+2}},
		{39, "Y", "Yttrium", 88.906, {+3}},
		{40, "Zr", "Zirconium", 91.224, {+4}},
		{41, "Nb", "Niobium", 92.906, {+5}},
		{42, "Mo", "Molybdenum", 95.95, {+6, +4}},
		{43, "Tc", "Technetium", 98.907, {+7, +4}},
		{44, "Ru", "Ruthenium", 101.07, {+4, +3}},
		{45, "Rh", "Rhodium", 102.906, {+3}},
		{46, "Pd", "Palladium", 106.42, {+4, +2}},
		{47, "Ag", "Silver", 107.868, {+1}},
		{48, "Cd", "Cadmium", 112.414, {+2}},
		{49, "In", "Indium", 114.818, {+3}},
		{50, "Sn", "Tin", 118.711, {+2, -4}},
		{51, "Sb", "Antimony", 121.76, {+3}},
		{52, "Te", "Tellurium", 127.6, {+4}},
		{53, "I", "Iodine", 126.904, {+5, -1}},
		{54, "Xe", "Xenon", 131.294},
		{55, "Cs", "Cesium", 132.905, {+1}},
		{56, "Ba", "Barium", 137.328, {+2}},

		// Lanthanide
		{57, "La", "Lanthanum", 138.905, {+3}},
		{58, "Ce", "Cerium", 140.116, {+3}},
		{59, "Pr", "Praseodymium", 140.908, {+3}},
		{60, "Nd", "Neodymium", 144.243, {+3}},
		{61, "Pm", "Promethium", 144.913, {+3}},
		{62, "Sm", "Samarium", 150.36, {+3}},
		{63, "Eu", "Europium", 151.964, {+3}},
		{64, "Gd", "Gadolinium", 157.25, {+3}},
		{65, "Tb", "Terbium", 158.925, {+3}},
		{66, "Dy", "Dysprosium", 162.5, {+3}},
		{67, "Ho", "Holmium", 164.93, {+3}},
		{68, "Er", "Erbium", 167.259, {+3}},
		{69, "Tm", "Thulium", 168.934, {+3}},
		{70, "Yb", "Ytterbium", 173.055, {+3}},
		{71, "Lu", "Lutetium", 174.967, {+3}},

		{72, "Hf", "Hafnium", 178.49, {+4}},
		{73, "Ta", "Tantalum", 180.948, {+5}},
		{74, "W", "Tungsten", 183.84, {+6, +4}},
		{75, "Re", "Rhenium", 186.207, {+5, +4, +3}},
		{76, "Os", "Osmium", 190.23, {+4}},
		{77, "Ir", "Iridium", 192.217, {+4, +3}},
		{78, "Pt", "Platinum", 195.085, {+4, +2}},
		{79, "Au", "Gold", 196.967, {+3}},
		{80, "Hg", "Mercury", 200.592, {+2, +1}},
		{81, "Tl", "Thallium", 204.383, {+3, +1}},
		{82, "Pb", "Lead", 207.2, {+2}},
		{83, "Bi", "Bismuth", 208.98, {+3}},
		{84, "Po", "Polonium", 208.982, {+4}},
		{85, "At", "Astatine", 209.987, {-1}},
		{86, "Rn", "Radon", 222.018},
		{87, "Fr", "Francium", 223.02, {+1}},
		{88, "Ra", "Radium", 226.025, {+2}},

		// Actinide
		{89, "Ac", "Actinium", 227.028, {+3}},
		{90, "Th", "Thorium", 232.038, {+4}},
		{91, "Pa", "Protactinium", 231.036, {+5}},
		{92, "U", "Uranium", 238.029, {+6}},
		{93, "Np", "Neptunium", 237.048, {+7}},
		{94, "Pu", "Plutonium", 244.064, {+7, +4}},
		{95, "Am", "Americium", 243.061, {+3}},
		{96, "Cm", "Curium", 247.07, {+3}},
		{97, "Bk", "Berkelium", 247.07, {+3}},
		{98, "Cf", "Californium", 251.08, {+3}},
		{99, "Es", "Einsteinium", 254.0, {+3}},
		{100, "Fm", "Fermium", 257.095, {+3}},
		{101, "Md", "Mendelevium", 258.1, {+3}},
		{102, "No", "Nobelium", 259.101, {+3}},
		{103, "Lr", "Lawrencium", 262.0, {+3}},

		{104, "Rf", "Rutherfordium", 261.0, {+4}},
		{105, "Db", "Dubnium", 262.0, {00000000}},
		{106, "Sg", "Seaborgium", 266.0, {00000000}},
		{107, "Bh", "Bohrium", 264.0, {00000000}},
		{108, "Hs", "Hassium", 269.0, {00000000}},
		{109, "Mt", "Meitnerium", 268.0, {00000000}},
		{110, "Ds", "Darmstadtium", 269.0, {00000000}},
		{111, "Rg", "Roentgenium", 272.0, {00000000}},
		{112, "Cn", "Copernicium", 277.0, {00000000}},
		{113, "Uut", "Ununtrium", 00000000.0, {00000000}},
		{114, "Fl", "Flerovium", 289.0, {00000000}},
		{115, "Uup", "Ununpentium", 00000000.0, {00000000}},
		{116, "Lv", "Livermorium", 298.0, {00000000}},
		{117, "Uus", "Ununseptium", 00000000.0, {00000000}},
		{118, "Uuo", "Ununoctium", 00000000.0, {00000000}},

	};
	
	const std::unordered_map<std::string_view, const Element*> Element::elementsBySymbol {
		{elements[0].symbol, &elements[0]},
		{elements[1].symbol, &elements[1]},
		{elements[2].symbol, &elements[2]},
		{elements[3].symbol, &elements[3]},
		{elements[4].symbol, &elements[4]},
		{elements[5].symbol, &elements[5]},
		{elements[6].symbol, &elements[6]},
		{elements[7].symbol, &elements[7]},
		{elements[8].symbol, &elements[8]},
		{elements[9].symbol, &elements[9]},
		{elements[10].symbol, &elements[10]},
		{elements[11].symbol, &elements[11]},
		{elements[12].symbol, &elements[12]},
		{elements[13].symbol, &elements[13]},
		{elements[14].symbol, &elements[14]},
		{elements[15].symbol, &elements[15]},
		{elements[16].symbol, &elements[16]},
		{elements[17].symbol, &elements[17]},
		{elements[18].symbol, &elements[18]},
		{elements[19].symbol, &elements[19]},
		{elements[20].symbol, &elements[20]},
		{elements[21].symbol, &elements[21]},
		{elements[22].symbol, &elements[22]},
		{elements[23].symbol, &elements[23]},
		{elements[24].symbol, &elements[24]},
		{elements[25].symbol, &elements[25]},
		{elements[26].symbol, &elements[26]},
		{elements[27].symbol, &elements[27]},
		{elements[28].symbol, &elements[28]},
		{elements[29].symbol, &elements[29]},
		{elements[30].symbol, &elements[30]},
		{elements[31].symbol, &elements[31]},
		{elements[32].symbol, &elements[32]},
		{elements[33].symbol, &elements[33]},
		{elements[34].symbol, &elements[34]},
		{elements[35].symbol, &elements[35]},
		{elements[36].symbol, &elements[36]},
		{elements[37].symbol, &elements[37]},
		{elements[38].symbol, &elements[38]},
		{elements[39].symbol, &elements[39]},
		{elements[40].symbol, &elements[40]},
		{elements[41].symbol, &elements[41]},
		{elements[42].symbol, &elements[42]},
		{elements[43].symbol, &elements[43]},
		{elements[44].symbol, &elements[44]},
		{elements[45].symbol, &elements[45]},
		{elements[46].symbol, &elements[46]},
		{elements[47].symbol, &elements[47]},
		{elements[48].symbol, &elements[48]},
		{elements[49].symbol, &elements[49]},
		{elements[50].symbol, &elements[50]},
		{elements[51].symbol, &elements[51]},
		{elements[52].symbol, &elements[52]},
		{elements[53].symbol, &elements[53]},
		{elements[54].symbol, &elements[54]},
		{elements[55].symbol, &elements[55]},
		{elements[56].symbol, &elements[56]},
		{elements[57].symbol, &elements[57]},
		{elements[58].symbol, &elements[58]},
		{elements[59].symbol, &elements[59]},
		{elements[60].symbol, &elements[60]},
		{elements[61].symbol, &elements[61]},
		{elements[62].symbol, &elements[62]},
		{elements[63].symbol, &elements[63]},
		{elements[64].symbol, &elements[64]},
		{elements[65].symbol, &elements[65]},
		{elements[66].symbol, &elements[66]},
		{elements[67].symbol, &elements[67]},
		{elements[68].symbol, &elements[68]},
		{elements[69].symbol, &elements[69]},
		{elements[70].symbol, &elements[70]},
		{elements[71].symbol, &elements[71]},
		{elements[72].symbol, &elements[72]},
		{elements[73].symbol, &elements[73]},
		{elements[74].symbol, &elements[74]},
		{elements[75].symbol, &elements[75]},
		{elements[76].symbol, &elements[76]},
		{elements[77].symbol, &elements[77]},
		{elements[78].symbol, &elements[78]},
		{elements[79].symbol, &elements[79]},
		{elements[80].symbol, &elements[80]},
		{elements[81].symbol, &elements[81]},
		{elements[82].symbol, &elements[82]},
		{elements[83].symbol, &elements[83]},
		{elements[84].symbol, &elements[84]},
		{elements[85].symbol, &elements[85]},
		{elements[86].symbol, &elements[86]},
		{elements[87].symbol, &elements[87]},
		{elements[88].symbol, &elements[88]},
		{elements[89].symbol, &elements[89]},
		{elements[90].symbol, &elements[90]},
		{elements[91].symbol, &elements[91]},
		{elements[92].symbol, &elements[92]},
		{elements[93].symbol, &elements[93]},
		{elements[94].symbol, &elements[94]},
		{elements[95].symbol, &elements[95]},
		{elements[96].symbol, &elements[96]},
		{elements[97].symbol, &elements[97]},
		{elements[98].symbol, &elements[98]},
		{elements[99].symbol, &elements[99]},
		{elements[100].symbol, &elements[100]},
		{elements[101].symbol, &elements[101]},
		{elements[102].symbol, &elements[102]},
		{elements[103].symbol, &elements[103]},
		{elements[104].symbol, &elements[104]},
		{elements[105].symbol, &elements[105]},
		{elements[106].symbol, &elements[106]},
		{elements[107].symbol, &elements[107]},
		{elements[108].symbol, &elements[108]},
		{elements[109].symbol, &elements[109]},
		{elements[110].symbol, &elements[110]},
		{elements[111].symbol, &elements[111]},
		{elements[112].symbol, &elements[112]},
		{elements[113].symbol, &elements[113]},
		{elements[114].symbol, &elements[114]},
		{elements[115].symbol, &elements[115]},
		{elements[116].symbol, &elements[116]},
		{elements[117].symbol, &elements[117]},
		{elements[118].symbol, &elements[118]},
	};

	// #define _DEFINE_MOLECULE(x) {#x, Molecule(#x)},

	// std::unordered_map<std::string_view, Molecule> Molecule::molecules {
	// 	_DEFINE_MOLECULE(H2)
	// 	_DEFINE_MOLECULE(N)
	// 	_DEFINE_MOLECULE(O2)
	// 	_DEFINE_MOLECULE(CO2)
	// 	_DEFINE_MOLECULE(H2O)
	// 	_DEFINE_MOLECULE(O)
	// 	_DEFINE_MOLECULE(Si)
	// 	_DEFINE_MOLECULE(Al)
	// 	_DEFINE_MOLECULE(Fe)
	// 	_DEFINE_MOLECULE(Ca)
	// 	_DEFINE_MOLECULE(Na)
	// 	_DEFINE_MOLECULE(NaCl)
	// 	_DEFINE_MOLECULE(K)
	// 	_DEFINE_MOLECULE(Mg)
	// 	_DEFINE_MOLECULE(Ti)
	// };
	
#endif
