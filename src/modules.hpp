#pragma once
#include "app.hh"
#include "networking.hh"
#include "Server.hpp"
#include "Client.hpp"
#include "utilities/io/StringListFile.h"

namespace app::modules {

	void Load() {
		if (app::settings->modules_list_file != "") {
			// Load modules list from modules.txt when given list is empty
			if (app::modulesList.size() == 0) {
				app::modulesList = v4d::io::StringListFile::Instance(app::settings->modules_list_file)->Load();
			}
			
			// Default Modules when modules.txt is empty
			if (app::modulesList.size() == 0) {
				app::modulesList = {APP_DEFAULT_MODULES};
				#ifdef APP_DEFAULT_MODULES_SAVE_TO_MODULES_TXT_WHEN_EMPTY
					auto file = v4d::io::StringListFile::Instance(app::settings->modules_list_file);
					file->lines = app::modulesList;
					file->WriteToFile();
				#endif
			}
		}
		for (auto module : app::modulesList) if (module[0] != '#') {
			LOG("Loading module " << module)
			V4D_Mod::LoadModule(module);
		}
		// Sort Modules
		V4D_Mod::SortModules([](auto* a, auto* b){
			return (a->OrderIndex? a->OrderIndex():0) < (b->OrderIndex? b->OrderIndex():0);
		});
	}
	void Unload() {
		V4D_Mod::UnloadModules();
	}

	void InitWindow() {
		if (app::window) {
			V4D_Mod::ForEachSortedModule([](auto* mod){
				if (mod->InitWindow) mod->InitWindow(app::window);
			});
		}
	}
	void InitRenderer() {
		if (app::renderer) {
			V4D_Mod::ForEachSortedModule([](auto* mod){
				if (mod->InitRenderer) mod->InitRenderer(app::renderer);
			});
		}
	}
	void InitServer(std::shared_ptr<app::Server> server) {
		V4D_Mod::ForEachSortedModule([server](auto* mod){
			if (mod->InitServer) mod->InitServer(server);
		});
	}
	void InitClient(std::shared_ptr<app::Client> client) {
		V4D_Mod::ForEachSortedModule([client](auto* mod){
			if (mod->InitClient) mod->InitClient(client);
		});
	}

	void LoadScene() {
		V4D_Mod::ForEachSortedModule([](auto* mod){
			if (mod->LoadScene) mod->LoadScene(app::scene);
		});
	}
	void UnloadScene() {
		V4D_Mod::ForEachSortedModule([](auto* mod){
			if (mod->UnloadScene) mod->UnloadScene();
		});
	}

}
