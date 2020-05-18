#include "core.glsl"

// #define MAX_PLANETS 1
// layout(set = 3, binding = 0) readonly buffer PlanetBuffer {dmat4 planets[];};
// layout(set = 3, binding = 0) readonly buffer PlanetBuffer {vec4 planets[];};

#common .*rchit

#include "rtx_base.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_rays.glsl"

#common .*comp

#include "noise.glsl"

#common .*map.comp

layout(set = 1, binding = 0, rgba32f) uniform image2D bumpMap[1];

layout(std430, push_constant) uniform Planet{
	int planetIndex;
	float planetHeightVariation;
};

vec3 GetCubeDirection(writeonly imageCube image) {
	const vec2 pixelCenter = vec2(gl_GlobalInvocationID.xy) + vec2(0.5);
	const vec2 uv = pixelCenter / imageSize(image).xy;
	vec2 d = uv * 2.0 - 1.0;
	
	vec3 direction = vec3(0);
	switch (int(gl_GlobalInvocationID.z)) {
		case 0 : // Right (POSITIVE_X)
			direction = vec3( 1, -d.y, -d.x);
		break;
		case 1 : // Left (NEGATIVE_X)
			direction = vec3(-1, -d.y, d.x);
		break;
		case 2 : // Front (POSITIVE_Y)
			direction = vec3(d.x, 1, d.y);
		break;
		case 3 : // Back (NEGATIVE_Y)
			direction = vec3(d.x,-1, -d.y);
		break;
		case 4 : // Top (POSITIVE_Z)
			direction = vec3(d.x,-d.y, 1);
		break;
		case 5 : // Bottom (NEGATIVE_Z)
			direction = vec3(-d.x,-d.y, -1);
		break;
	}
	
	return normalize(direction);
}

#common terrain.*comp|terrain.rchit

layout(set = 2, binding = 0) uniform sampler2D bumpMap[1];

#############################################################
#shader bump.altitude.map.comp
void main() {
	vec2 size = vec2(imageSize(bumpMap[0]).xy);
	float mult = 40;
	float noiseOffset = 2331.43;
	vec2 coords1 = abs((vec2(gl_GlobalInvocationID.xy) / size - 0.5) * 2.0); // -1 to +1
	vec2 coords2 = abs((vec2(gl_GlobalInvocationID.yx) / size - 0.5) * 2.0);
	float altitude1 = FastSimplexFractal(vec3(coords1 * mult, noiseOffset), 15);
	float altitude2 = FastSimplexFractal(vec3(coords2 * mult, noiseOffset), 15);
	float altitude = mix(altitude1, altitude2, min(1.0,coords1.x+coords1.y)/2.0);
	imageStore(bumpMap[0], ivec2(gl_GlobalInvocationID.xy), vec4(0,0,0, altitude));
}

#############################################################
#shader bump.normals.map.comp
void main() {
	ivec2 size = ivec2(imageSize(bumpMap[0]).xy);
	ivec2 center = ivec2(gl_GlobalInvocationID.xy);
	ivec2 top = ivec2(gl_GlobalInvocationID.xy) + ivec2(0, -1);
	ivec2 bottom = ivec2(gl_GlobalInvocationID.xy) + ivec2(0, +1);
	ivec2 right = ivec2(gl_GlobalInvocationID.xy) + ivec2(+1, 0);
	ivec2 left = ivec2(gl_GlobalInvocationID.xy) + ivec2(-1, 0);
	if (bottom.y >= size.y) bottom.y = 0;
	if (right.x >= size.x) right.x = 0;
	if (top.y < 0) top.y = size.y-1;
	if (left.x < 0) left.x = size.x-1;
	float altitude = imageLoad(bumpMap[0], ivec2(gl_GlobalInvocationID.xy)).a;
	float altitudeTop = imageLoad(bumpMap[0], top).a / 2.0 + 0.5;
	float altitudeBottom = imageLoad(bumpMap[0], bottom).a / 2.0 + 0.5;
	float altitudeRight = imageLoad(bumpMap[0], right).a / 2.0 + 0.5;
	float altitudeLeft = imageLoad(bumpMap[0], left).a / 2.0 + 0.5;
	vec3 normal = normalize(vec3(4*(altitudeRight-altitudeLeft), 4*(altitudeBottom-altitudeTop), 1));
	imageStore(bumpMap[0], ivec2(gl_GlobalInvocationID.xy), vec4(normal, altitude));
}

#############################################################
#shader terrain.rchit

hitAttributeEXT vec3 hitAttribs;
layout(location = 0) rayPayloadInEXT RayPayload ray;
layout(location = 2) rayPayloadEXT bool shadowed;

#include "rtx_pbr.glsl"
#include "rtx_fragment.glsl"

// #include "noise.glsl"

vec4 GetBumpMap(vec2 uv, vec2 uvChunk) {
	return vec4(0,0,1,0)
	//  + texture(bumpMap[0], uv)
	//  + texture(bumpMap[0], uv*256)
	 + texture(bumpMap[0], uvChunk)/2.0
	;
}

void main() {
	Fragment fragment = GetHitFragment(true);


	vec3 tangentX = normalize(cross(fragment.geometryInstance.normalViewTransform * vec3(0,1,0)/* fixed arbitrary vector in object space */, fragment.viewSpaceNormal));
	vec3 tangentY = normalize(cross(fragment.viewSpaceNormal, tangentX));
	mat3 TBN = mat3(tangentX, tangentY, fragment.viewSpaceNormal); // viewSpace TBN
	vec2 uvOffset = fragment.geometryInstance.custom3f.xy;
	vec2 uvMult = vec2(fragment.geometryInstance.custom3f.z);
	vec2 uv = (fragment.uv*uvMult+uvOffset);
	vec4 bump = GetBumpMap(uv, fragment.uv);
	vec3 normal = normalize(TBN * bump.xyz);
	vec3 color = ApplyPBRShading(fragment.hitPoint, fragment.color.rgb, normal, /*bump*/fragment.viewSpaceNormal*(bump.w+1.0)*0.001, /*roughness*/0.6, /*metallic*/0.1);
	vec3 ambient = fragment.color.rgb * 0.001;
	
	ray.color = color + ambient;
	ray.distance = gl_HitTEXT;
	
	
	// vec3 color = ApplyPBRShading(fragment.hitPoint, fragment.color.rgb, fragment.viewSpaceNormal, vec3(0), /*roughness*/0.6, /*metallic*/0.1);
	// ray.color = color;
	// ray.distance = gl_HitTEXT;
	
}
