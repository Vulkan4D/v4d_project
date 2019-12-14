#version 460 core
#extension GL_ARB_shader_viewport_layer_array : enable

precision highp int;
precision highp float;
precision highp sampler2D;

#common gen.*

layout(std430, push_constant) uniform GalaxyGenPushConstant {
	dvec3 cameraPosition;
	int frameIndex;
} galaxyGen;

##################################################################
#shader gen.vert

layout(location = 0) in vec4 posr;
layout(location = 1) in uint seed;
layout(location = 2) in uint numStars;

layout(location = 0) out uint out_seed;
layout(location = 1) out uint out_numStars;

void main() {
	gl_Position = posr; // passthrough
	out_seed = seed + galaxyGen.frameIndex;
	out_numStars = numStars;
}

##################################################################
#shader gen.geom

#include "_noise.glsl"

/* 
max_vertices <= min(limits.maxGeometryOutputVertices, floor(limits.maxGeometryTotalOutputComponents / min(limits.maxGeometryOutputComponents, 7 + NUM_OUT_COMPONENTS)))
Typical values (GTX 1050ti, GTX 1080, RTX 2080): 
	limits.maxGeometryOutputComponents 		= 128
	limits.maxGeometryOutputVertices 		= 1024
	limits.maxGeometryTotalOutputComponents = 1024
=======
In practice: 
	max_vertices <= floor(1024 / 7 + NUM_OUT_COMPONENTS)
*/

layout(points) in;
layout(points, max_vertices = 80) out; // takes up 7 components per vertex (1 for gl_PointSize, 4 for gl_Position, 2 for gl_PointCoord)
layout(location = 0) out vec4 out_color; // takes up 4 components

layout(location = 0) in uint in_seed[];
layout(location = 1) in uint in_numStars[];

const float MIN_VIEW_DISTANCE = 0.01;
const float MAX_VIEW_DISTANCE = 10000;

void main(void) {
	uint seed = in_seed[0];
	uint fseed = seed + 15;
	
	// position relative to camera
	vec3 relPos = vec3(dvec3(gl_in[0].gl_Position.xyz) - galaxyGen.cameraPosition.xyz);
	
	float radius = gl_in[0].gl_Position.w;
	
	for (int i = 0; i < in_numStars[0]; i++) {
		if (i > 80) break;
		vec3 pos = relPos + RandomInUnitSphere(seed)*4;
		
		float brightnessBasedOnDistance = pow(smoothstep(MAX_VIEW_DISTANCE, MIN_VIEW_DISTANCE, length(pos)), 2);
		
		gl_PointSize = radius + RandomFloat(fseed) + brightnessBasedOnDistance*2;
		
		vec4 starType = normalize(vec4(
			/*red*/		RandomFloat(fseed) * 1.1 ,
			/*yellow*/	RandomFloat(fseed) * 1.2 ,
			/*blue*/	RandomFloat(fseed) * 1.1 ,
			/*white*/	RandomFloat(fseed) * 1.8 
		));
		
		vec3 color =/*red*/		vec3( 1.0 , 0.5 , 0.3 ) * starType.x +
					/*yellow*/	vec3( 1.0 , 1.0 , 0.2 ) * starType.y +
					/*blue*/	vec3( 0.2 , 0.5 , 1.0 ) * starType.z +
					/*white*/	vec3( 1.0 , 1.0 , 1.0 ) * starType.w ;
		
		out_color = vec4(
			color,
			(1-pow(1-RandomFloat(fseed), 2)) * brightnessBasedOnDistance
		);
		
		// Compute which side of the cube map we should render to
		if (pos.x > 0 && abs(pos.z) <= pos.x && abs(pos.y) <= pos.x) {
			// Right
			gl_Layer = 0;
			gl_Position = vec4(
				-pos.z / pos.x,
				-pos.y / pos.x,
				0, 1
			);
		} else 
		if (-pos.x > 0 && abs(pos.z) <= -pos.x && abs(pos.y) <= -pos.x) {
			// Left
			gl_Layer = 1;
			gl_Position = vec4(
				-pos.z / pos.x,
				 pos.y / pos.x,
				0, 1
			);
		} else
		if (pos.y > 0 && abs(pos.z) <= pos.y && abs(pos.x) <= pos.y) {
			// Front
			gl_Layer = 2;
			gl_Position = vec4(
				pos.x / pos.y,
				pos.z / pos.y,
				0, 1
			);
		} else 
		if (-pos.y > 0 && abs(pos.z) <= -pos.y && abs(pos.x) <= -pos.y) {
			// Back
			gl_Layer = 3;
			gl_Position = vec4(
				-pos.x / pos.y,
				 pos.z / pos.y,
				0, 1
			);
		} else 
		if (pos.z > 0 && abs(pos.x) <= pos.z && abs(pos.y) <= pos.z) {
			// Top
			gl_Layer = 4;
			gl_Position = vec4(
				 pos.x / pos.z,
				-pos.y / pos.z,
				0, 1
			);
		} else 
		if (-pos.z > 0 && abs(pos.x) <= -pos.z && abs(pos.y) <= -pos.z) {
			// Bottom
			gl_Layer = 5;
			gl_Position = vec4(
				pos.x / pos.z,
				pos.y / pos.z,
				0, 1
			);
		}
		
		// Magical formula to adjust point size for sperical cubemap... Took 3 days of intensive math to figure it out...
		gl_PointSize *= (tan(length(gl_Position.xy)/sqrt(2))+0.5)/2;
		
		EmitVertex();
	}
}

##################################################################
#shader gen.frag

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void main() {
	float center = 1 - pow(length(gl_PointCoord * 2 - 1), 1.0 / max(0.7, in_color.a));
	out_color = vec4(in_color.rgb * in_color.a, in_color.a) * center;
}

##################################################################
#shader fade.vert

layout(location = 0) out int index;

void main() {
	index = gl_VertexIndex;
}

##################################################################
#shader fade.geom

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in int index[];

void main(void) {
	gl_Position = vec4(-1, 1, 0, 1);
	gl_Layer = index[0];
	EmitVertex();
	gl_Position = vec4(-1, -1, 0, 1);
	gl_Layer = index[0];
	EmitVertex();
	gl_Position = vec4(1, 1, 0, 1);
	gl_Layer = index[0];
	EmitVertex();
	gl_Position = vec4(1, -1, 0, 1);
	gl_Layer = index[0];
	EmitVertex();
	
	EndPrimitive();
}

##################################################################
#shader fade.frag

layout(location = 0) out vec4 out_color;

void main() {
	out_color = vec4(0.004,0.004,0.004, 1);
}

##################################################################
#shader box.vert

layout(push_constant) uniform GalaxyBoxPushConstant {
	dmat4 inverseProjectionView;
} galaxyBox;

layout(location = 0) out vec3 out_dir;

void main() {
	// Full-screen Quad from 4 empty vertices
	dvec2 pos = vec2((gl_VertexIndex & 2)>>1, 1-(gl_VertexIndex & 1)) * 2.0 - 1.0;
	gl_Position = vec4(vec2(pos), 0, 1);
	// output direction of vertex into world
	out_dir = vec3(normalize(galaxyBox.inverseProjectionView * dvec4(pos, 1, 1)).xyz);
}

##################################################################
#shader box.frag

layout(location = 0) in vec3 in_dir;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform samplerCube galaxyBox;

void main() {
	out_color = texture(galaxyBox, in_dir);
}
