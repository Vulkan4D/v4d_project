struct Collision {
	int32_t objectInstanceA;
	int32_t objectGeometryA;
	int32_t objectInstanceB;
	int32_t objectGeometryB;
	
	vec4 startPosition; // view-space, w = max distance to surface (typically boundingRadius)
	vec4 velocity; // view-space, w = max travel distance from surface (typically velocity*deltaTime)
	
	vec4 contactA;
	vec4 contactB;
};

layout(set = 1, binding = 0, std430) buffer CollisionBuffer { Collision collisions[]; };
