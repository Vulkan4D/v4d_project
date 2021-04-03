#pragma once
#include <v4d.h>
#include "utilities/io/ConfigFile.h"
#include <string>

// Settings
struct ProjectSettings : public v4d::io::ConfigFile {
	CONFIGFILE_STRUCT(ProjectSettings)

	// Application Settings
	#ifdef _DEBUG
		bool log_verbose = true;
	#else
		bool log_verbose = false;
	#endif
	std::string modules_list_file = "modules.txt";
	
	// Graphics
	int framerate_limit_rendering = 0;
	int framerate_limit_ui = 30;

	// Networking
	int default_server_port = 0;
	bool bursts_force_tcp = false;
	double bursts_server_max_send_fps = 15;
	double bursts_client_max_send_fps = 25;
	
	// Physics
	int framerate_limit_physics = 200;
	
private:
	void ReadConfig() override {
		CONFIGFILE_READ_FROM_INI_WRITE(
			"application"
			, log_verbose
			, modules_list_file
		)
		CONFIGFILE_READ_FROM_INI_WRITE(
			"graphics"
			, framerate_limit_rendering
			, framerate_limit_ui
		)
		CONFIGFILE_READ_FROM_INI_WRITE(
			"networking"
			, default_server_port
			, bursts_force_tcp
			, bursts_server_max_send_fps
			, bursts_client_max_send_fps
		)
		CONFIGFILE_READ_FROM_INI_WRITE(
			"physics"
			, framerate_limit_physics
		)
		
		LOGGER_INSTANCE->SetVerbose(log_verbose);
	}
	void WriteConfig() override {
		CONFIGFILE_WRITE_TO_INI(
			"application"
			, log_verbose
			, modules_list_file
		)
		CONFIGFILE_WRITE_TO_INI(
			"graphics"
			, framerate_limit_rendering
			, framerate_limit_ui
		)
		CONFIGFILE_WRITE_TO_INI(
			"networking"
			, default_server_port
			, bursts_force_tcp
			, bursts_server_max_send_fps
			, bursts_client_max_send_fps
		)
		CONFIGFILE_WRITE_TO_INI(
			"physics"
			, framerate_limit_physics
		)
	}
};
