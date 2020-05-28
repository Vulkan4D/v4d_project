#include <v4d.h>

class V4D_Test {
	V4D_MODULE_CLASS_HEADER(V4D_Test, init, test1, test2)
	V4D_MODULE_FUNC_DECLARE(void, init)
	V4D_MODULE_FUNC_DECLARE(int, test1, double)
	V4D_MODULE_FUNC_DECLARE(int, test2)
};

V4D_MODULE_CLASS_CPP(V4D_Test);

namespace v4d::tests {
	int MODULES_TEST_1() {
		int res = 0;

		V4D_Test::ModulesSetLoadCallback([](auto* module){
			module->init();
		});
		
		auto* testModule1 = V4D_Test::LoadModule("V4D_test");
		if (!testModule1) {
			LOG_ERROR("ERROR : Could not load test module shared library")
			return 1;
		}
		
		if (!testModule1->test1) {
			LOG_ERROR("Error getting symbol pointer for function test1 in test module")
			return 2;
		}
		
		if (!testModule1->test2) {
			LOG_ERROR("Error getting symbol pointer for function test1 in test module")
			return 3;
		}
		
		res += testModule1->test1(3.46);
		res += testModule1->test2();
		
		V4D_Test::UnloadModule("V4D_test");
		
		return res;
	}
}
