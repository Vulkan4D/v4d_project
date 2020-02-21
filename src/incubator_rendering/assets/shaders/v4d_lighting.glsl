#version 460 core

precision highp int;
precision highp float;
precision highp sampler2D;

#common .*frag

#include "Camera.glsl"
#include "LightSource.glsl"
#include "gBuffers_in.glsl"

const float PI = 3.1415926543;

// PBR formulas
float distributionGGX(float NdotH, float roughness) {
	float a = roughness*roughness;
	float a2 = a*a;
	float denom = NdotH*NdotH * (a2 - 1.0) + 1.0;
	denom = PI * denom*denom;
	return a2 / max(denom, 0.0000001);
}
float geometrySmith(float NdotV, float NdotL, float roughness) {
	float r = roughness + 1.0;
	float k = (r*r) / 8.0;
	float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
	float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
	return ggx1 * ggx2;
}
vec3 fresnelSchlick(float HdotV, vec3 baseReflectivity) {
	// baseReflectivity in range 0 to 1
	// returns range baseReflectivity to 1
	// increases as HdotV decreases (more reflectivity when surface viewed at larger angles)
	return baseReflectivity + (1.0 - baseReflectivity) * pow(1.0 - HdotV, 5.0);
}

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

##################################################################
#shader vert

layout (location = 0) out vec2 outUV;

void main() 
{
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0f + -1.0f, 0.0f, 1.0f);
}

##################################################################

#shader opaque.frag
void main() {
	GBuffers gBuffers = LoadGBuffers();
	
	
	// PBR
	vec3 N = normalize(gBuffers.normal);
	vec3 V = normalize(-gBuffers.position);
	// calculate reflectance at normla incidence; if dia-electric (like plastic) use baseReflectivity of 0.4 and if it's metal, use the albedo color as baseReflectivity
	vec3 baseReflectivity = mix(vec3(0.04), gBuffers.albedo, gBuffers.metallic);
	// reflectance equiation
	// for each light {
		// calculate per-light radiance
		vec3 L = normalize(lightSource.viewPosition - gBuffers.position);
		vec3 H = normalize(V + L);
		float dist = length(lightSource.viewPosition - gBuffers.position);
		float atten = 8.0;//1.0 / (dist*dist);
		vec3 radiance = lightSource.color * atten;
		// cook-torrance BRDF
		float NdotV = max(dot(N,V), 0.0000001);
		float NdotL = max(dot(N,L), 0.0000001);
		float HdotV = max(dot(H,V), 0.0);
		float NdotH = max(dot(N,H), 0.0);
		//
		float D = distributionGGX(NdotH, gBuffers.roughness); // larger the more micro-facets aligned to H (normal distribution function)
		float G = geometrySmith(NdotV, NdotL, gBuffers.roughness); // smaller the more micro-facets shadowed by other micro-facets
		vec3 F = fresnelSchlick(HdotV, baseReflectivity); // proportion of specular reflectance
		//
		vec3 specular = D * G * F;
		specular /= 4.0 * NdotV * NdotL;
		// for energy conservation, the diffuse and specular light can't be above 1.0 (unless the surface emits light); to preserve this relationship the diffuse component (kD) should equal to 1.0 - kS.
		vec3 kD = vec3(1.0) - F; // F equals kS
		// multiply kD by the inverse metalness such that only non-metals have diffuse lighting, or a linear blend if partly metal (pure metals have no diffuse light).
		kD *= 1.0 - gBuffers.metallic;
		// Final lit color
		vec3 color = (kD * gBuffers.albedo / PI + specular) * radiance * NdotL;
	// }
	
	// Ambient
	// color += gBuffers.albedo * 0.01;
	
	// Emission
	color += gBuffers.emission;
	
	
	
	
// 	/////////////////////////////
// 	// Blinn-Phong

// vec3 color = gBuffers.albedo;

// 	// ambient
// 	vec3 ambient = vec3(0);// lightSource.color * lightSource.ambientStrength;

// 	// diffuse
// 	vec3 norm = normalize(gBuffers.normal);
// 	vec3 lightDir = normalize(lightSource.viewPosition - gBuffers.position);
// 	float diff = max(dot(norm, lightDir), 0.0);
// 	vec3 diffuse = diff * lightSource.color * (lightSource.intensity/20.0);

// 	// specular
// 	float specularStrength = 0.5;
// 	vec3 viewDir = normalize(-gBuffers.position);
// 	vec3 reflectDir = reflect(-lightDir, norm);
// 	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
// 	vec3 specular = specularStrength * spec * lightSource.color * (lightSource.intensity/30.0);

// // diffuse = vec3(0.5);
// // specular = vec3(0);

// 	// // Attenuation (light.constant >= 1.0, light.linear ~= 0.1, light.quadratic ~= 0.05)
// 	// float dist = distance(lightSource.viewPosition, gBuffers.position);
// 	// float attenuation = 1.0 / (1.0/*light.constant*/ + 0.1/*light.linear*/*dist + 0.05/*light.quadratic*/*(dist*dist));
// 	// ambient *= attenuation;
// 	// diffuse *= attenuation;
// 	// specular *= attenuation;
	
	
// 	// //////
// 	// // For Spot lights : 
// 	// float innerCutOff = cos(radians(12/*degrees*/));
// 	// float outerCutOff = cos(radians(18/*degrees*/));
// 	// float theta = dot(lightDir, normalize(-spotLight.direction));
// 	// float epsilon = (innerCutOff - outerCutOff);
// 	// float intensity = clamp((theta - outerCutOff) / epsilon, 0.0, 1.0);
// 	// diffuse *= intensity;
// 	// specular *= intensity;
	
	
	
// 	// Final color
// 	color *= (ambient + diffuse + specular);

// 	/////////////////////////////
	
	
	
	
	
	
	
	
	// color = gBuffers.albedo * vec3(dot(gBuffers.normal, normalize(lightSource.viewPosition - gBuffers.position)));
	
	
	
	
	
	
	
	
	out_color = vec4(color, gBuffers.alpha);
}

#shader transparent.frag
void main() {
	GBuffers gBuffers = LoadGBuffers();
	vec3 color = gBuffers.albedo;
	float alpha = gBuffers.alpha;
	
	if (alpha < 0.01) discard;
	
	out_color = vec4(color, alpha);
}
