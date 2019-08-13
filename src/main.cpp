#include "v4d.h"

#ifdef _DEBUG
	#include "v4d/core/tests.cxx"
#endif

int main(int argc, char **args) {
    #ifdef _DEBUG
        return v4d::tests::RunAllTests();
    #endif
}
