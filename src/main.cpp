#include "v4d.h"

int main(int argc, char** args) {
	v4d::event::V4D_CORE_INIT << [](void*){LOG("Initialized V4D")};

	v4d::Init();
	
	std::cout << "Hello V4D !" << std::endl;

	std::cin.get();
}
