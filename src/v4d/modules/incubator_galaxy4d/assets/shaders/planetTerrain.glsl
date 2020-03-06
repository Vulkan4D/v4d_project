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
vec3 f_tangentX = cross(planetChunk.northDir, f_normal);
vec3 f_tangentY = cross(normalize(f_normal), f_tangentX);

vec2 normalsUV = abs(f_uv - 0.5) * float(planetChunk.vertexSubdivisionsPerChunk);

struct RockDetail { // 3 rgba image textures or in-shader procedural generation
	vec3 albedo;
	vec3 normal;
	vec3 emission;
	float roughness;
	float metallic;
};

RockDetail RockType_1() {
	RockDetail rock;
	
	float cracks = 0;
	if (planetChunk.isLastLevel) {
		cracks += clamp(pow(1-abs(FastSimplexFractal(vec3(normalsUV/2.1+2.12, 43.67), 4)), 20) - abs(Noise(normalsUV/2+98.12)), 0, 1);
		cracks = clamp(cracks, 0, 1);
	}
	rock.albedo = mix(vec3(0.25,0.2,0.15), vec3(0), cracks);
	
	// Normals
	float normalNoiseX = Noise(normalsUV*4.0+10.25) + Noise(normalsUV*10.0+20.85);
	float normalNoiseY = Noise(normalsUV*4.0+21.5) + Noise(normalsUV*10.0+150.5);
	// if (planetChunk.isLastLevel) {
		normalNoiseX += Noise(normalsUV*50.0+201.85);
		normalNoiseY += Noise(normalsUV*50.0+300.5);
	// }
	vec3 normalNoise = ((
		+ normalize(f_tangentX) * normalNoiseX
		+ normalize(f_tangentY) * normalNoiseY
	) - 0.5) / 2.0;
	rock.normal = normalize(mix(f_normal, f_normal + normalNoise, 0.5));
	
	rock.roughness = 0.6;
	rock.metallic = 0.1;
	rock.emission = vec3(0);
	
	return rock;
}

void GetRockColorBlend(inout RockDetail dst, in RockDetail src, float blendFactor) {
	dst.albedo += src.albedo * blendFactor;
	dst.normal += src.normal * blendFactor;
	dst.emission += src.emission * blendFactor;
	dst.roughness += src.roughness * blendFactor;
	dst.metallic += src.metallic * blendFactor;
}

void main() {
	vec3 albedo = vec3(0);
	vec3 normal;
	vec3 emission = vec3(0);
	float roughness = 0;
	float metallic = 0;
	
	dvec3 posOnPlanet = dvec3(planetChunk.chunkPos) + dvec3(f_pos);
	
	RockDetail rocks;
	rocks.albedo = vec3(0);
	rocks.normal = vec3(0);
	rocks.emission = vec3(0);
	rocks.roughness = 0;
	rocks.metallic = 0;
	
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
	
	// Snow
	vec2 uv = abs(f_uv - 0.5) * float(planetChunk.vertexSubdivisionsPerChunk);
	float slopeNoise = Noise(uv)+Noise(uv*5.81)*0.6+Noise(uv*12.2)*0.3;
	float slope = clamp(mix(f_slope, slopeNoise, 0.1), 0,1);
	float snowSlope = mix(1, 0.7, f_snow);
	if (slope > snowSlope) {
		float snowGlitter = mix(pow(max(0,Noise(uv*35)),6) + pow(max(0,Noise(uv*70+12.85)),8), 0, smoothstep(20, 500, f_trueDistance));
		snowGlitter += mix(pow(max(0,Noise(uv*85)),6) + pow(max(0,Noise(uv*150+12.85)),8), 0, smoothstep(5, 50, f_trueDistance));
		
		// Normals
		float normalNoiseX = Noise(normalsUV+0.365)*0.4 + Noise(normalsUV*6.0+0.5685)*0.5 + Noise(normalsUV*12.0+8.85)*0.4;
		float normalNoiseY = Noise(normalsUV+14.5)*0.4 + Noise(normalsUV*6.0+21.5)*0.5 + Noise(normalsUV*12.0+110.25)*0.4;
		vec3 normalNoise = ((
			+ normalize(f_tangentX) * normalNoiseX
			+ normalize(f_tangentY) * normalNoiseY
		) - 0.5) / 2.0;
		
		vec3 snowColor = vec3(1);
		if (planetChunk.isLastLevel) {
			snowColor -= vec3(Noise(normalsUV*86.0+11.5),Noise(normalsUV*87.0+11.5),Noise(normalsUV*83.0+16.5))/2;
			snowColor -= vec3(Noise(normalsUV*148.0+38.5),Noise(normalsUV*148.0+1.5),Noise(normalsUV*140.0+82.5))/2;
			snowColor += vec3(Noise(normalsUV*946.0+11.5),Noise(normalsUV*945.0+11.5),Noise(normalsUV*944.0+16.5));
		}
		
		albedo = mix(albedo, snowColor, smoothstep(snowSlope, snowSlope+0.02, slope));
		metallic = mix(metallic, -1.0, smoothstep(snowSlope, snowSlope+0.02, slope));
		roughness = mix(roughness, 1.0-clamp(snowGlitter,0,1), smoothstep(snowSlope, snowSlope+0.02, slope));
		normal = normalize(mix(f_normal, f_normal + normalNoise, 0.2));
		emission = mix(emission, emission/3, smoothstep(snowSlope, snowSlope+0.02, slope));
	}
	
	// May discard fragment here...
	
	// albedo = vec3(FastSimplexFractal(vec3(normalize(posOnPlanet)*8000000.0/1000000.0), 10)*15000.0);
	// albedo += vec3(FastSimplexFractal(vec3(normalize(posOnPlanet)*8000000.0/30000.0), 8)*4000.0);
	// albedo += vec3(FastSimplexFractal(vec3(normalize(posOnPlanet)*8000000.0/50.0), 7)*4.0);
	// albedo += vec3(FastSimplexFractal(vec3(normalize(posOnPlanet)*8000000.0/6.0), 3)*0.5);
	// albedo += 10000;
	// roughness = 1;
	// metallic = 0;
	// normal = vec3(0,0,1);
	
	GBuffers gBuffers;
	gBuffers.albedo = albedo;
	gBuffers.alpha = 1.0;
	gBuffers.normal = ViewSpaceNormal(normalize(normal));
	gBuffers.roughness = roughness;
	gBuffers.metallic = metallic;
	gBuffers.emission = emission;
	gBuffers.position = f_viewPos;
	gBuffers.dist = f_trueDistance;
	WriteGBuffers(gBuffers);
}
