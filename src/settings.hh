#pragma once
#include "app.hh"

// Settings
struct ProjectSettings : public v4d::io::ConfigFile {
	CONFIGFILE_STRUCT(ProjectSettings)

	// Application Settings
	bool log_verbose = false;
	std::string modules_list_file = "modules.txt";
	
	// Graphics
	int framerate_limit_rendering = 0;
	#ifdef RENDER_SECONDARY_IN_ANOTHER_THREAD
		int framerate_limit_ui = 30;
	#endif

	// Networking
	int default_server_port = 0;
	
private:
	void ReadConfig() override {
		CONFIGFILE_READ_FROM_INI_WRITE("application", log_verbose)
		CONFIGFILE_READ_FROM_INI_WRITE("application", modules_list_file)
		CONFIGFILE_READ_FROM_INI_WRITE("graphics", framerate_limit_rendering)
		#ifdef RENDER_SECONDARY_IN_ANOTHER_THREAD
			CONFIGFILE_READ_FROM_INI_WRITE("graphics", framerate_limit_ui)
		#endif
		CONFIGFILE_READ_FROM_INI_WRITE("networking", default_server_port)
		
		LOGGER_INSTANCE->SetVerbose(log_verbose);
	}
	void WriteConfig() override {
		CONFIGFILE_WRITE_TO_INI("application", log_verbose)
		CONFIGFILE_WRITE_TO_INI("application", modules_list_file)
		CONFIGFILE_WRITE_TO_INI("graphics", framerate_limit_rendering)
		#ifdef RENDER_SECONDARY_IN_ANOTHER_THREAD
			CONFIGFILE_WRITE_TO_INI("graphics", framerate_limit_ui)
		#endif
		CONFIGFILE_WRITE_TO_INI("networking", default_server_port)
	}
};
