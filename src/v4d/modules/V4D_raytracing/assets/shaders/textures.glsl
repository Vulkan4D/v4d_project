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

#shader tex_noisy.rcall
void main() {
	float noise = clamp01(FastSimplexFractal(tex.materialPayload.rayPayload.position.xyz*1000, 3)/2+.5);
	
	MixTex(tex.roughness, noise/2);
	MixTex(tex.metallic, (1-noise)/2);
}

#shader tex_grainy.rcall
float GrainyNoise(vec3 pos) {
	return clamp01(FastSimplexFractal(pos*500, 2)/2+.5);
}
void main() {
	NormalFromBumpNoise(GrainyNoise);
	
	MixTexNormal(normal/2);
	MixTex(tex.roughness, 0.6);
	MixTex(tex.metallic, 0.5);
}

#shader tex_bumped.rcall
float BumpyNoise(vec3 pos) {
	return clamp01(FastSimplex(pos*4)/2+.5);
}
void main() {
	NormalFromBumpNoise(BumpyNoise);
	
	MixTexNormal(normal/2);
}

#shader tex_brushed.rcall
float BrushedNoise(vec3 pos) {
	return clamp01(FastSimplex(pos*vec3(20, 3000, 10))/2+.5);
}
void main() {
	NormalFromBumpNoise(BrushedNoise);
	
	MixTexNormal(normal/2);
	MixTex(tex.roughness, 0.3);
	MixTex(tex.metallic, 0.7);
}

#shader tex_hammered.rcall
float HammeredNoise(vec3 pos) {
	return 1-pow(clamp01(voronoi3d(pos*20).x), 2);
}
void main() {
	float hammer = HammeredNoise(tex.materialPayload.rayPayload.position.xyz);
	float roughness = mix(0.3, 0.8, hammer);
	float metallic = mix(0.3, 0.1, hammer);
	
	NormalFromBumpNoise(HammeredNoise);
	
	MixTexNormal(normal);
	MixTex(tex.albedo.rgb, tex.albedo.rgb/3);
	MixTex(tex.roughness, roughness);
	MixTex(tex.metallic, metallic);
}

#shader tex_polished.rcall
void main() {
	MixTex(tex.metallic, 1.0);
	MixTex(tex.roughness, 0.01);
}

#shader tex_galvanized.rcall
void main(){
	//TODO
}

#shader tex_perforated.rcall
void main(){
	//TODO
}

#shader tex_diamond.rcall
void main(){
	//TODO
}

#shader tex_carbon_fiber.rcall
void main(){
	//TODO
}

#shader tex_ceramic.rcall
void main(){
	//TODO
}

#shader tex_oxydation_iron.rcall
void main(){
	//TODO
}

#shader tex_oxydation_copper.rcall
void main(){
	//TODO
}

#shader tex_oxydation_aluminum.rcall
void main(){
	//TODO
}

#shader tex_oxydation_silver.rcall
void main(){
	//TODO
}

#shader tex_scratches_metal.rcall
void main(){
	//TODO
}

#shader tex_scratches_plastic.rcall
void main(){
	//TODO
}

#shader tex_scratches_glass.rcall
void main(){
	//TODO
}

#shader tex_cracked_rubber.rcall
void main(){
	//TODO
}

#shader tex_cracked_ceramic.rcall
void main(){
	//TODO
}

