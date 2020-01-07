#version 460 core
#extension GL_ARB_enhanced_layouts : enable

precision highp int;
precision highp float;
precision highp sampler2D;

// struct PlanetInfo {
// 	double radius;
// };

// layout(set = 1, binding = 0) uniform Planets {
// 	PlanetInfo planets[255];
// };

layout(std430, push_constant) uniform Planet {
	dvec3 absolutePosition;
	float radius;
} planet;

#include "incubator_rendering/assets/shaders/_v4d_baseDescriptorSet.glsl"

##################################################################
#shader vert

// #include "incubator_rendering/assets/shaders/_cube.glsl"

layout(location = 0) in double posX;
layout(location = 1) in double posY;
layout(location = 2) in double posZ;
layout(location = 3) in vec4 pos;

layout(location = 0) out V2F {
	flat dvec3 posOnPlanet;
	smooth vec3 posOnTriangle;
};

void main() {
	gl_Position = cameraUBO.projection * vec4(cameraUBO.origin * dmat4(cameraUBO.relativeView) * (dvec4(planet.absolutePosition, 1) + dvec4(posX+pos.x, posY+pos.y, posZ+pos.z, 0)));
	posOnPlanet = dvec3(posX, posY, posZ);
	posOnTriangle = pos.xyz;
}

##################################################################

#shader geom

layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

void main() {
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();
	gl_Position = gl_in[1].gl_Position;
	EmitVertex();
	EndPrimitive();
	
	gl_Position = gl_in[1].gl_Position;
	EmitVertex();
	gl_Position = gl_in[2].gl_Position;
	EmitVertex();
	EndPrimitive();
	
	gl_Position = gl_in[2].gl_Position;
	EmitVertex();
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();
	EndPrimitive();
}


##################################################################

#shader frag

layout(location = 0) in V2F {
	flat dvec3 posOnPlanet;
	smooth vec3 posOnTriangle;
};

layout(location = 0) out vec4 out_color;

void main() {
	out_color = vec4(1);
}

