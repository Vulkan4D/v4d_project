#include "v4d.h"

// v4d::event::V4D_CORE_INIT << []{LOG("Initialized V4D")};

int main(int argc, char** args) {
	v4d::Init();
	std::cout << "Hello V4D !" << std::endl;

	std::cin.get();
}
