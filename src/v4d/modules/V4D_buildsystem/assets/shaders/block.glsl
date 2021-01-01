#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


vec3 TriplanarBlending(vec3 norm) {
	vec3 blending = abs(norm);
	blending = normalize(max(blending, 0.00001));
	float b = blending.x + blending.y + blending.z;
	blending /= vec3(b);
	return blending;
}
// vec4 TriplanarTextureRGBA(sampler2D tex, vec3 coords, vec3 blending) {
// 	vec4 xaxis = texture( tex, coords.zy);
// 	vec4 yaxis = texture( tex, coords.xz);
// 	vec4 zaxis = texture( tex, coords.xy);
// 	return xaxis * blending.x + yaxis * blending.y + zaxis * blending.z;
// }
// vec3 TriplanarTextureRGB(sampler2D tex, vec3 coords, vec3 blending) {
// 	vec3 xaxis = texture( tex, coords.zy).rgb;
// 	vec3 yaxis = texture( tex, coords.xz).rgb;
// 	vec3 zaxis = texture( tex, coords.xy).rgb;
// 	return xaxis * blending.x + yaxis * blending.y + zaxis * blending.z;
// }
// float TriplanarTextureR(sampler2D tex, vec3 coords, vec3 blending) {
// 	float xaxis = texture( tex, coords.zy).r;
// 	float yaxis = texture( tex, coords.xz).r;
// 	float zaxis = texture( tex, coords.xy).r;
// 	return xaxis * blending.x + yaxis * blending.y + zaxis * blending.z;
// }
// float TriplanarTextureA(sampler2D tex, vec3 coords, vec3 blending) {
// 	float xaxis = texture( tex, coords.zy).a;
// 	float yaxis = texture( tex, coords.xz).a;
// 	float zaxis = texture( tex, coords.xy).a;
// 	return xaxis * blending.x + yaxis * blending.y + zaxis * blending.z;
// }
vec3 TriplanarLocalNormalMap(sampler2D normalTex, vec3 coords, vec3 localFaceNormal, vec3 blending, float distance) {
	uvec2 texSize = textureSize(normalTex, 0).st;
	float resolutionRatio = min(texSize.s, texSize.t) / min(camera.width, camera.height);
	float lod = pow(distance, 0.5) * resolutionRatio;
	vec3 tnormalX = textureLod(normalTex, coords.zy, lod).xyz * 2 - 1;
	vec3 tnormalY = textureLod(normalTex, coords.xz, lod).xyz * 2 - 1;
	vec3 tnormalZ = textureLod(normalTex, coords.xy, lod).xyz * 2 - 1;
	tnormalX = vec3(0, tnormalX.yx);
	tnormalY = vec3(tnormalY.x, 0, tnormalY.y);
	tnormalZ = vec3(tnormalZ.xy, 0);
	return normalize(
		localFaceNormal
		+ tnormalX * blending.x
		+ tnormalY * blending.y
		+ tnormalZ * blending.z
	);
}


#############################################################
#shader rendering.rchit

layout(buffer_reference, std430, buffer_reference_align = 4) buffer CustomData {
	uint packed[];
};

hitAttributeEXT vec3 hitAttribs;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	uint i0 = GetIndex(0);
	uint i1 = GetIndex(1);
	uint i2 = GetIndex(2);
	
	// Interpolate fragment
	vec3 barycentricCoords = vec3(1.0f - hitAttribs.x - hitAttribs.y, hitAttribs.x, hitAttribs.y);
	vec3 pos = (
		+ GetVertexPosition(i0) * barycentricCoords.x
		+ GetVertexPosition(i1) * barycentricCoords.y
		+ GetVertexPosition(i2) * barycentricCoords.z
	);
	vec3 normal = normalize(
		+ GetVertexNormal(i0) * barycentricCoords.x
		+ GetVertexNormal(i1) * barycentricCoords.y
		+ GetVertexNormal(i2) * barycentricCoords.z
	);
	vec4 color = HasVertexColor()? (
		+ GetVertexColor(i0) * barycentricCoords.x
		+ GetVertexColor(i1) * barycentricCoords.y
		+ GetVertexColor(i2) * barycentricCoords.z
	) : vec4(0);
	// vec2 uv = HasVertexUV()? (
	// 	+ GetVertexUV(i0) * barycentricCoords.x
	// 	+ GetVertexUV(i1) * barycentricCoords.y
	// 	+ GetVertexUV(i2) * barycentricCoords.z
	// ) : vec2(0);
	
	
	vec3 blending = TriplanarBlending(normal);
	normal = TriplanarLocalNormalMap(tex_img_metalNormal, pos, normal, blending, gl_HitTEXT);
	normal += TriplanarLocalNormalMap(tex_img_metalNormal, pos/200, normal, blending, gl_HitTEXT/100)/2;
	normal = normalize(normal);
	
	WriteRayPayload(ray);
	ray.albedo = color.rgb;
	ray.normal = DoubleSidedNormals(normalize(GetModelNormalViewMatrix() * normal));
	ray.metallic = 0.7;
	ray.roughness = 0.1;
	ray.raycastCustomData = uint64_t(CustomData(GetCustomData()).packed[i0]);
}
