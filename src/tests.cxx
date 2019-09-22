#include "config.hh"
#include "v4d.h"

#include "v4d/core/tests.cxx"
#include "v4d/modules/sample/tests.cxx"

namespace MyProject {
	int MyUnitTest1(v4d_core v4dCore) {
		// return 0 for succes, anything else for failure
		return 0;
	}
}

START_UNIT_TESTS

	RUN_UNIT_TESTS( V4D_CORE )
	RUN_UNIT_TESTS( MODULES_SAMPLE_1 , v4dCore )
	RUN_UNIT_TESTS( MyProject::MyUnitTest1 , v4dCore )


END_UNIT_TESTS
