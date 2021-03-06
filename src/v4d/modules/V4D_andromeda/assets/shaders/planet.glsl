// #common .*comp

// #include "noise.glsl"

// #common .*map.comp

// layout(set = 1, binding = 0, rgba32f) uniform image2D bumpMap[1];

// layout(std430, push_constant) uniform Planet{
// 	int planetIndex;
// 	float planetHeightVariation;
// };

// vec3 GetCubeDirection(writeonly imageCube image) {
// 	const vec2 pixelCenter = vec2(gl_GlobalInvocationID.xy) + vec2(0.5);
// 	const vec2 uv = pixelCenter / imageSize(image).xy;
// 	vec2 d = uv * 2.0 - 1.0;
	
// 	vec3 direction = vec3(0);
// 	switch (int(gl_GlobalInvocationID.z)) {
// 		case 0 : // Right (POSITIVE_X)
// 			direction = vec3( 1, -d.y, -d.x);
// 		break;
// 		case 1 : // Left (NEGATIVE_X)
// 			direction = vec3(-1, -d.y, d.x);
// 		break;
// 		case 2 : // Front (POSITIVE_Y)
// 			direction = vec3(d.x, 1, d.y);
// 		break;
// 		case 3 : // Back (NEGATIVE_Y)
// 			direction = vec3(d.x,-1, -d.y);
// 		break;
// 		case 4 : // Top (POSITIVE_Z)
// 			direction = vec3(d.x,-d.y, 1);
// 		break;
// 		case 5 : // Bottom (NEGATIVE_Z)
// 			direction = vec3(-d.x,-d.y, -1);
// 		break;
// 	}
	
// 	return normalize(direction);
// }

// #common terrain.*comp|terrain.visibility.rchit

// layout(set = 2, binding = 0) uniform sampler2D bumpMap[1];

// #############################################################
// #shader bump.altitude.map.comp
// void main() {
// 	vec2 size = vec2(imageSize(bumpMap[0]).xy);
// 	float mult = 40;
// 	float noiseOffset = 2331.43;
// 	vec2 coords1 = abs((vec2(gl_GlobalInvocationID.xy) / size - 0.5) * 2.0); // -1 to +1
// 	vec2 coords2 = abs((vec2(gl_GlobalInvocationID.yx) / size - 0.5) * 2.0);
// 	float altitude1 = FastSimplexFractal(vec3(coords1 * mult, noiseOffset), 15);
// 	float altitude2 = FastSimplexFractal(vec3(coords2 * mult, noiseOffset), 15);
// 	float altitude = mix(altitude1, altitude2, min(1.0,coords1.x+coords1.y)/2.0);
// 	imageStore(bumpMap[0], ivec2(gl_GlobalInvocationID.xy), vec4(0,0,0, altitude));
// }

// #############################################################
// #shader bump.normals.map.comp
// void main() {
// 	ivec2 size = ivec2(imageSize(bumpMap[0]).xy);
// 	ivec2 center = ivec2(gl_GlobalInvocationID.xy);
// 	ivec2 top = ivec2(gl_GlobalInvocationID.xy) + ivec2(0, -1);
// 	ivec2 bottom = ivec2(gl_GlobalInvocationID.xy) + ivec2(0, +1);
// 	ivec2 right = ivec2(gl_GlobalInvocationID.xy) + ivec2(+1, 0);
// 	ivec2 left = ivec2(gl_GlobalInvocationID.xy) + ivec2(-1, 0);
// 	if (bottom.y >= size.y) bottom.y = 0;
// 	if (right.x >= size.x) right.x = 0;
// 	if (top.y < 0) top.y = size.y-1;
// 	if (left.x < 0) left.x = size.x-1;
// 	float altitude = imageLoad(bumpMap[0], ivec2(gl_GlobalInvocationID.xy)).a;
// 	float altitudeTop = imageLoad(bumpMap[0], top).a / 2.0 + 0.5;
// 	float altitudeBottom = imageLoad(bumpMap[0], bottom).a / 2.0 + 0.5;
// 	float altitudeRight = imageLoad(bumpMap[0], right).a / 2.0 + 0.5;
// 	float altitudeLeft = imageLoad(bumpMap[0], left).a / 2.0 + 0.5;
// 	vec3 normal = normalize(vec3(4*(altitudeRight-altitudeLeft), 4*(altitudeBottom-altitudeTop), 1));
// 	imageStore(bumpMap[0], ivec2(gl_GlobalInvocationID.xy), vec4(normal, altitude));
// }

