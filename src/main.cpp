// V4D Core Header
#include <v4d.h>

// Settings file
#include "settings.hh"
auto settings = ProjectSettings::Instance("settings.ini", 1000);

// Application start
int main() {
	
	// Load settings
	settings->Load();

	// Core Event Bindings
	v4d::event::V4D_CORE_INIT << [](v4d::CoreInitEvent&){
		LOG("Initialized V4D")
	};
	v4d::event::V4D_CORE_DESTROY << [](v4d::CoreDestroyEvent&){
		LOG("Destroyed V4D")
	};
	v4d::event::APP_KILLED << [](int signal){
		LOG("Process has been killed by signal " << signal)
	};
	v4d::event::APP_ERROR << [](int signal){
		LOG_ERROR("Process signaled error " << signal)
	};


	// Instantiate a v4d::Core as v4dCore
	V4D_PROJECT_INSTANTIATE_CORE_IN_MAIN ( v4dCore )


	// Do Project-specific init here
	LOG("Hello V4D !")
	//...


	std::cin.get(); // pause execution until key press, then exit program
}
