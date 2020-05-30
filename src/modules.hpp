#pragma once
#include "globalscope.hh"

namespace app::modules {

	void Load() {
		if (app::settings->modules_list_file != "") {
			app::modulesList = v4d::io::StringListFile::Instance(app::settings->modules_list_file)->Load();
			
			// Default Modules when modules.txt is empty
			if (app::modulesList.size() == 0) {
				app::modulesList.push_back("V4D_basicscene");
				app::modulesList.push_back("V4D_bullet");
				// app::modulesList.push_back("V4D_planetdemo");
			}
		}
		for (auto module : app::modulesList) {
			V4D_Game::LoadModule(module);
			V4D_Input::LoadModule(module);
			V4D_Renderer::LoadModule(module);
			V4D_Physics::LoadModule(module);
		}
		if (!V4D_Renderer::GetPrimaryModule()) { // We need at least one primary Renderer module
			V4D_Renderer::LoadModule("V4D_hybrid");
		}
		// Sort Modules
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
	}
	void Unload() {
		V4D_Game::UnloadModules();
		V4D_Input::UnloadModules();
		V4D_Renderer::UnloadModules();
		V4D_Physics::UnloadModules();
	}

	void Init() {
		if (app::renderer) {
			V4D_Renderer::ForEachSortedModule([](auto* mod){
				if (mod->Init) mod->Init(app::renderer, app::scene);
			});
		}
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
	}
	void UnloadScene() {
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
