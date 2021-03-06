#pragma once

namespace networking::action {
	typedef uint8_t Action;
	// const Action EXTENDED_ACTION = 0;

	// Action stream Only (TCP)
	const Action ADD_ENTITY = 1;
	const Action REMOVE_ENTITY = 2;
	const Action UPDATE_ENTITY = 3;

	// Burst streams only (TCP & UDP)
	const Action SYNC_ENTITY_TRANSFORM = 16;
	const Action SYNC_GROUPED_ENTITIES_POSITIONS_ROTATIONS = 17;
	const Action SYNC_GROUPED_ENTITIES_POSITIONS = 18;
	const Action SYNC_GROUPED_ENTITIES_ROTATIONS = 19;

}
