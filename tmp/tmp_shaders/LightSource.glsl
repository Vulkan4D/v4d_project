layout(std430, push_constant) uniform LightSource {
	dvec3 worldPosition;
	vec3 color;
	float intensity;
	vec3 worldDirection;
	int type; // 0 = point
	vec3 viewPosition;
	float innerAngle;
	vec3 viewDirection;
	float outerAngle;
} lightSource;
