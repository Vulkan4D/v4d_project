#include <v4d.h>
#include <V4D_Mod.h>

int main(const int argc, const char** argv) {
	int ret = 0;
	if (argc > 1) {
		if (auto andromeda = V4D_Mod::LoadModule(argv[1]); andromeda) {
			if (andromeda->RunFromConsole) {
				ret = andromeda->RunFromConsole(argc-2, argv+2);
			} else {
				std::cout << "Function RunFromConsole() was not found in module " << argv[1] << std::endl;
				ret = -1;
			}
			V4D_Mod::UnloadModule(argv[1]);
		} else {
			std::cout << "Failed to load module " << argv[1] << std::endl;
			ret = -1;
		}
	} else {
		std::cout << "Calls the RunFromConsole() function of a V4D_Mod submodule.\n\nUsage:\n ./console <ModuleName> [args...]\n\n" << std::endl;
		ret = -1;
	}
	return ret;
}
