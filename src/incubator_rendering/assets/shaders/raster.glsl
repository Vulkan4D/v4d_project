#include "core.glsl"
#include "Camera.glsl"
layout(set = 0, binding = 1) readonly buffer ObjectBuffer {mat4 objectInstances[];};
layout(set = 0, binding = 2) readonly buffer LightBuffer {float lightSources[];};
layout(set = 0, binding = 3) readonly buffer ActiveLights {
	uint activeLights;
	uint lightIndices[];
};
// layout(set = 1, binding = 0) readonly buffer GeometryBuffer {uvec4 geometries[];};
// layout(set = 1, binding = 1) readonly buffer IndexBuffer {uint indices[];};
// layout(set = 1, binding = 2) readonly buffer VertexBuffer {vec4 vertices[];};

layout(std430, push_constant) uniform GeometryPushConstant{
	int objectIndex;
};

struct ObjectInstance {
	mat4 modelViewMatrix;
	mat3 normalMatrix;
	vec3 position;
	vec3 custom3;
	vec4 custom4;
};

ObjectInstance GetObjectInstance(uint index) {
	ObjectInstance obj;
	obj.modelViewMatrix = objectInstances[index*2];
	mat4 secondMatrix = objectInstances[index*2+1];
	obj.normalMatrix = mat3(
		secondMatrix[0].xyz,
		vec3(secondMatrix[0].w, secondMatrix[1].x, secondMatrix[1].y),
		vec3(secondMatrix[1].z, secondMatrix[1].w, secondMatrix[2].x)
	);
	obj.position = obj.modelViewMatrix[3].xyz;
	obj.custom3 = secondMatrix[2].yzw;
	obj.custom4 = objectInstances[index*2+1][3];
	return obj;
}

struct Vertex {
	vec3 pos;
	vec4 color;
	vec3 normal;
	vec2 uv;
};

// Vertex GetVertex(uint index) {
// 	Vertex v;
// 	v.pos = vertices[index*2].xyz;
// 	v.color = UnpackColorFromFloat(vertices[index*2].w);
// 	v.normal = vertices[index*2+1].xyz;
// 	v.uv = UnpackUVfromFloat(vertices[index*2+1].w);
// 	return v;
// }

#shader visibility.vert

// layout(location = 0) in vec4 in_pos_color; // a = color
// layout(location = 1) in vec4 in_normal_uv; // a = uv
// Vertex GetVertex() {
// 	Vertex v;
// 	v.pos = in_pos_color.xyz;
// 	v.color = UnpackColorFromFloat(in_pos_color.w);
// 	v.normal = in_normal_uv.xyz;
// 	v.uv = UnpackUVfromFloat(in_normal_uv.w);
// 	return v;
// }

layout(location = 0) in vec3 in_pos;
layout(location = 1) in uint _in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in uint _in_uv;
Vertex GetVertex() {
	Vertex v;
	v.pos = in_pos.xyz;
	v.color = UnpackColorFromUint(_in_color);
	v.normal = in_normal.xyz;
	v.uv = UnpackUVfromUint(_in_uv);
	return v;
}

layout(location = 0) out vec3 out_pos;
layout(location = 1) out vec4 out_color;
layout(location = 2) out vec3 out_normal;
layout(location = 3) out vec2 out_uv;

void main() {
	
	// uint indexOffset = geometries[geometryIndex].x;
	// uint vertexOffset = geometries[geometryIndex].y;
	// uint objectIndex = geometries[geometryIndex].z;
	// uint material = geometries[geometryIndex].w;
	
	ObjectInstance obj = GetObjectInstance(objectIndex);
	// Vertex vert = GetVertex(gl_VertexIndex + vertexOffset);
	Vertex vert = GetVertex();
	gl_Position = mat4(camera.projectionMatrix) * obj.modelViewMatrix * vec4(vert.pos, 1);
	
	out_pos = vert.pos;
	out_color = vert.color;
	out_normal = vert.normal;
	out_uv = vert.uv;
}

#shader visibility.frag

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec2 in_uv;

layout(location = 0) out vec2 gBuffer_albedo_geomIndex; // r = albedo, g = geometry index
layout(location = 1) out vec4 gBuffer_normal_uv; // rgb = normal xyz,  a = uv
layout(location = 2) out vec4 gBuffer_position_dist; // rgb = position xyz,  a = trueDistanceFromCamera

void main() {
	gBuffer_position_dist = vec4(1);
}
