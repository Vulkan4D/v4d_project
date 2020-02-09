#include "core.glsl"
#include "Camera.glsl"

// https://www.thoughtco.com/rock-identification-tables-1441174
const int NB_ROCK_TYPES = 44; // must be a multiple of 4

layout(std430, push_constant) uniform PlanetChunk {
	mat4 modelViewMatrix;
	ivec3 chunkPos;
	float chunkSize;
	vec3 northDir;
	int vertexSubdivisionsPerChunk;
	bool isLastLevel;
} planetChunk;

struct V2F {
	vec4 pos_alt; // w = altitude
	vec4 normal_slope; // w = slope
	vec3 tangentX;
	vec3 tangentY;
	vec4 uv_wet_snow;
	vec4 rockTypes[NB_ROCK_TYPES/4];
	vec4 terrainType;
	vec4 terrainFeature;
	vec4 sand;
	vec4 dust;
};

vec3 ViewSpaceNormal(vec3 normal) {
	return normalize(transpose(inverse(mat3(planetChunk.modelViewMatrix))) * normal);
}

##################################################################
#shader vert

layout(location = 0) in vec4 v_pos_alt;
layout(location = 1) in vec4 v_uv_wet_snow;
layout(location = 2) in vec4 v_normal_slope;
layout(location = 3) in uint v_rockType;
layout(location = 4) in uint v_terrainType;
layout(location = 5) in uint v_terrainFeature;
layout(location = 6) in uint v_sand;
layout(location = 7) in uint v_dust;

layout(location = 0) out V2F v2f;

void main() {
	gl_Position = mat4(camera.projectionMatrix) * planetChunk.modelViewMatrix * vec4(v_pos_alt.xyz, 1);
	
	v2f.pos_alt = v_pos_alt;
	v2f.normal_slope = v_normal_slope;
	v2f.tangentX = cross(planetChunk.northDir, v_normal_slope.xyz);
	v2f.tangentY = cross(normalize(v_normal_slope.xyz), v2f.tangentX);
	v2f.uv_wet_snow = v_uv_wet_snow;
	
	ivec4 rockTypes = UnpackIVec4FromUint(v_rockType);
	float total = float((rockTypes.x>0?1:0) + (rockTypes.y>0?1:0) + (rockTypes.z>0?1:0) + (rockTypes.w>0?1:0));
	if (total > 0.0) {
		for (int i = 0; i < NB_ROCK_TYPES; i+=4) {
			v2f.rockTypes[i] = vec4(
				(rockTypes.x==i+1 || rockTypes.y==i+1 || rockTypes.z==i+1 || rockTypes.w==i+1)? 1.0/total : 0.0,
				(rockTypes.x==i+2 || rockTypes.y==i+2 || rockTypes.z==i+2 || rockTypes.w==i+2)? 1.0/total : 0.0,
				(rockTypes.x==i+3 || rockTypes.y==i+3 || rockTypes.z==i+3 || rockTypes.w==i+3)? 1.0/total : 0.0,
				(rockTypes.x==i+4 || rockTypes.y==i+4 || rockTypes.z==i+4 || rockTypes.w==i+4)? 1.0/total : 0.0
			);
		}
	}
	v2f.terrainType = UnpackVec4FromUint(v_terrainType);
	v2f.terrainFeature = UnpackVec4FromUint(v_terrainFeature);
	v2f.sand = UnpackVec4FromUint(v_sand);
	v2f.dust = UnpackVec4FromUint(v_dust);
}

##################################################################
#shader surface.frag

#include "incubator_rendering/assets/shaders/_v4dnoise.glsl"
#include "gBuffers_out.glsl"

layout(location = 0) in V2F v2f;
mat4 modelMatrix = inverse(mat4(camera.viewMatrix)) * planetChunk.modelViewMatrix;

