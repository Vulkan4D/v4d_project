#include "core.glsl"
#include "Camera.glsl"

#common visibility.*

layout(std430, push_constant) uniform GeometryPushConstant{
	uint objectIndex;
	uint geometryIndex;
};

struct V2F {
	vec4 pos;
	vec4 color;
	vec4 normal_dist;
	vec2 uv;
};

#shader visibility.vert

#define GEOMETRY_BUFFERS_ACCESS readonly
#define VISIBILITY_VERTEX_SHADER
#include "core_buffers.glsl"

layout(location = 0) out V2F v2f;

void main() {
	GeometryInstance geometry = GetGeometryInstance(geometryIndex);
	Vertex vert = GetVertex();
	vec4 pos = vec4(vert.pos, 1);
	
	v2f.pos = geometry.modelViewTransform * pos;
	v2f.color = vert.color;
	v2f.normal_dist = vec4(
		geometry.normalViewTransform * vert.normal,
		// True Distance From Camera
		clamp(distance(vec3(camera.worldPosition), (mat4(geometry.modelTransform) * pos).xyz), float(camera.znear), float(camera.zfar))
		// length(geometry.viewPosition)
	);
	v2f.uv = vert.uv;
	
	gl_Position = mat4(camera.projectionMatrix) * v2f.pos;
}

#shader visibility.frag

layout(location = 0) in V2F v2f;

layout(location = 0) out vec2 gBuffer_albedo_geometryIndex; // r = albedo, g = geometry index
layout(location = 1) out vec4 gBuffer_normal_uv; // rgb = normal xyz,  a = uv
layout(location = 2) out vec4 gBuffer_position_dist; // rgb = position xyz,  a = trueDistanceFromCamera

void main() {
	gBuffer_albedo_geometryIndex = vec2(PackColorAsFloat(v2f.color), uintBitsToFloat(geometryIndex));
	gBuffer_normal_uv = vec4(v2f.normal_dist.xyz, PackUVasFloat(v2f.uv));
	gBuffer_position_dist = vec4(v2f.pos.xyz, v2f.normal_dist.w);
}

// #shader lighting.comp

#shader lighting.vert

void main() {
	vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(uv * 2.0f + -1.0f, 0.0f, 1.0f);
}

#shader lighting.frag

#define GEOMETRY_BUFFERS_ACCESS readonly
#include "core_buffers.glsl"
#include "core_pbr.glsl"

vec3 ApplyPBRShading(vec3 hitPoint, vec3 albedo, vec3 normal, vec3 bump, float roughness, float metallic) {
	vec3 color = vec3(0);
	
	// PBR lighting
	vec3 N = normal;
	vec3 V = normalize(-hitPoint);
	// calculate reflectance at normal incidence; if dia-electric (like plastic) use baseReflectivity of 0.4 and if it's metal, use the albedo color as baseReflectivity
	vec3 baseReflectivity = mix(vec3(0.04), albedo, max(0,metallic));
	
	for (int activeLightIndex = 0; activeLightIndex < activeLights; activeLightIndex++) {
		LightSource light = GetLight(lightIndices[activeLightIndex]);
		
		// Calculate light radiance at distance
		float dist = length(light.position - hitPoint);
		vec3 radiance = light.color * light.intensity * (1.0 / (dist*dist));
		vec3 L = normalize(light.position - hitPoint);
		
		if (length(radiance) > radianceThreshold) {
			// cook-torrance BRDF
			vec3 H = normalize(V + L);
			float NdotV = max(dot(N,V), 0.000001);
			float NdotL = max(dot(N,L), 0.000001);
			float HdotV = max(dot(H,V), 0.0);
			float NdotH = max(dot(N,H), 0.0);
			//
			float D = distributionGGX(NdotH, roughness); // larger the more micro-facets aligned to H (normal distribution function)
			float G = geometrySmith(NdotV, NdotL, roughness); // smaller the more micro-facets shadowed by other micro-facets
			vec3 F = fresnelSchlick(HdotV, baseReflectivity); // proportion of specular reflectance
			vec3 FV = fresnelSchlick(NdotV, baseReflectivity); // proportion of specular reflectance
			vec3 FL = fresnelSchlick(NdotL, baseReflectivity); // proportion of specular reflectance
			//
			vec3 specular = D * G * F;
			specular /= 4.0 * NdotV * NdotL;
			// for energy conservation, the diffuse and specular light can't be above 1.0 (unless the surface emits light); to preserve this relationship the diffuse component (kD) should equal to 1.0 - kS.
			// vec3 kD = vec3(1.0)-F;
			vec3 kD = (vec3(1.0)-FV) * (vec3(1.0)-FL) * 1.05;
			// multiply kD by the inverse metalness such that only non-metals have diffuse lighting, or a linear blend if partly metal (pure metals have no diffuse light).
			kD *= 1.0 - max(0, metallic);
			// Final lit color
			color += (kD * albedo / PI + specular) * radiance * max(dot(N,L) + max(0, metallic/-5), 0);
			
			// Sub-Surface Scattering (simple rim for now)
			// if (metallic < 0) {
				float rim = pow(1.0 - NdotV, 2-metallic*2+NdotL) * NdotL;
				color += -min(0,metallic) * radiance * rim;
			// }
		}
	}
	
	return color;
}

#include "core_lightingPass.glsl"

void main() {
	vec2 albedo_geometryIndex = subpassLoad(gBuffer_albedo_geometryIndex).rg;
	vec4 normal_uv = subpassLoad(gBuffer_normal_uv);
	vec4 position_dist = subpassLoad(gBuffer_position_dist);
	vec4 albedo = UnpackColorFromFloat(albedo_geometryIndex.r);
	uint geometryIndex = floatBitsToUint(albedo_geometryIndex.g);
	vec3 normal = normal_uv.xyz;
	vec2 uv = UnpackUVfromFloat(normal_uv.w);
	
	if (camera.debug) {
		out_color = vec4(normal, 1);
		return;
	}
	
	out_color = vec4(ApplyPBRShading(position_dist.xyz, albedo.rgb, normal, vec3(0), 0.6, 0.1), 1);
}
