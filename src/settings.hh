#pragma once

#include <v4d.h>

// Settings
struct ProjectSettings : public v4d::io::ConfigFile {
	CONFIGFILE_STRUCT(ProjectSettings)

	// Application Settings
	bool log_verbose = false;


private:
	void ReadConfig() override {
		CONFIGFILE_READ_FROM_INI_WRITE("application", log_verbose)
		LOGGER_INSTANCE->SetVerbose(log_verbose);
	}
	void WriteConfig() override {
		CONFIGFILE_WRITE_TO_INI("application", log_verbose)
	}
};
