#include "config.hh"
#include "v4d.h"

int main(int argc, char** args) {

	// Events
	v4d::event::V4D_CORE_INIT << [](v4d::CoreInitEvent& e){
		LOG("Initialized V4D")
	};
	v4d::event::V4D_CORE_DESTROY << [](v4d::CoreDestroyEvent& e){
		LOG("Destroyed V4D")
	};

	// V4D Project
	{
		// Instantiate a v4d::CoreInstance as v4dCore
		V4D_PROJECT_INSTANTIATE_CORE_IN_MAIN( v4dCore )

		// Load Project-required Modules and Systems here
		//...

		// Do Project-specific init here
		std::cout << "Hello V4D !" << std::endl;
		//...

	} // v4dCore is supposed to get destroyed here

	std::cin.get(); // pause execution until key press, then exit program
}
