// V4D Core Header
#include <v4d.h>

// Settings file
#include "settings.hh"
auto settings = ProjectSettings::Instance("settings.ini", 1000);

// Application start
int main() {
	if (!v4d::Init()) return -1;

	// Load settings
	settings->Load();

	// Core Event Bindings
	v4d::event::APP_KILLED << [](int signal){
		LOG("Process has been killed by signal " << signal)
	};
	v4d::event::APP_ERROR << [](int signal){
		LOG_ERROR("Process signaled error " << signal)
	};


	// Do Project-specific init here
	LOG("Hello V4D !")
	//...


	std::cin.get(); // pause execution until key press, then exit program
}
