#include "config.hh"
#include "v4d.h"

int main(int argc, char** args) {

	v4d::event::V4D_CORE_INIT << [](v4d::CoreInitEvent& e){
		LOG("Initialized V4D")
	};

	v4d::event::V4D_CORE_DESTROY << [](v4d::CoreDestroyEvent& e){
		LOG("Destroyed V4D")
	};

	v4d::Init();

	std::cout << "Hello V4D !" << std::endl;

	std::cin.get();

	v4d::Destroy();
}
