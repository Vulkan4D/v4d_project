#include <v4d.h>

namespace v4d::tests {
	int MODULES_TEST_1(v4d_core v4dCore) {

		v4dCore->SetProjectName("Test Project");

		auto testModule = v4dCore->LoadModule("test");
		auto sampleModule = v4dCore->LoadModule("sample");
		if (!sampleModule) {
			LOG_ERROR("ERROR : Could not load sample module library")
			return 1;
		}

		LOAD_DLL_FUNC(sampleModule, int, test1, int)
		if (!test1) {
			LOG_ERROR("Error getting symbol pointer for function test1 in sample module : " << LOAD_DLL_ERR)
			return 2;
		}
		
		int res = test1(4);
		if (res != 8) {
			LOG_ERROR("ERROR : test1 from sample module returned " << res << " instead of 8")
			return 3;
		}

		LOAD_DLL_FUNC(sampleModule, void, test2)
		if (test2) {
			LOG_ERROR("ERROR: function test2 was supposed to cause an error because it does not exist")
			return 4;
		}

		LOAD_DLL_FUNC(testModule, int, test)
		if (!test) {
			LOG_ERROR("ERROR: function test does not exist in test module")
			return 5;
		}
		int a = test();
		if (a != 198) {
			LOG_ERROR("ERROR: function test returned " << a << " instead of 198")
			return 6;
		}
		
		return 0;
	}
}
