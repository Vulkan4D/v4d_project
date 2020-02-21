#include "core.glsl"
#include "Camera.glsl"

vec3 ViewSpaceNormal(vec3 normal) {
	return normalize(transpose(inverse(mat3(camera.viewMatrix))) * normal);
}

##################################################################
#shader vert

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec3 v_normal;

layout(location = 0) out vec3 f_worldPos;
layout(location = 1) out vec3 f_normal;
layout(location = 2) out flat uint f_instanceIndex;

vec3 positions[4] = {
	vec3(-6,3,3),
	vec3(-3,3,3),
	vec3(0,3,3),
	vec3(3,3,3)
};

void main() {
	f_worldPos = v_pos + positions[gl_InstanceIndex];
	f_normal = v_normal;
	f_instanceIndex = gl_InstanceIndex;
	gl_Position = mat4(camera.projectionMatrix * camera.viewMatrix) * vec4(f_worldPos, 1);
}

##################################################################
#shader surface.frag

#include "gBuffers_out.glsl"

layout(location = 0) in vec3 f_worldPos;
layout(location = 1) in vec3 f_normal;
layout(location = 2) in flat uint f_instanceIndex;

vec3 f_viewPos = (mat4(camera.viewMatrix) * vec4(f_worldPos, 1)).xyz;
float f_trueDistance = float(distance(camera.worldPosition, dvec3(f_worldPos)));

float roughness[4] = {
	0.05,
	0.05,
	0.2,
	0.05
};

float metallic[4] = {
	0.0,
	0.333,
	0.666,
	1.0
};

void main() {
	vec3 albedo = vec3(1,0,0);
	vec3 normal = f_normal;
	vec3 emission = vec3(0);
	float scatter = 0;
	float occlusion = 0;
	
	GBuffers gBuffers;
	gBuffers.albedo = albedo;
	gBuffers.alpha = 1.0;
	gBuffers.normal = ViewSpaceNormal(normalize(normal));
	gBuffers.roughness = roughness[f_instanceIndex];
	gBuffers.metallic = metallic[f_instanceIndex];
	gBuffers.emission = emission;
	gBuffers.position = f_viewPos;
	gBuffers.dist = f_trueDistance;
	WriteGBuffers(gBuffers);
}
