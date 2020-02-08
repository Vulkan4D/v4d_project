#version 460 core
#extension GL_ARB_shader_viewport_layer_array : enable

precision highp int;
precision highp float;
precision highp sampler2D;

#common gen.*

// #define GALAXY_INFO_USE_VERTEX_BINDINGS // Seems to be slower than calculating all values directly in geometry shader
// #define GALAXY_INFO_CALCULATE_IN_VERTEX_SHADER // geometry shader is as fast as vertex shader. Having to transfer it from vert to geom causes overhead. 

#include "_noise.glsl"
#include "_v4dnoise.glsl"

layout(std430, push_constant) uniform GalaxyGenPushConstant {
	dvec3 cameraPosition;
	int frameIndex;
	int resolution;
} galaxyGen;

#common box.*
#include "_v4d_baseDescriptorSet.glsl"

##################################################################
#shader gen.vert

layout(location = 0) in vec4 posr;
#ifdef GALAXY_INFO_USE_VERTEX_BINDINGS
	layout(location = 1) in float spiralCloudsFactor;
	layout(location = 2) in float swirlTwist;
	layout(location = 3) in float swirlDetail;
	layout(location = 4) in float coreSize;
	layout(location = 5) in float cloudsSize;
	layout(location = 6) in float cloudsFrequency;
	layout(location = 7) in float squish;
	layout(location = 8) in float attenuationCloudsFrequency;
	layout(location = 9) in float attenuationCloudsFactor;
	layout(location = 10) in vec3 position;
	layout(location = 11) in vec3 noiseOffset;
	layout(location = 12) in float irregularities;
	layout(location = 13) in mat4 rotation;
#endif

layout(location = 0) out uint out_seed;
#if defined(GALAXY_INFO_USE_VERTEX_BINDINGS) || defined(GALAXY_INFO_CALCULATE_IN_VERTEX_SHADER)
	layout(location = 1) out GalaxyInfo out_galaxyInfo;
#endif

void main() {
	gl_Position = posr; // passthrough
	out_seed = galaxyGen.frameIndex;
	#ifdef GALAXY_INFO_USE_VERTEX_BINDINGS
		out_galaxyInfo.spiralCloudsFactor = spiralCloudsFactor;
		out_galaxyInfo.swirlTwist = swirlTwist;
		out_galaxyInfo.swirlDetail = swirlDetail;
		out_galaxyInfo.coreSize = coreSize;
		out_galaxyInfo.cloudsSize = cloudsSize;
		out_galaxyInfo.cloudsFrequency = cloudsFrequency;
		out_galaxyInfo.squish = squish;
		out_galaxyInfo.attenuationCloudsFrequency = attenuationCloudsFrequency;
		out_galaxyInfo.attenuationCloudsFactor = attenuationCloudsFactor;
		out_galaxyInfo.position = position;
		out_galaxyInfo.noiseOffset = noiseOffset;
		out_galaxyInfo.irregularities = irregularities;
		out_galaxyInfo.rotation = rotation;
	#else
		#ifdef GALAXY_INFO_CALCULATE_IN_VERTEX_SHADER
			out_galaxyInfo = GetGalaxyInfo(posr.xyz);
		#endif
	#endif
}

##################################################################
#shader gen.geom


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
#if defined(GALAXY_INFO_USE_VERTEX_BINDINGS) || defined(GALAXY_INFO_CALCULATE_IN_VERTEX_SHADER)
	layout(location = 1) in GalaxyInfo in_galaxyInfo[];
#endif

const float MIN_VIEW_DISTANCE = 0.0;
const float MAX_VIEW_DISTANCE = 2000.0;

float linearstep(float a, float b, float x) {
	return (x - a) / (b - a);
}

