#pragma once

namespace networking::action {
	typedef uint8_t Action;
	// const Action EXTENDED_ACTION = 0;

	// Action stream Only (TCP)
	const Action ADD_OBJECT = 1; // + (moduleID, objectType, parentID, objectID, physicsControl, attributes, iteration, NetworkGameObjectTransform) = 158 bytes + CustomObjectData + CustomTransformData
	const Action REMOVE_OBJECT = 2; // + (objectID) = 4 bytes
	const Action UPDATE_OBJECT = 3; // + (parentID, objectID, physicsControl, attributes, iteration, NetworkGameObjectTransform) = 137 bytes + CustomObjectData + CustomTransformData
	const Action CUSTOM = 4; // + (string) = variable size
	const Action ASSIGN = 5; // + (objectID) = 4

	// Burst streams only (TCP & UDP)
	const Action SYNC_OBJECT_TRANSFORM = 16; // + (objectID, iteration, NetworkGameObjectTransform) = 128 bytes + CustomTransformData (+ 4 bytes remaining for future use)

}
