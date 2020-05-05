// Metadata
#define _V4D_MODULE
#define THIS_MODULE_VENDOR_ID 1
#define THIS_MODULE_ID 1
#define THIS_MODULE_REV 1
#define THIS_MODULE_NAME "Test"
#define THIS_MODULE_DESCRIPTION "Test Module"

// V4D Core Header
#include <v4d.h>
using namespace v4d::modules;

#define SAMPLE_MODULE_ID V4D_MODULE_ID(123, 12345)

V4DMODULE int test() {
	
	int result = 0;
	
	ModuleInstance::Get(SAMPLE_MODULE_ID)->ForEachSubmodule<Test>([&](auto* testSubmodule){
		result = +testSubmodule->Run();
	});
	
	for (auto[id, module] : ModuleInstance::GetLoadedModules()) {
		module->ForEachSubmodule<Test>([&](auto* testSubmodule){
			result += testSubmodule->Run();
		});
	}
	
	return result;
}