void main(void) {
	uint seed = in_seed[0];
	vec3 relPos = vec3(dvec3(gl_in[0].gl_Position.xyz) - galaxyGen.cameraPosition.xyz); // position relative to camera
	float radius = gl_in[0].gl_Position.w;
	
	float dist = length(relPos);
	float sizeInScreen = radius / dist * float(galaxyGen.resolution);
	
	if (sizeInScreen < 200 && galaxyGen.frameIndex > 100 && sizeInScreen < sqrt(galaxyGen.frameIndex)) return;
	
	int nbStarsToDraw = int(max(1, min(80, sizeInScreen / 10.0)));
	float brightnessBasedOnDistance = linearstep(MAX_VIEW_DISTANCE, MIN_VIEW_DISTANCE, dist);// 1.0 - pow(1.0-linearstep(MAX_VIEW_DISTANCE, MIN_VIEW_DISTANCE, dist), 2.0);
	
	if (brightnessBasedOnDistance < 0.01) return;
	
	#if defined(GALAXY_INFO_USE_VERTEX_BINDINGS) || defined(GALAXY_INFO_CALCULATE_IN_VERTEX_SHADER)
		GalaxyInfo info = in_galaxyInfo[0];
	#else
		GalaxyInfo info = GetGalaxyInfo(gl_in[0].gl_Position.xyz);
	#endif
	
	for (int i = 0; i < nbStarsToDraw; i++) {
		// gl_PointSize = brightnessBasedOnDistance * 8.0 + 1.0;
		gl_PointSize = 5;
		vec3 starPos = RandomInUnitSphere(seed);
		starPos = mix(starPos, starPos * length(starPos), 0.5);
		
		float starDensity = GalaxyStarDensity(starPos, info, int(max(1.0, min(8.0, sizeInScreen/100.0))));
		
		if (starDensity == 0.0) continue;
		
		vec3 color = GalaxyStarColor(starPos, info);
		
		out_color = vec4(
			color * starDensity,
			brightnessBasedOnDistance * starDensity
		);
		
		vec3 pos = relPos + starPos*radius;
		
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
		// gl_PointSize *= (tan(length(gl_Position.xy)/sqrt(2))+0.5)/2;
		
		EmitVertex();
	}
}

##################################################################
#shader gen.frag

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void main() {
	float center = 1.0 - length(gl_PointCoord * 2 - 1);
	out_color = vec4(in_color.rgb, in_color.a) * center;
}








// layout(points) in;
// layout(points, max_vertices = 80) out;

// void main(void) {
// 	vec3 relPos = vec3(dvec3(gl_in[0].gl_Position.xyz) - galaxyGen.cameraPosition.xyz); // position relative to camera
// 	float radius = gl_in[0].gl_Position.w;
	
// 	float dist = length(relPos);
// 	float sizeInScreen = radius / dist * float(galaxyGen.resolution);
	
// 	gl_PointSize = sizeInScreen;
	
// 	vec3 pos = relPos;
	
// 	// Compute which side of the cube map we should render to
// 	if (pos.x > 0 && abs(pos.z) <= pos.x && abs(pos.y) <= pos.x) {
// 		// Right
// 		gl_Layer = 0;
// 		gl_Position = vec4(
// 			-pos.z / pos.x,
// 			-pos.y / pos.x,
// 			0, 1
// 		);
// 	} else 
// 	if (-pos.x > 0 && abs(pos.z) <= -pos.x && abs(pos.y) <= -pos.x) {
// 		// Left
// 		gl_Layer = 1;
// 		gl_Position = vec4(
// 			-pos.z / pos.x,
// 				pos.y / pos.x,
// 			0, 1
// 		);
// 	} else
// 	if (pos.y > 0 && abs(pos.z) <= pos.y && abs(pos.x) <= pos.y) {
// 		// Front
// 		gl_Layer = 2;
// 		gl_Position = vec4(
// 			pos.x / pos.y,
// 			pos.z / pos.y,
// 			0, 1
// 		);
// 	} else 
// 	if (-pos.y > 0 && abs(pos.z) <= -pos.y && abs(pos.x) <= -pos.y) {
// 		// Back
// 		gl_Layer = 3;
// 		gl_Position = vec4(
// 			-pos.x / pos.y,
// 			pos.z / pos.y,
// 			0, 1
// 		);
// 	} else 
// 	if (pos.z > 0 && abs(pos.x) <= pos.z && abs(pos.y) <= pos.z) {
// 		// Top
// 		gl_Layer = 4;
// 		gl_Position = vec4(
// 			pos.x / pos.z,
// 			-pos.y / pos.z,
// 			0, 1
// 		);
// 	} else 
// 	if (-pos.z > 0 && abs(pos.x) <= -pos.z && abs(pos.y) <= -pos.z) {
// 		// Bottom
// 		gl_Layer = 5;
// 		gl_Position = vec4(
// 			pos.x / pos.z,
// 			pos.y / pos.z,
// 			0, 1
// 		);
// 	}
	
// 	// Magical formula to adjust point size for sperical cubemap... Took 3 days of intensive math to figure it out...
// 	// gl_PointSize *= (tan(length(gl_Position.xy)/sqrt(2))+0.5)/2;
	
// 	EmitVertex();
// }


// ##################################################################
// #shader gen.frag

// layout(location = 0) out vec4 o_color;
// void main() {
// 	o_color = vec4(1,1,1,1);
// }








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
	// out_color = vec4(0.004,0.004,0.004, 1);
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

layout(set = 1, binding = 0) uniform samplerCube galaxyBox;

void main() {
	out_color = texture(galaxyBox, in_dir);
}
