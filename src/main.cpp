#include "config.hh"
#include "v4d.h"

// Logger
#define LOGGER_INSTANCE v4d::io::Logger::ConsoleInstance()

// Application start
int main(int argc, char** args) {

	// Core Event Bindings
	v4d::event::V4D_CORE_INIT << [](v4d::CoreInitEvent& e){
		LOG("Initialized V4D")
	};
	v4d::event::V4D_CORE_DESTROY << [](v4d::CoreDestroyEvent& e){
		LOG("Destroyed V4D")
	};


	// Instantiate a v4d::Core as v4dCore
	V4D_PROJECT_INSTANTIATE_CORE_IN_MAIN( v4dCore, LOGGER_INSTANCE )


	// Do Project-specific init here
	std::cout << "Hello V4D !" << std::endl;
	//...


	std::cin.get(); // pause execution until key press, then exit program
}
