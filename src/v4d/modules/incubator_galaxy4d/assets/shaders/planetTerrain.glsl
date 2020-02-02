#version 460 core
#extension GL_ARB_enhanced_layouts : enable

precision highp int;
precision highp float;
precision highp sampler2D;

#include "Camera.glsl"

// https://www.thoughtco.com/rock-identification-tables-1441174
const int NB_ROCK_TYPES = 44; // must be a multiple of 4

layout(std430, push_constant) uniform PlanetChunk {
	mat4 modelViewMatrix;
	ivec3 chunkPos;
	float chunkSize;
	float radius;
	float solidRadius;
	int level;
	bool isLastLevel;
	int vertexSubdivisionsPerChunk;
	float cameraAltitudeAboveTerrain;
	float cameraDistanceFromPlanet;
	// float ???;
	vec3 northDir;
	// float ???;
} planetChunk;

struct V2F {
	vec3 pos;
	float altitude;
	vec3 normal;
	float slope;
	vec3 tangentX;
	vec3 tangentY;
	vec2 uv;
	float wet;
	float snow;
	vec4 sand;
	vec4 dust;
	vec4 terrainTypes[NB_ROCK_TYPES/4];
	// one vec4 remaining for future use when using 80 rock types
};

vec3 ViewSpaceNormal(vec3 normal) {
	return normalize(transpose(inverse(mat3(planetChunk.modelViewMatrix))) * normal);
}

##################################################################
#shader vert

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 uv;
layout(location = 2) in vec4 normal;
layout(location = 3) in uint sand;
layout(location = 4) in uint dust;
layout(location = 5) in uint terrainType;

layout(location = 0) out V2F v2f;

ivec4 UnpackIVec4FromUint(uint i) {
	return ivec4(
		(i & 0xff000000) >> 24,
		(i & 0x00ff0000) >> 16,
		(i & 0x0000ff00) >> 8,
		(i & 0x000000ff)
	);
}

vec4 UnpackVec4FromUint(uint i) {
	return vec4(
		(i & 0xff000000) >> 24,
		(i & 0x00ff0000) >> 16,
		(i & 0x0000ff00) >> 8,
		(i & 0x000000ff)
	);
}

void main() {
	gl_Position = mat4(camera.projectionMatrix) * planetChunk.modelViewMatrix * vec4(pos.xyz, 1);
	
	v2f.pos = pos.xyz;
	v2f.altitude = pos.a;
	v2f.normal = normal.xyz;
	v2f.slope = normal.w;
	v2f.tangentX = cross(planetChunk.northDir, normal.xyz);
	v2f.tangentY = cross(normalize(normal.xyz), v2f.tangentX);
	v2f.uv = uv.st;
	
	v2f.wet = uv.p;
	v2f.snow = uv.q;
	
	v2f.sand = UnpackVec4FromUint(sand);
	v2f.dust = UnpackVec4FromUint(dust);
	
	ivec4 terrainTypes = UnpackIVec4FromUint(terrainType);
	float total = float((terrainTypes.x>0?1:0) + (terrainTypes.y>0?1:0) + (terrainTypes.z>0?1:0) + (terrainTypes.w>0?1:0));
	if (total > 0.0) {
		for (int i = 0; i < NB_ROCK_TYPES; i+=4) {
			v2f.terrainTypes[i] = vec4(
				(terrainTypes.x==i+1 || terrainTypes.y==i+1 || terrainTypes.z==i+1 || terrainTypes.w==i+1)? 1.0/total : 0.0,
				(terrainTypes.x==i+2 || terrainTypes.y==i+2 || terrainTypes.z==i+2 || terrainTypes.w==i+2)? 1.0/total : 0.0,
				(terrainTypes.x==i+3 || terrainTypes.y==i+3 || terrainTypes.z==i+3 || terrainTypes.w==i+3)? 1.0/total : 0.0,
				(terrainTypes.x==i+4 || terrainTypes.y==i+4 || terrainTypes.z==i+4 || terrainTypes.w==i+4)? 1.0/total : 0.0
			);
		}
	}
}

##################################################################
#shader surface.frag

#include "incubator_rendering/assets/shaders/_v4dnoise.glsl"
#include "gBuffers_out.glsl"

layout(location = 0) in V2F v2f;

struct TerrainDetail {
	vec3 albedo;
	vec3 normal;
	vec3 emission;
	float roughness;
	float metallic;
	float scatter;
};

TerrainDetail TerrainType_1() {
	TerrainDetail t;
	
	t.albedo = vec3(0.6,0.4,0.2);
	
	// Normals
	vec2 normalsUV = abs(v2f.uv - 0.5) * float(planetChunk.vertexSubdivisionsPerChunk);
	float normalNoiseX = Noise(normalsUV+0.865)*0.4 + Noise(normalsUV*8.0+0.2685)*0.5 + Noise(normalsUV*20.0+20.85)*0.4;
	float normalNoiseY = Noise(normalsUV+24.5)*0.4 + Noise(normalsUV*8.0+21.5)*0.5 + Noise(normalsUV*20.0+150.5)*0.4;
	if (planetChunk.isLastLevel) {
		normalNoiseX += Noise(normalsUV*50.0+201.85)*0.25 + Noise(normalsUV*100.0+450.25)*0.15;
		normalNoiseY += Noise(normalsUV*50.0+300.5)*0.25 + Noise(normalsUV*100.0+120.78)*0.15;
	}
	vec3 normalNoise = ((
		+ normalize(v2f.tangentX) * normalNoiseX
		+ normalize(v2f.tangentY) * normalNoiseY
	) - 0.5) / 2.0;
	t.normal = normalize(mix(v2f.normal, v2f.normal + normalNoise, 0.5));
	
	return t;
}

