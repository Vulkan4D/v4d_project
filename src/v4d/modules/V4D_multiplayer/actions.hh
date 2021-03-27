#pragma once

namespace networking::action {
	typedef uint8_t Action;
	// const Action EXTENDED_ACTION = 0;

	// Action stream Only (TCP)
	const Action ADD_ENTITY = 1; // + (moduleID, objectType, parentID, objectID, physicsControl, attributes, iteration, NetworkGameObjectTransform) = 158 bytes + CustomObjectData + CustomTransformData
	const Action REMOVE_ENTITY = 2; // + (objectID) = 4 bytes
	const Action UPDATE_ENTITY = 3; // + (parentID, objectID, physicsControl, attributes, iteration, NetworkGameObjectTransform) = 137 bytes + CustomObjectData + CustomTransformData

	// Burst streams only (TCP & UDP)
	const Action SYNC_ENTITY_TRANSFORM = 16; // + (objectID, iteration, NetworkGameObjectTransform) = 128 bytes + CustomTransformData (+ 4 bytes remaining for future use)

}
