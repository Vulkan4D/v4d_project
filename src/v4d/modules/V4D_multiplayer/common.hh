#pragma once

typedef std::unordered_map<uint32_t /* id */, v4d::scene::NetworkGameObjectPtr> ClientObjects;

namespace OBJECT_TYPE {
	const uint32_t Player = 0;
	const uint32_t Ball = 1;
	const uint32_t Light = 2;
}
