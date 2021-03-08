#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


#############################################################
#shader visibility.rchit

// #define DISABLE_SHADOWS
// #include "v4d/modules/V4D_raytracing/glsl_includes/core_pbr.glsl"

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
	
	ray.normal.xyz = DoubleSidedNormals(normalize(
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
	
	if (HasVertexColor()) {
		ray.color = 
			+ GetVertexColor(i0) * barycentricCoords.x
			+ GetVertexColor(i1) * barycentricCoords.y
			+ GetVertexColor(i2) * barycentricCoords.z
		;
	} else {
		ray.color = vec4(1);
	}
}


#############################################################
#shader spectral.rchit

void main(){}

