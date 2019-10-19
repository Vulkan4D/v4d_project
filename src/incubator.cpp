#include <common/pch.hh>
#include "config.hh"
#include <v4d.h>


namespace v4d::io {
	class ConfigFile : public FilePath {
	public:
		enum CONFIGFILETYPE {
			AUTO,
			INI,
			// YAML,
		} type;

		ConfigFile(const std::string& filePath, CONFIGFILETYPE type = AUTO) : FilePath(filePath), type(type) {
			AutoCreateFile();
		}

	};
}


int main() {



}