#############################################################
#shader terrain.visibility.rchit

#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

// layout(buffer_reference, std430, buffer_reference_align = 16) buffer CustomData {
// 	vec2 uvMult;
// 	vec2 uvOffset;
// };

// vec4 GetBumpMap(vec2 uv) {
// 	return vec4(0,0,1,0) + texture(bumpMap[0], uv) / 2.0;
// }

hitAttributeEXT vec3 hitAttribs;

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

void main() {
	WriteRayPayload(ray);
	
	uint i0 = GetIndex(0);
	uint i1 = GetIndex(1);
	uint i2 = GetIndex(2);
	
	vec3 barycentricCoords = vec3(1.0f - hitAttribs.x - hitAttribs.y, hitAttribs.x, hitAttribs.y);
	
	// vec3 pos = (
	// 	+ GetVertexPosition(i0) * barycentricCoords.x
	// 	+ GetVertexPosition(i1) * barycentricCoords.y
	// 	+ GetVertexPosition(i2) * barycentricCoords.z
	// );
	
	ray.normal.xyz = (normalize(
		+ GetVertexNormal(i0) * barycentricCoords.x
		+ GetVertexNormal(i1) * barycentricCoords.y
		+ GetVertexNormal(i2) * barycentricCoords.z
	));
	
	if (HasVertexUV()) {
		ray.uv = 
			+ GetVertexUV(i0) * barycentricCoords.x
			+ GetVertexUV(i1) * barycentricCoords.y
			+ GetVertexUV(i2) * barycentricCoords.z
		;
	}
	
	// BaseColor
	if (HasVertexColor()) {
		ray.color = 
			+ GetVertexColor(i0) * barycentricCoords.x
			+ GetVertexColor(i1) * barycentricCoords.y
			+ GetVertexColor(i2) * barycentricCoords.z
		;
	} else {
		ray.color = vec4(1);
	}
	
	// uint i0 = GetIndex(0);
	// uint i1 = GetIndex(1);
	// uint i2 = GetIndex(2);
	
	// // Interpolate fragment
	// vec3 barycentricCoords = vec3(1.0f - hitAttribs.x - hitAttribs.y, hitAttribs.x, hitAttribs.y);
	// // vec3 pos = (
	// // 	+ GetVertexPosition(i0) * barycentricCoords.x
	// // 	+ GetVertexPosition(i1) * barycentricCoords.y
	// // 	+ GetVertexPosition(i2) * barycentricCoords.z
	// // );
	// vec3 normal = normalize(
	// 	+ GetVertexNormal(i0) * barycentricCoords.x
	// 	+ GetVertexNormal(i1) * barycentricCoords.y
	// 	+ GetVertexNormal(i2) * barycentricCoords.z
	// );
	// vec4 color = HasVertexColor()? (
	// 	+ GetVertexColor(i0) * barycentricCoords.x
	// 	+ GetVertexColor(i1) * barycentricCoords.y
	// 	+ GetVertexColor(i2) * barycentricCoords.z
	// ) : vec4(0);
	// vec2 uv = HasVertexUV()? (
	// 	+ GetVertexUV(i0) * barycentricCoords.x
	// 	+ GetVertexUV(i1) * barycentricCoords.y
	// 	+ GetVertexUV(i2) * barycentricCoords.z
	// ) : vec2(0);
	
	// vec3 viewSpaceNormal = normalize(GetModelNormalViewMatrix() * normal);
	// vec3 tangentX = normalize(cross(GetModelNormalViewMatrix() * vec3(0,1,0)/* fixed arbitrary vector in object space */, viewSpaceNormal));
	// vec3 tangentY = normalize(cross(viewSpaceNormal, tangentX));
	// mat3 TBN = mat3(tangentX, tangentY, viewSpaceNormal); // viewSpace TBN
	// vec4 bump = GetBumpMap(uv);
	// normal = normalize(TBN * bump.xyz);
	// float roughness = 0.7;
	
	// WriteRayPayload(ray);
	// ray.color = color;
	// ScatterLambertian(ray, roughness, normal);
	
	
	// DebugRay(ray, color.rgb, normal, 0, 0, 0);
}


