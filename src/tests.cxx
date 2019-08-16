#include "v4d/core/tests.cxx"
#include "v4d/systems/sample/tests.cxx"

namespace v4d::tests {
	int CHECK_V4D_VERSION() { // check whether the version of v4d.dll matches v4d.h
		return GET_V4D_CORE_BUILD_VERSION() == V4D_VERSION ? 0:1;
	}
}

namespace MyProject {
	int MyUnitTest1() {
		// return 0 for succes, anything else for failure
		return 0;
	}
}

START_UNIT_TESTS

	RUN_UNIT_TESTS( CHECK_V4D_VERSION )
	RUN_UNIT_TESTS( V4D_CORE )
	RUN_UNIT_TESTS( SYSTEMS_SAMPLE_1 )
	RUN_UNIT_TESTS( MyProject::MyUnitTest1 )


END_UNIT_TESTS
