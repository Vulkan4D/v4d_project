#include <iostream>

// GLM
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/texture.hpp>

using namespace glm;

#pragma region helpers functions
dvec4 mod(dvec4 x, double f) {return x - f * floor(x/f);}
vec4 mod(vec4 x, float f) {return x - f * floor(x/f);}
vec4 _permute(vec4 x){return mod(((x*34.0f)+1.0f)*x, 289.0f);} // used for Simplex noise
dvec4 _permute(dvec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
// Returns a well-distributed range between -0.5 and +0.5
vec3 Noise3(vec3 pos) { // used for FastSimplex
	float j = 4096.0f*sin(dot(pos,vec3(17.0, 59.4, 15.0)));
	vec3 r;
	r.z = fract(512.0f*j);
	j *= .125f;
	r.x = fract(512.0f*j);
	j *= .125f;
	r.y = fract(512.0f*j);
	return r-0.5f;
}
dvec3 Noise3(dvec3 pos) { // used for FastSimplex
	double j = 4096.0*sin(dot(pos,dvec3(17.0, 59.4, 15.0)));
	dvec3 r;
	r.z = fract(512.0*j);
	j *= .125;
	r.x = fract(512.0*j);
	j *= .125;
	r.y = fract(512.0*j);
	return r-0.5;
}
// float fbm(vec3 x, int octaves) {
// 	float v = 0.0;
// 	float a = 0.5;
// 	vec3 shift = vec3(100);
// 	for (int i = 0; i < octaves; ++i) {
// 		v += a * noise(x);
// 		x = x * 2.0 + shift;
// 		a *= 0.5;
// 	}
// 	return v;
// }
#pragma endregion

// Faster Simplex noise, less precise and not well tested, seems suitable for pos ranges (-10k, +10k) with a step of 0.01 and gradient of 0.5
// Returns a float value between -1.00 and +1.00 with a distribution that strongly tends towards the center (0.5)
float FastSimplex(vec3 pos) {
	const float F3 = 0.3333333f;
	const float G3 = 0.1666667f;

	vec3 s = floor(pos + dot(pos, vec3(F3)));
	vec3 x = pos - s + dot(s, vec3(G3));

	vec3 e = step(vec3(0.0f), x - vec3(x.y, x.z, x.x));
	vec3 i1 = e * (1.0f - vec3(e.z, e.x, e.y));
	vec3 i2 = 1.0f - vec3(e.z, e.x, e.y) * (1.0f - e);

	vec3 x1 = x - i1 + G3;
	vec3 x2 = x - i2 + 2.0f * G3;
	vec3 x3 = x - 1.0f + 3.0f * G3;

	vec4 w, d;

	w.x = dot(x, x);
	w.y = dot(x1, x1);
	w.z = dot(x2, x2);
	w.w = dot(x3, x3);

	w = max(0.6f - w, 0.0f);

	d.x = dot(Noise3(s), x);
	d.y = dot(Noise3(s + i1), x1);
	d.z = dot(Noise3(s + i2), x2);
	d.w = dot(Noise3(s + 1.0f), x3);

	w *= w;
	w *= w;
	d *= w;

	return dot(d, vec4(52.0f));
}
double FastSimplex(dvec3 pos) {
	const double F3 = 0.33333333333333333;
	const double G3 = 0.16666666666666667;

	dvec3 s = floor(pos + dot(pos, dvec3(F3)));
	dvec3 x = pos - s + dot(s, dvec3(G3));

	dvec3 e = step(dvec3(0.0), x - dvec3(x.y, x.z, x.x));
	dvec3 i1 = e * (1.0 - dvec3(e.z, e.x, e.y));
	dvec3 i2 = 1.0 - dvec3(e.z, e.x, e.y) * (1.0 - e);

	dvec3 x1 = x - i1 + G3;
	dvec3 x2 = x - i2 + 2.0 * G3;
	dvec3 x3 = x - 1.0 + 3.0 * G3;

	dvec4 w, d;

	w.x = dot(x, x);
	w.y = dot(x1, x1);
	w.z = dot(x2, x2);
	w.w = dot(x3, x3);

	w = max(0.6 - w, 0.0);

	d.x = dot(Noise3(s), x);
	d.y = dot(Noise3(s + i1), x1);
	d.z = dot(Noise3(s + i2), x2);
	d.w = dot(Noise3(s + 1.0), x3);

	w *= w;
	w *= w;
	d *= w;

	return dot(d, dvec4(52.0));
}
float FastSimplexFractal(vec3 m) {
	return 0.5333333f * FastSimplex(m)
			+0.2666667f * FastSimplex(2.0f*m)
			+0.1333333f * FastSimplex(4.0f*m)
			+0.0666667f * FastSimplex(8.0f*m);
}
double FastSimplexFractal(dvec3 m) {
	return 0.5333333* FastSimplex(m)
			+0.2666667* FastSimplex(2.0*m)
			+0.1333333* FastSimplex(4.0*m)
			+0.0666667* FastSimplex(8.0*m);
}
float FastSimplexFractal(vec3 pos, int octaves) {
	float amplitude = 0.5333333333333333f;
	float frequency = 1.0;
	float f = FastSimplex(pos * frequency);
	for (int i = 1; i < octaves; ++i) {
		amplitude /= 2.0f;
		frequency *= 2.0f;
		f += amplitude * FastSimplex(pos * frequency);
	}
	return f;
}
double FastSimplexFractal(dvec3 pos, int octaves) {
	double amplitude = 0.5333333333333333;
	double frequency = 1.0;
	double f = FastSimplex(pos * frequency);
	for (int i = 1; i < octaves; ++i) {
		amplitude /= 2.0;
		frequency *= 2.0;
		f += amplitude * FastSimplex(pos * frequency);
	}
	return f;
}

double solidRadius = 8000000;
double heightVariation = 10000;

extern "C" {
	void init() {
		std::cout << "TerrainGenerator dynamic library Init()" << std::endl;
	}
	double generateHeightMap(glm::dvec3* const pos) {
		double height = 0;
		
		// height += FastSimplexFractal(((*pos)*solidRadius/100000.0), 2)*heightVariation;
		// height += FastSimplexFractal(((*pos)*solidRadius/20000.0), 5)*heightVariation/10.0;
		
		// height += FastSimplexFractal(((*pos)*solidRadius/100.0), 3)*6.0;
		// height += FastSimplexFractal((*pos)*solidRadius/10.0, 2);
		// height += FastSimplexFractal((*pos)*solidRadius, 3)/5.0;
	
		return height;
	}
}

