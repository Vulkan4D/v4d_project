#pragma once

#define CUSTOM_OBJECT_DATA_INITIAL_STREAM_SIZE 256 // anything between 128 and 768 should be safe and fast
#define CUSTOM_OBJECT_TRANSFORM_DATA_MAX_STREAM_SIZE 254 // maximum of 254 because we do not want to use more than one byte for the stream size info (since in ZAP definition a size info of 255 means that we are expecting another 8 bytes for a full 64-bit size_t)

namespace OBJECT_TYPE {
	const uint32_t Player = 0;
	const uint32_t Ball = 1;
	const uint32_t Light = 2;
}
