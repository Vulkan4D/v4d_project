// Project Config
#include "config.hh"

#define LOGGER_PREFIX " [tests] "

// V4D Core Header
#include <v4d.h>

// Other tests
#include "v4d/core/tests.cxx"
#include "v4d/modules/test/tests.cxx"

namespace MyProject {
	int MyUnitTest1(v4d_core) {
		// return 0 for succes, anything else for failure
		return 0;
	}
}


START_UNIT_TESTS

	LOGGER_INSTANCE->SetVerbose(false);

	RUN_UNIT_TESTS( V4D_CORE )
	RUN_UNIT_TESTS( MODULES_TEST_1 , v4dCore )
	RUN_UNIT_TESTS( MyProject::MyUnitTest1 , v4dCore )


END_UNIT_TESTS
