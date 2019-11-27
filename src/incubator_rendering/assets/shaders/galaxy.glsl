#version 460 core
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shader_viewport_layer_array : enable

layout(set = 0, binding = 0) uniform UBO {
	dmat4 proj;
	dmat4 view;
	dmat4 model;
	dvec3 cameraPosition;
	float speed;
	int galaxyFrameIndex;
} ubo;

##################################################################

#shader gen.vert

layout(location = 0) in vec4 posr;
layout(location = 1) in uint seed;

layout(location = 0) out uint out_seed;

void main() {
	gl_Position = posr; // passthrough
	out_seed = seed + ubo.galaxyFrameIndex;
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
layout(points, max_vertices = 92) out; // takes up 7 components per vertex (1 for gl_PointSize, 4 for gl_Position, 2 for gl_PointCoord)
layout(location = 0) out vec4 out_color; // takes up 4 components

layout(location = 0) in uint in_seed[];

const float MAX_VIEW_DISTANCE = 40;

void main(void) {
	uint seed = in_seed[0];
	uint fseed = seed + 15;
	
	vec3 wpos = gl_in[0].gl_Position.xyz;
	float radius = gl_in[0].gl_Position.w;
	
	for (int i = 0; i < 10; i++) {
		// position relative to camera
		vec4 pos = vec4(wpos + RandomInUnitSphere(seed), 1) - vec4(vec3(ubo.cameraPosition), 0);
		
		gl_PointSize = radius + RandomFloat(fseed) + smoothstep(MAX_VIEW_DISTANCE, 0, length(pos))*2;
		
		vec4 starType = normalize(vec4(
			/*red*/		(RandomFloat(fseed) * 2 - 1) * 1.3 ,
			/*yellow*/	(RandomFloat(fseed) * 2 - 1) * 1.5 ,
			/*blue*/	(RandomFloat(fseed) * 2 - 1) * 0.7 ,
			/*white*/	(RandomFloat(fseed) * 2 - 1) * 1.8 
		));
		
		vec3 color =/*red*/		vec3( 1.0 , 0.8 , 0.6 ) * starType.x +
					/*yellow*/	vec3( 1.0 , 1.0 , 0.7 ) * starType.y +
					/*blue*/	vec3( 0.6 , 0.8 , 1.0 ) * starType.z +
					/*white*/	vec3( 1.0 , 1.0 , 1.0 ) * starType.w ;
		
		out_color = vec4(
			color,
			RandomFloat(fseed) * smoothstep(MAX_VIEW_DISTANCE, 0, length(pos))
		);
		
		float u = 0;
		float v = 0;
		
		// Compute which side of the cube map we should render to
		if (pos.x > 0 && abs(pos.z) < pos.x && abs(pos.y) < pos.x) {
			// Right
			u = -pos.z / pos.x;
			v = -pos.y / pos.x;
			gl_Layer = 0;
		} else 
		if (-pos.x > 0 && abs(pos.z) < -pos.x && abs(pos.y) < -pos.x) {
			// Left
			u = -pos.z / pos.x;
			v = pos.y / pos.x;
			gl_Layer = 1;
		} else
		if (pos.y > 0 && abs(pos.z) < pos.y && abs(pos.x) < pos.y) {
			// Front
			u = pos.x / pos.y;
			v = pos.z / pos.y;
			gl_Layer = 2;
		} else 
		if (-pos.y > 0 && abs(pos.z) < -pos.y && abs(pos.x) < -pos.y) {
			// Back
			u = -pos.x / pos.y;
			v = pos.z / pos.y;
			gl_Layer = 3;
		} else 
		if (pos.z > 0 && abs(pos.x) < pos.z && abs(pos.y) < pos.z) {
			// Top
			u = pos.x / pos.z;
			v = -pos.y / pos.z;
			gl_Layer = 4;
		} else 
		if (-pos.z > 0 && abs(pos.x) < -pos.z && abs(pos.y) < -pos.z) {
			// Bottom
			u = pos.x / pos.z;
			v = pos.y / pos.z;
			gl_Layer = 5;
		}
		
		gl_Position = vec4(u, v, 0, 1);
		
		EmitVertex();
	}
}

##################################################################

#shader gen.frag

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void main() {
	float center = 1 - pow(length(gl_PointCoord * 2 - 1), 1.0 / max(0.7, in_color.a));
	out_color = vec4(in_color.rgb, in_color.a) * center;
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
	out_color = vec4(0.003,0.003,0.003, 1);
}

##################################################################

#shader box.vert

layout(location = 0) out vec3 out_uv;

void main() {
	// Generate a cube, simply from vertex id
	int r = int(gl_VertexIndex > 6);
	int i = r==1 ? 13-gl_VertexIndex : gl_VertexIndex;
	int x = int(i<3 || i==4);
	int y = r ^ int(i>0 && i<4);
	int z = r ^ int(i<2 || i>5);
	// output vertex positions and coords
	out_uv = vec3(x, y, z) * 2.0 - 1.0;
	gl_Position = mat4(ubo.proj) * mat4(ubo.view) * vec4(out_uv, 0);
}

##################################################################

#shader box.frag

layout(location = 0) in vec3 in_uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 1) uniform samplerCube galaxyBox;

void main() {
	out_color = texture(galaxyBox, normalize(in_uv));
}

