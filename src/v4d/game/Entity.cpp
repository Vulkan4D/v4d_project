#include "Entity.h"

V4D_ENTITY_DEFINE_CLASS_MAP(ServerSideEntity)
	V4D_ENTITY_DEFINE_COMPONENT(ServerSideEntity, Rigidbody, rigidbody)
	bool ServerSideEntity::colliderCacheValid = false;

V4D_ENTITY_DEFINE_CLASS_MAP(ClientSideEntity)
	// V4D_ENTITY_DEFINE_COMPONENT_MAP(ClientSideEntity, v4d::TextID, Renderable, renderable)

V4D_ENTITY_DEFINE_CLASS_MAP(ServerSidePlayer)

// V4D_ENTITY_DEFINE_CLASS(RenderableGeometry)