void GetRockColorBlend(inout TerrainDetail dst, in TerrainDetail src, float blendFactor) {
	dst.albedo += src.albedo * blendFactor;
	dst.normal += src.normal * blendFactor;
	dst.emission += src.emission * blendFactor;
	dst.roughness += src.roughness * blendFactor;
	dst.metallic += src.metallic * blendFactor;
	dst.scatter += src.scatter * blendFactor;
}

void main() {
	// dvec3 posOnPlanet = dvec3(planetChunk.chunkPos) + dvec3(v2f.pos);
	
	TerrainDetail terrain;
	terrain.albedo = vec3(0);
	terrain.normal = vec3(0);
	terrain.emission = vec3(0);
	terrain.roughness = 0;
	terrain.metallic = 0;
	terrain.scatter = 0;
	
	const float terrainType_1 = v2f.terrainTypes[0].x;
	// const float terrainType_2 = v2f.terrainTypes[0].y;
	// const float terrainType_3 = v2f.terrainTypes[0].z;
	// const float terrainType_4 = v2f.terrainTypes[0].w;
	// const float terrainType_5 = v2f.terrainTypes[1].x;
	//...
	
	if (terrainType_1 > 0) GetRockColorBlend(terrain, TerrainType_1(), terrainType_1);
	// if (terrainType_2 > 0) GetRockColorBlend(terrain, TerrainType_2(), terrainType_2);
	//...
	
	// Snow
	vec2 uv = abs(v2f.uv - 0.5) * float(planetChunk.vertexSubdivisionsPerChunk);
	float slopeNoise = Noise(uv)+Noise(uv*7.81)*0.6+Noise(uv*19.2)*0.3;
	if (planetChunk.isLastLevel) {
		slopeNoise += Noise(uv*45.37)*0.2;
		slopeNoise += Noise(uv*98.12)*0.1;
		slopeNoise += Noise(uv*256.94)*0.1;
	}
	float slope = mix(v2f.slope, slopeNoise, 0.1);
	terrain.albedo = mix(terrain.albedo, vec3(1), smoothstep(0.85, 0.98, slope));
	
	GBuffers gBuffers;
	
	gBuffers.albedo = vec4(terrain.albedo, 1);
	gBuffers.normal = ViewSpaceNormal(normalize(terrain.normal));
	gBuffers.roughness = terrain.roughness;
	gBuffers.metallic = terrain.metallic;
	gBuffers.scatter = terrain.scatter;
	gBuffers.occlusion = 0;
	gBuffers.emission = terrain.emission;
	gBuffers.position = (planetChunk.modelViewMatrix * vec4(v2f.pos, 1)).xyz;
	
	WriteGBuffers(gBuffers);
}

// #################
// #shader wireframe.geom

// layout(triangles) in;
// layout(line_strip, max_vertices = 12) out;

// layout(location = 0) in V2F in_v2f[];
// layout(location = 0) out V2F out_v2f;

// void main() {
	
// 	float normalsLength = 500000.0;

// 	// Mesh
// 	gl_Position = gl_in[0].gl_Position;
// 	out_v2f = in_v2f[0];
// 	EmitVertex();
// 	gl_Position = gl_in[1].gl_Position;
// 	out_v2f = in_v2f[1];
// 	EmitVertex();

// 	EndPrimitive();

// 	gl_Position = gl_in[1].gl_Position;
// 	out_v2f = in_v2f[1];
// 	EmitVertex();
// 	gl_Position = gl_in[2].gl_Position;
// 	out_v2f = in_v2f[2];
// 	EmitVertex();

// 	EndPrimitive();

// 	gl_Position = gl_in[2].gl_Position;
// 	out_v2f = in_v2f[2];
// 	EmitVertex();
// 	gl_Position = gl_in[0].gl_Position;
// 	out_v2f = in_v2f[0];
// 	EmitVertex();

// 	EndPrimitive();

// 	// Normals
// 	gl_Position = gl_in[0].gl_Position;
// 	out_v2f = in_v2f[0];
// 	EmitVertex();
// 	gl_Position = mat4(camera.projectionMatrix) * (inverse(mat4(camera.projectionMatrix)) * gl_in[0].gl_Position + vec4(in_v2f[1].normal * normalsLength, 0));
// 	out_v2f = in_v2f[1];
// 	EmitVertex();

// 	EndPrimitive();

// 	gl_Position = gl_in[1].gl_Position;
// 	out_v2f = in_v2f[1];
// 	EmitVertex();
// 	gl_Position = mat4(camera.projectionMatrix) * (inverse(mat4(camera.projectionMatrix)) * gl_in[1].gl_Position + vec4(in_v2f[2].normal * normalsLength, 0));
// 	out_v2f = in_v2f[2];
// 	EmitVertex();

// 	EndPrimitive();

// 	gl_Position = gl_in[2].gl_Position;
// 	out_v2f = in_v2f[2];
// 	EmitVertex();
// 	gl_Position = mat4(camera.projectionMatrix) * (inverse(mat4(camera.projectionMatrix)) * gl_in[2].gl_Position + vec4(in_v2f[0].normal * normalsLength, 0));
// 	out_v2f = in_v2f[0];
// 	EmitVertex();

// 	EndPrimitive();
	
// }

