#pragma once
#include "app.hh"

namespace app::events {

	void Bind() {
		
		// Core Event Bindings
		v4d::event::APP_KILLED << [](int signal){
			LOG("Process has been killed by signal " << signal)
		};
		
		v4d::event::APP_ERROR << [](int signal){
			LOG_ERROR("Process signaled error " << signal)
		};
		
	}

}