// Fragment information
vec3 f_pos = v2f.pos_alt.xyz;
float f_altitude = v2f.pos_alt.w;
vec3 f_normal = v2f.normal_slope.xyz;
float f_slope = v2f.normal_slope.w;
vec2 f_uv = v2f.uv_wet_snow.st;
float f_wet = v2f.uv_wet_snow.p;
float f_snow = v2f.uv_wet_snow.q;
vec4 f_sand = v2f.sand;
vec4 f_dust = v2f.dust;
vec3 f_viewPos = (planetChunk.modelViewMatrix * vec4(f_pos, 1)).xyz;
float f_trueDistance = clamp(distance(vec3(camera.worldPosition), (modelMatrix * vec4(f_pos, 1)).xyz), float(camera.znear), float(camera.zfar));

struct RockDetail { // 3 rgba image textures or in-shader procedural generation
	vec3 albedo;
	vec3 normal;
	vec3 emission;
	float roughness;
	float metallic;
	float scatter;
};

RockDetail RockType_1() {
	RockDetail rock;
	
	rock.albedo = vec3(0.6,0.4,0.2);
	
	// Normals
	vec2 normalsUV = abs(f_uv - 0.5) * float(planetChunk.vertexSubdivisionsPerChunk);
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
	rock.normal = normalize(mix(f_normal, f_normal + normalNoise, 0.5));
	
	return rock;
}

void GetRockColorBlend(inout RockDetail dst, in RockDetail src, float blendFactor) {
	dst.albedo += src.albedo * blendFactor;
	dst.normal += src.normal * blendFactor;
	dst.emission += src.emission * blendFactor;
	dst.roughness += src.roughness * blendFactor;
	dst.metallic += src.metallic * blendFactor;
	dst.scatter += src.scatter * blendFactor;
}

void main() {
	vec3 albedo = vec3(0);
	vec3 normal;
	vec3 emission = vec3(0);
	float roughness = 0;
	float metallic = 0;
	float scatter = 0;
	float occlusion = 0;
	
	// dvec3 posOnPlanet = dvec3(planetChunk.chunkPos) + dvec3(f_pos);
	
	RockDetail rocks;
	rocks.albedo = vec3(0);
	rocks.normal = vec3(0);
	rocks.emission = vec3(0);
	rocks.roughness = 0;
	rocks.metallic = 0;
	rocks.scatter = 0;
	
	const float rockType_1 = v2f.rockTypes[0].x;
	// const float rockType_2 = v2f.rockTypes[0].y;
	// const float rockType_3 = v2f.rockTypes[0].z;
	// const float rockType_4 = v2f.rockTypes[0].w;
	// const float rockType_5 = v2f.rockTypes[1].x;
	//...
	
	if (rockType_1 > 0) GetRockColorBlend(rocks, RockType_1(), rockType_1);
	// if (rockType_2 > 0) GetRockColorBlend(rocks, TerrainType_2(), rockType_2);
	//...
	albedo += rocks.albedo;
	normal = rocks.normal;
	emission += rocks.emission;
	roughness += rocks.roughness;
	metallic += rocks.metallic;
	scatter += rocks.scatter;
	
	// Snow
	vec2 uv = abs(f_uv - 0.5) * float(planetChunk.vertexSubdivisionsPerChunk);
	float slopeNoise = Noise(uv)+Noise(uv*7.81)*0.6+Noise(uv*19.2)*0.3;
	if (planetChunk.isLastLevel) {
		slopeNoise += Noise(uv*45.37)*0.2;
		slopeNoise += Noise(uv*98.12)*0.1;
		slopeNoise += Noise(uv*256.94)*0.1;
	}
	float slope = mix(f_slope, slopeNoise, 0.1);
	albedo = mix(albedo, vec3(1), smoothstep(0.85, 0.98, slope));
	
	// May discard fragment here...
	
	GBuffers gBuffers;
	gBuffers.albedo = vec4(albedo, 1);
	gBuffers.normal = ViewSpaceNormal(normalize(normal));
	gBuffers.roughness = roughness;
	gBuffers.metallic = metallic;
	gBuffers.scatter = scatter;
	gBuffers.occlusion = clamp(occlusion, 0, 1);
	gBuffers.emission = emission;
	gBuffers.position = vec4(f_viewPos, f_trueDistance);
	WriteGBuffers(gBuffers);
}
