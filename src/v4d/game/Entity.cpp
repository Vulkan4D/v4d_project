#include "Entity.h"

V4D_ENTITY_DEFINE_CLASS_MAP(ServerSideEntity)
	V4D_ENTITY_DEFINE_COMPONENT(ServerSideEntity, Rigidbody, rigidbody)

V4D_ENTITY_DEFINE_CLASS_MAP(ClientSideEntity)
	// V4D_ENTITY_DEFINE_COMPONENT_MAP(ClientSideEntity, std::string_view, Renderable, renderable)

V4D_ENTITY_DEFINE_CLASS_MAP(ServerSidePlayer)
