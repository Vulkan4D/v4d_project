#include "config.hh"
#include "v4d.h"

// Application start
int main() {

	// Core Event Bindings
	v4d::event::V4D_CORE_INIT << [](v4d::CoreInitEvent&){
		LOG("Initialized V4D")
	};
	v4d::event::V4D_CORE_DESTROY << [](v4d::CoreDestroyEvent&){
		LOG("Destroyed V4D")
	};


	// Instantiate a v4d::Core as v4dCore
	V4D_PROJECT_INSTANTIATE_CORE_IN_MAIN ( v4dCore )


	// Do Project-specific init here
	LOG("Hello V4D !")
	//...


	std::cin.get(); // pause execution until key press, then exit program
}
