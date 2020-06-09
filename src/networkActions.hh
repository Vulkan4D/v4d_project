#pragma once

namespace app::networking {
	typedef uint8_t Action;
	namespace action {
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
}

#define CUSTOM_OBJECT_DATA_INITIAL_STREAM_SIZE 256 // anything between 128 and 768 should be safe and fast
#define CUSTOM_OBJECT_TRANSFORM_DATA_MAX_STREAM_SIZE 254 // maximum of 254 because we do not want to use more than one byte for the stream size info (since in ZAP definition a size info of 255 means that we are expecting another 8 bytes for a full 64-bit size_t)
