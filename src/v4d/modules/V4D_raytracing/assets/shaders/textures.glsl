#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"
#include "noise.glsl"

#define NormalFromBumpNoise(noiseFunc) \
	vec3 _tangentX = normalize(cross(normalize(vec3(0.356,1.2145,0.24537))/* fixed arbitrary vector in object space */, tex.normal));\
	vec3 _tangentY = normalize(cross(tex.normal, _tangentX));\
	mat3 _TBN = mat3(_tangentX, _tangentY, tex.normal);\
	float _altitudeTop = noiseFunc(tex.localHitPosition + _tangentY/1000);\
	float _altitudeBottom = noiseFunc(tex.localHitPosition - _tangentY/1000);\
	float _altitudeRight = noiseFunc(tex.localHitPosition + _tangentX/1000);\
	float _altitudeLeft = noiseFunc(tex.localHitPosition - _tangentX/1000);\
	vec3 _bump = normalize(vec3((_altitudeRight-_altitudeLeft), (_altitudeBottom-_altitudeTop), 2));\
	vec3 normal = normalize(_TBN * _bump)

struct ProceduralTextureCall {
	vec3 albedo;
	vec3 normal;
	float metallic;
	float roughness;
	float opacity;
	// input-only
	vec3 localHitPosition;
	float distance;
	float factor;
};

layout(location = 0) callableDataInEXT ProceduralTextureCall tex;

#######################################################################################3

#shader tex_noisy.rcall
void main() {
	float noise = clamp01(FastSimplexFractal(tex.localHitPosition*1000, 3)/2+.5);
	tex.roughness = mix(tex.roughness, noise, tex.factor/2);
	tex.metallic = mix(tex.metallic, 1-noise, tex.factor/2);
}

#shader tex_grainy.rcall
float GrainyNoise(vec3 pos) {
	return clamp01(FastSimplexFractal(pos*500, 2)/2+.5);
}
void main() {
	NormalFromBumpNoise(GrainyNoise);
	tex.normal = mix(tex.normal, normal, tex.factor/2);
	tex.roughness = mix(tex.roughness, 0.6, tex.factor);
	tex.metallic = mix(tex.metallic, 0.5, tex.factor);
}

#shader tex_bumped.rcall
float BumpyNoise(vec3 pos) {
	return clamp01(FastSimplex(pos*4)/2+.5);
}
void main() {
	NormalFromBumpNoise(BumpyNoise);
	tex.normal = mix(tex.normal, normal, tex.factor/2);
}

#shader tex_brushed.rcall
float BrushedNoise(vec3 pos) {
	return clamp01(FastSimplex(pos*vec3(20, 3000, 10))/2+.5);
}
void main() {
	NormalFromBumpNoise(BrushedNoise);
	tex.normal = mix(tex.normal, normal, tex.factor/2);
	tex.roughness = mix(tex.roughness, 0.3, tex.factor);
	tex.metallic = mix(tex.metallic, 0.7, tex.factor);
}

#shader tex_hammered.rcall
float HammeredNoise(vec3 pos) {
	return 1-pow(clamp01(voronoi3d(pos*20).x), 2);
}
void main() {
	NormalFromBumpNoise(HammeredNoise);
	tex.normal = mix(tex.normal, normal, tex.factor);
	float hammer = HammeredNoise(tex.localHitPosition);
	tex.albedo = mix(tex.albedo, tex.albedo/3, tex.factor);
	tex.roughness = mix(tex.roughness, mix(0.3, 0.8, hammer), tex.factor);
	tex.metallic = mix(tex.metallic, mix(0.3, 0.1, hammer), tex.factor);
}

#shader tex_polished.rcall
void main() {
	tex.metallic = mix(tex.metallic, 1.0, tex.factor);
	tex.roughness = mix(tex.roughness, 0.01, tex.factor);
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

