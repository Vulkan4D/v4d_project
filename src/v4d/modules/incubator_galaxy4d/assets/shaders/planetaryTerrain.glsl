#version 460 core
#extension GL_ARB_enhanced_layouts : enable

precision highp int;
precision highp float;
precision highp sampler2D;

layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 projection;
} cameraUBO;

layout(std430, push_constant) uniform PlanetChunk {
	mat4 modelView;
	vec4 testColor;
} planetChunk;

struct V2F {
	vec3 relPos;
	vec3 normal;
	vec2 uv;
};

##################################################################
#shader vert

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec2 uv;

layout(location = 0) out V2F v2f;

void main() {
	// vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	// vec4 pos = vec4(uv * 2.0f + -1.0f, 0.0f, 1.0f);
	// vec4 normal = vec4(0);
	
	gl_Position = cameraUBO.projection * planetChunk.modelView * vec4(pos.xyz, 1);
	v2f.relPos = (planetChunk.modelView * vec4(pos.xyz, 1)).xyz;
	v2f.normal = normal.xyz;
	v2f.uv = uv.st;
}

##################################################################

#shader wireframe.geom

layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

layout(location = 0) in V2F in_v2f[];
layout(location = 0) out V2F out_v2f;

void main() {
	gl_Position = gl_in[0].gl_Position;
	out_v2f = in_v2f[0];
	EmitVertex();
	gl_Position = gl_in[1].gl_Position;
	out_v2f = in_v2f[1];
	EmitVertex();
	
	EndPrimitive();
	
	gl_Position = gl_in[1].gl_Position;
	out_v2f = in_v2f[1];
	EmitVertex();
	gl_Position = gl_in[2].gl_Position;
	out_v2f = in_v2f[2];
	EmitVertex();
	
	EndPrimitive();
	
	gl_Position = gl_in[2].gl_Position;
	out_v2f = in_v2f[2];
	EmitVertex();
	gl_Position = gl_in[0].gl_Position;
	out_v2f = in_v2f[0];
	EmitVertex();
	
	EndPrimitive();
}

##################################################################

#shader surface.frag

layout(location = 0) in V2F v2f;

// G-Buffers
layout(location = 0) out highp vec4 gBuffer_albedo;
layout(location = 1) out lowp  vec3 gBuffer_normal;
layout(location = 2) out lowp float gBuffer_roughness;
layout(location = 3) out lowp float gBuffer_metallic;
layout(location = 4) out lowp float gBuffer_scatter;
layout(location = 5) out lowp float gBuffer_occlusion;
layout(location = 6) out highp vec3 gBuffer_emission;
layout(location = 7) out highp vec3 gBuffer_position;

void main() {
	gBuffer_albedo = planetChunk.testColor;
	gBuffer_normal = v2f.normal;
	gBuffer_roughness = 0;
	gBuffer_metallic = 0;
	gBuffer_scatter = 0;
	gBuffer_occlusion = 0;
	gBuffer_emission = vec3(0);
	gBuffer_position = v2f.relPos;
}

