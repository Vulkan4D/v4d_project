#include "v4d/core/tests.cxx"

namespace MyProject {
	int MyUnitTest1() {
		//...
		return 0; // return code 0 for success, anything else for failure
	}
}

START_UNIT_TESTS

	RUN_UNIT_TESTS( V4D_CORE )
	RUN_UNIT_TESTS( MyProject::MyUnitTest1 )

END_UNIT_TESTS
