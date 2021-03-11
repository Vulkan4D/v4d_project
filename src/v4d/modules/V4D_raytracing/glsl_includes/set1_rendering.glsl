layout(set = 1, binding = 0, rgba16f) uniform image2D img_lit;
layout(set = 1, binding = 1, rg32f) uniform image2D img_depth;
layout(set = 1, binding = 2, rgba32f) uniform image2D img_pos;
layout(set = 1, binding = 3, rgba32f) uniform image2D img_geometry;
layout(set = 1, binding = 4, rgba16f) uniform image2D img_albedo;
layout(set = 1, binding = 5, rgba16f) uniform image2D img_normal;

layout(set = 1, binding = 6) uniform sampler2D img_lit_history;
layout(set = 1, binding = 7) uniform sampler2D img_depth_history;
layout(set = 1, binding = 8) uniform sampler2D img_pos_history;
layout(set = 1, binding = 9) uniform sampler2D img_geometry_history;
layout(set = 1, binding = 10) uniform sampler2D img_albedo_history;
layout(set = 1, binding = 11) uniform sampler2D img_normal_history;

layout(set = 1, binding = 12) buffer writeonly RayCast {
	uint64_t moduleVen;
	uint64_t moduleId;
	uint64_t objId;
	uint64_t raycastCustomData;
	vec4 localSpaceHitPositionAndDistance; // w component is distance
	vec4 localSpaceHitSurfaceNormal; // w component is unused
} rayCast;

layout(set = 1, binding = 13) uniform samplerCube img_background;
