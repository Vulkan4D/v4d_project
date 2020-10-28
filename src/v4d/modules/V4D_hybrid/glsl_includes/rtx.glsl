#include "rtx_base.glsl"

#if defined(RAY_TRACING)

	// Ray Tracing Payload
	struct RayPayload_visibility {
		uvec4 customData;
		vec3 viewSpacePosition;
		vec3 viewSpaceNormal;
		vec3 albedo;
		float emit;
		float uv;
		float metallic;
		float roughness;
		float distance;
	};

	// struct RayPayload_lighting {
	// 	vec3 color;
	// 	vec3 origin;
	// 	vec3 direction;
	// 	float distance;
	// };
	
#endif
