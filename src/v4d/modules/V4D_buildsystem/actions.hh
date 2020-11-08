#pragma once

namespace networking::actions {
	typedef uint8_t Action;
	
	// Client -> Server
	const Action CREATE_NEW_BUILD = 1;
	const Action ADD_BLOCK_TO_BUILD = 2;
	const Action REMOVE_BLOCK_FROM_BUILD = 3;
	
}
