#pragma once
#include "app.hh"
#include "networking.hh"
#include "Server.hpp"
#include "Client.hpp"

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
			V4D_Objects::LoadModule(module);
			V4D_Game::LoadModule(module);
			V4D_Input::LoadModule(module);
			V4D_Renderer::LoadModule(module);
			V4D_Physics::LoadModule(module);
			V4D_Server::LoadModule(module);
			V4D_Client::LoadModule(module);
		}
		if (!V4D_Renderer::GetPrimaryModule()) { // We need at least one primary Renderer module
			V4D_Renderer::LoadModule(APP_DEFAULT_RENDERER_MODULE);
		}
		// Sort Modules
		V4D_Objects::SortModules([](auto* a, auto* b){
			return (a->OrderIndex? a->OrderIndex():0) < (b->OrderIndex? b->OrderIndex():0);
		});
		V4D_Game::SortModules([](auto* a, auto* b){
			return (a->OrderIndex? a->OrderIndex():0) < (b->OrderIndex? b->OrderIndex():0);
		});
		V4D_Input::SortModules([](auto* a, auto* b){
			return (a->OrderIndex? a->OrderIndex():0) < (b->OrderIndex? b->OrderIndex():0);
		});
		V4D_Renderer::SortModules([](auto* a, auto* b){
			return (a->OrderIndex? a->OrderIndex():0) < (b->OrderIndex? b->OrderIndex():0);
		});
		V4D_Physics::SortModules([](auto* a, auto* b){
			return (a->OrderIndex? a->OrderIndex():0) < (b->OrderIndex? b->OrderIndex():0);
		});
		V4D_Server::SortModules([](auto* a, auto* b){
			return (a->OrderIndex? a->OrderIndex():0) < (b->OrderIndex? b->OrderIndex():0);
		});
		V4D_Client::SortModules([](auto* a, auto* b){
			return (a->OrderIndex? a->OrderIndex():0) < (b->OrderIndex? b->OrderIndex():0);
		});
	}
	void Unload() {
		V4D_Objects::UnloadModules();
		V4D_Game::UnloadModules();
		V4D_Input::UnloadModules();
		V4D_Renderer::UnloadModules();
		V4D_Physics::UnloadModules();
		V4D_Server::UnloadModules();
		V4D_Client::UnloadModules();
	}

	void Init() {
		if (app::renderer) {
			V4D_Renderer::ForEachSortedModule([](auto* mod){
				if (mod->Init) mod->Init(app::renderer, app::scene);
			});
		}
		V4D_Objects::ForEachSortedModule([](auto* mod){
			if (mod->Init) mod->Init();
		});
		V4D_Game::ForEachSortedModule([](auto* mod){
			if (mod->Init) mod->Init(app::scene);
		});
		V4D_Physics::ForEachSortedModule([](auto* mod){
			if (mod->Init) mod->Init(app::renderer, app::scene);
		});
		if (app::window && app::renderer) {
			V4D_Input::ForEachSortedModule([](auto* mod){
				if (mod->Init) mod->Init(app::window, app::renderer, app::scene);
			});
		}
	}
	void InitServer(std::shared_ptr<app::Server> server) {
		V4D_Server::ForEachSortedModule([server](auto* mod){
			if (mod->Init) mod->Init(server, app::scene);
		});
	}
	void InitClient(std::shared_ptr<app::Client> client) {
		V4D_Client::ForEachSortedModule([client](auto* mod){
			if (mod->Init) mod->Init(client, app::scene);
		});
	}

	void LoadScene() {
		V4D_Physics::ForEachSortedModule([](auto* mod){
			if (mod->LoadScene) mod->LoadScene();
		});
		if (app::renderer) {
			V4D_Renderer::ForEachSortedModule([](auto* mod){
				if (mod->LoadScene) mod->LoadScene();
			});
		}
		V4D_Game::ForEachSortedModule([](auto* mod){
			if (mod->LoadScene) mod->LoadScene();
		});
		V4D_Server::ForEachSortedModule([](auto* mod){
			if (mod->LoadScene) mod->LoadScene();
		});
		V4D_Client::ForEachSortedModule([](auto* mod){
			if (mod->LoadScene) mod->LoadScene();
		});
	}
	void UnloadScene() {
		V4D_Client::ForEachSortedModule([](auto* mod){
			if (mod->UnloadScene) mod->UnloadScene();
		});
		V4D_Server::ForEachSortedModule([](auto* mod){
			if (mod->UnloadScene) mod->UnloadScene();
		});
		V4D_Game::ForEachSortedModule([](auto* mod){
			if (mod->UnloadScene) mod->UnloadScene();
		});
		if (app::renderer) {
			V4D_Renderer::ForEachSortedModule([](auto* mod){
				if (mod->UnloadScene) mod->UnloadScene();
			});
		}
		V4D_Physics::ForEachSortedModule([](auto* mod){
			if (mod->UnloadScene) mod->UnloadScene();
		});
	}

}
