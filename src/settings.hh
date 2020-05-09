#pragma once

#include <v4d.h>

// Settings
struct ProjectSettings : public v4d::io::ConfigFile {
	CONFIGFILE_STRUCT(ProjectSettings)

	// Application Settings
	bool log_verbose = false;
	std::string modules_list_file = "modules.txt";
	
	// Graphics
	int framerate_limit_rendering = 60;
	int framerate_limit_ui = 30;

private:
	void ReadConfig() override {
		CONFIGFILE_READ_FROM_INI_WRITE("application", log_verbose)
		CONFIGFILE_READ_FROM_INI_WRITE("application", modules_list_file)
		CONFIGFILE_READ_FROM_INI_WRITE("graphics", framerate_limit_rendering)
		CONFIGFILE_READ_FROM_INI_WRITE("graphics", framerate_limit_ui)
		
		LOGGER_INSTANCE->SetVerbose(log_verbose);
	}
	void WriteConfig() override {
		CONFIGFILE_WRITE_TO_INI("application", log_verbose)
		CONFIGFILE_WRITE_TO_INI("application", modules_list_file)
		CONFIGFILE_WRITE_TO_INI("graphics", framerate_limit_rendering)
		CONFIGFILE_WRITE_TO_INI("graphics", framerate_limit_ui)
	}
};
