struct Collision {
	uint32_t collisionInstance; // objectInstanceA | collision Flags
	uint32_t objectInstanceB; // objectInstanceB | collision Ray Mask
	
	int32_t objectGeometryA;
	int32_t objectGeometryB;
	
	vec4 position; // view-space, w = depth (how much under the object's surface this start position is)
	vec4 direction; // view-space direction, w = max travel distance from surface (typically velocity*deltaTime)
	
	vec4 contactA;
	vec4 contactB;
	
	vec4 normalA;
	vec4 normalB;
};

layout(set = 1, binding = 0, std430) buffer CollisionBuffer { Collision collisions[]; };

struct CollisionPayload {
	int32_t entityInstanceIndex;
	int32_t geometryIndex;
	vec4 hit; // xyz = view-space position, w = hit distance
	vec3 normal;
};

