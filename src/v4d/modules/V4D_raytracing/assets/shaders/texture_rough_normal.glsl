#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"
#include "noise.glsl"

#define NormalFromBumpNoise(noiseFunc) \
	vec3 _tangentX = normalize(cross(normalize(vec3(0.356,1.2145,0.24537))/* fixed arbitrary vector in object space */, tex.normal.xyz));\
	vec3 _tangentY = normalize(cross(tex.normal.xyz, _tangentX));\
	mat3 _TBN = mat3(_tangentX, _tangentY, tex.normal.xyz);\
	float _altitudeTop = noiseFunc(tex.materialPayload.rayPayload.position.xyz + _tangentY/1000);\
	float _altitudeBottom = noiseFunc(tex.materialPayload.rayPayload.position.xyz - _tangentY/1000);\
	float _altitudeRight = noiseFunc(tex.materialPayload.rayPayload.position.xyz + _tangentX/1000);\
	float _altitudeLeft = noiseFunc(tex.materialPayload.rayPayload.position.xyz - _tangentX/1000);\
	vec3 _bump = normalize(vec3((_altitudeRight-_altitudeLeft), (_altitudeBottom-_altitudeTop), 2));\
	vec3 normal = normalize(_TBN * _bump)

#define MixTexNormal(val) tex.normal.xyz = normalize(mix(tex.normal.xyz, val, tex.factor))
#define MixTex(what, val) what = mix(what, val, tex.factor)

layout(location = CALL_DATA_LOCATION_TEXTURE) callableDataInEXT ProceduralTextureCall tex;

#######################################################################################

#shader rcall
float BumpyNoise(vec3 pos) {
	return clamp01(Simplex(pos*10000*tex.roughness)/2+.5);
}
void main() {
	// if (camera.accumulateFrames >= 0) {
		NormalFromBumpNoise(BumpyNoise);
		tex.factor = tex.roughness;
		MixTexNormal(normal);
	// }
}
