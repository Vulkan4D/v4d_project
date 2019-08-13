#include "v4d.h"

#include <functional>
#include <future>
#include <thread>


#ifdef _DEBUG
	#include "v4d/core/tests.cxx"
#endif

int main(int argc, char **args) {
    #ifdef _DEBUG
        return v4d::tests::RunAllTests();
    #endif
}
