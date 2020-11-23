#include "core.glsl"

struct V2F {
	vec4 color;
	vec4 pos;
	vec3 normal;
};

vec3 TriplanarBlending(vec3 norm) {
	vec3 blending = abs(norm);
	blending = normalize(max(blending, 0.00001));
	float b = blending.x + blending.y + blending.z;
	blending /= vec3(b);
	return blending;
}
vec4 TriplanarTextureRGBA(sampler2D tex, vec3 coords, vec3 blending) {
	vec4 xaxis = texture( tex, coords.yz);
	vec4 yaxis = texture( tex, coords.xz);
	vec4 zaxis = texture( tex, coords.xy);
	return xaxis * blending.x + yaxis * blending.y + zaxis * blending.z;
}
vec3 TriplanarTextureRGB(sampler2D tex, vec3 coords, vec3 blending) {
	vec3 xaxis = texture( tex, coords.yz).rgb;
	vec3 yaxis = texture( tex, coords.xz).rgb;
	vec3 zaxis = texture( tex, coords.xy).rgb;
	return xaxis * blending.x + yaxis * blending.y + zaxis * blending.z;
}
float TriplanarTextureR(sampler2D tex, vec3 coords, vec3 blending) {
	float xaxis = texture( tex, coords.yz).r;
	float yaxis = texture( tex, coords.xz).r;
	float zaxis = texture( tex, coords.xy).r;
	return xaxis * blending.x + yaxis * blending.y + zaxis * blending.z;
}
float TriplanarTextureA(sampler2D tex, vec3 coords, vec3 blending) {
	float xaxis = texture( tex, coords.yz).a;
	float yaxis = texture( tex, coords.xz).a;
	float zaxis = texture( tex, coords.xy).a;
	return xaxis * blending.x + yaxis * blending.y + zaxis * blending.z;
}

#common .*rchit

#include "v4d/modules/V4D_hybrid/glsl_includes/rtx.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_rays.glsl"

#############################################################
#shader rchit

hitAttributeEXT vec3 hitAttribs;

layout(location = 0) rayPayloadInEXT RayPayload_visibility ray;

#include "rtx_fragment.glsl"

void main() {
	Fragment fragment = GetHitFragment(true);
	
	ray.customData = GenerateCustomData(fragment.objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
	ray.viewSpacePosition = fragment.hitPoint;
	ray.emit = 0;
	ray.uv = uintBitsToFloat(fragment.customData);
	ray.distance = gl_HitTEXT;
	
	vec3 blending = TriplanarBlending(fragment.normal);
	ray.albedo = mix(TriplanarTextureRGB(tex_img_metalAlbedo, fragment.pos, blending), fragment.color.rgb, 0.7);
	ray.metallic = TriplanarTextureR(tex_img_metalMetallic, fragment.pos, blending);
	ray.roughness = TriplanarTextureR(tex_img_metalRoughness, fragment.pos, blending);
	
	// Normal map
	vec3 tangentX = normalize(cross(fragment.geometryInstance.normalViewTransform * vec3(0.758,0.899,1.212)/* fixed arbitrary vector in object space */, fragment.viewSpaceNormal));
	vec3 tangentY = normalize(cross(fragment.viewSpaceNormal, tangentX));
	mat3 TBN = mat3(tangentX, tangentY, fragment.viewSpaceNormal); // viewSpace TBN
	ray.viewSpaceNormal = normalize(TBN * TriplanarTextureRGB(tex_img_metalNormal, fragment.pos, blending));

}

###########################################
#shader vert

#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_raster.glsl"

layout(location = 0) out V2F v2f;
layout(location = 3) out flat uint customData;
layout(location = 4) out vec3 triplanarCoords;
layout(location = 5) out vec3 triplanarNormal;

void main() {
	GeometryInstance geometryInstance = GetGeometryInstance(geometryIndex);
	Vertex vert = GetVertex();
	
	v2f.color = vert.color;
	v2f.pos = (geometryInstance.modelViewTransform * vec4(vert.pos, 1));
	v2f.normal = geometryInstance.normalViewTransform * vert.normal;
	customData = vert.customData;
	triplanarCoords = vert.pos;
	triplanarNormal = vert.normal;
	
	// Calculate actual distance from camera
	v2f.pos.w = clamp(distance(vec3(camera.worldPosition), (mat4(geometryInstance.modelTransform) * vec4(vert.pos,1)).xyz), float(camera.znear), float(camera.zfar));
	
	gl_Position = mat4(camera.projectionMatrix) * vec4(v2f.pos.xyz, 1);
}

###########################################
#shader frag

#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_raster.glsl"

layout(location = 0) in V2F v2f;
layout(location = 3) in flat uint customData;
layout(location = 4) in vec3 triplanarCoords;
layout(location = 5) in vec3 triplanarNormal;

void main() {
	pbrGBuffers.viewSpacePosition = v2f.pos.xyz;
	pbrGBuffers.uv = uintBitsToFloat(customData);
	pbrGBuffers.emit = 0;
	
	vec3 blending = TriplanarBlending(triplanarNormal);
	pbrGBuffers.albedo = mix(TriplanarTextureRGB(tex_img_metalAlbedo, triplanarCoords, blending), v2f.color.rgb, 0.7);
	pbrGBuffers.metallic = TriplanarTextureR(tex_img_metalMetallic, triplanarCoords, blending);
	pbrGBuffers.roughness = TriplanarTextureR(tex_img_metalRoughness, triplanarCoords, blending);
	
	// Normal map
	GeometryInstance geometryInstance = GetGeometryInstance(geometryIndex);
	vec3 tangentX = normalize(cross(geometryInstance.normalViewTransform * normalize(vec3(0.758,0.899,1.212))/* fixed arbitrary vector in object space */, v2f.normal));
	vec3 tangentY = normalize(cross(v2f.normal, tangentX));
	mat3 TBN = mat3(tangentX, tangentY, v2f.normal); // viewSpace TBN
	pbrGBuffers.viewSpaceNormal = normalize(TBN * TriplanarTextureRGB(tex_img_metalNormal, triplanarCoords, blending));
	
	pbrGBuffers.distance = v2f.pos.w;
	WritePbrGBuffers();
	WriteCustomBuffer(objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
}

