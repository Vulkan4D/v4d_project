#pragma once

#include <v4d.h>

namespace v4d::noise {
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
	
	
	// Very quick pseudo-random generator, suitable for pos range (-100k, +100k) with a step of at least 0.01
	// Returns a uniformly distributed float value between 0.00 and 1.00
	float QuickNoise(float pos){
		return fract(sin(pos) * 13159.52714f);
	}
	double QuickNoise(double pos){
		return fract(sin(pos) * 13159.527140783267);
	}
	float QuickNoise(vec2 pos) {
		return fract(sin(dot(pos, vec2(13.657f,9.558f))) * 24097.524f);
	}
	float QuickNoise(vec3 pos) {
		return fract(sin(dot(pos, vec3(13.657f,9.558f,11.606f))) * 24097.524f);
	}
	double QuickNoise(dvec3 pos) {
		return fract(sin(dot(pos, dvec3(13.657023817,9.5580981772,11.606065918))) * 24097.5240569198);
	}
	double QuickNoise(dvec2 pos) {
		return fract(sin(dot(pos, dvec2(13.657023817,9.5580981772))) * 24097.5240569198);
	}
	// Returns a uniformly distributed integer value beterrn 0 and n-1, suitable for pseudo-random enum value where n is the number of elements
	int QuickNoiseIntegral(vec2 pos, int n) {
		return (int)floor(QuickNoise(pos)*n);
	}
	int QuickNoiseIntegral(vec3 pos, int n) {
		return (int)floor(QuickNoise(pos)*n);
	}
	int QuickNoiseIntegral(dvec2 pos, int n) {
		return (int)floor(QuickNoise(pos)*n);
	}
	int QuickNoiseIntegral(dvec3 pos, int n) {
		return (int)floor(QuickNoise(pos)*n);
	}

	
	// 1-dimentional fast noise, suitable for pos range (-1M, +1M) with a step of 0.1 and gradient of 0.5
	// Returns a float value between 0.00 and 1.00 with a somewhat uniform distribution
	float Noise(float pos) {
		float fl = floor(pos);
		float fc = fract(pos);
		return mix(QuickNoise(fl), QuickNoise(fl + 1.0f), fc);
	}
	double Noise(double pos) {
		double fl = floor(pos);
		double fc = fract(pos);
		return mix(QuickNoise(fl), QuickNoise(fl + 1.0), fc);
	}
	
	
	// Simple 2D noise suitable for pos range (-100k, +100k) with a step of 0.1 and gradient of 0.5
	// Returns a float value between 0.00 and 1.00 with a distribution that tends a little bit towards the center (0.5)
	float Noise(vec2 pos) {
		const vec2 d = vec2(0.0, 1.0);
		vec2 b = floor(pos), f = smoothstep(vec2(0.0), vec2(1.0), fract(pos));
		return mix(mix(QuickNoise(b), QuickNoise(b + vec2(d.y, d.x)), f.x), mix(QuickNoise(b + vec2(d.x, d.y)), QuickNoise(b + vec2(d.y)), f.x), f.y);
	}
	double Noise(dvec2 pos) {
		const dvec2 d = dvec2(0.0, 1.0);
		dvec2 b = floor(pos), f = smoothstep(dvec2(0.0), dvec2(1.0), fract(pos));
		return mix(mix(QuickNoise(b), QuickNoise(b + dvec2(d.y, d.x)), f.x), mix(QuickNoise(b + dvec2(d.x, d.y)), QuickNoise(b + dvec2(d.y)), f.x), f.y);
	}


	// simple-precision Simplex noise, suitable for pos range (-1M, +1M) with a step of 0.001 and gradient of 1.0
	// Returns a float value between -1.000 and +1.000 with a distribution that strongly tends towards the center (0.5)
	float Simplex(vec3 pos){
		const vec2 C = vec2(1.0/6.0, 1.0/3.0);
		const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

		vec3 i = floor(pos + dot(pos, vec3(C.y)));
		vec3 x0 = pos - i + dot(i, vec3(C.x));

		vec3 g = step(vec3(x0.y, x0.z, x0.x), x0);
		vec3 l = 1.0f - g;
		vec3 i1 = min( g, vec3(l.z, l.x, l.y));
		vec3 i2 = max( g, vec3(l.z, l.x, l.y));

		vec3 x1 = x0 - i1 + 1.0f * vec3(C.x);
		vec3 x2 = x0 - i2 + 2.0f * vec3(C.x);
		vec3 x3 = x0 - 1.0f + 3.0f * vec3(C.x);

		i = mod(i, 289.0f); 
		vec4 p = _permute(_permute(_permute(i.z + vec4(0.0, i1.z, i2.z, 1.0)) + i.y + vec4(0.0, i1.y, i2.y, 1.0)) + i.x + vec4(0.0, i1.x, i2.x, 1.0));

		float n_ = 1.0f/7.0f;
		vec3  ns = n_ * vec3(D.w, D.y, D.z) - vec3(D.x, D.z, D.x);

		vec4 j = p - 49.0f * floor(p * ns.z *ns.z);

		vec4 x_ = floor(j * ns.z);
		vec4 y_ = floor(j - 7.0f * x_);

		vec4 x = x_ * ns.x + vec4(ns.y);
		vec4 y = y_ * ns.x + vec4(ns.y);
		vec4 h = 1.0f - abs(x) - abs(y);

		vec4 b0 = vec4(x.x, x.y, y.x, y.y);
		vec4 b1 = vec4(x.z, x.w, y.z, y.w);

		vec4 s0 = floor(b0)*2.0f + 1.0f;
		vec4 s1 = floor(b1)*2.0f + 1.0f;
		vec4 sh = -step(h, vec4(0.0));

		vec4 a0 = vec4(b0.x,b0.z,b0.y,b0.w) + vec4(s0.x,s0.z,s0.y,s0.w) * vec4(sh.x,sh.x,sh.y,sh.y);
		vec4 a1 = vec4(b1.x,b1.z,b1.y,b1.w) + vec4(s1.x,s1.z,s1.y,s1.w) * vec4(sh.z,sh.z,sh.w,sh.w);

		vec3 p0 = vec3(a0.x, a0.y, h.x);
		vec3 p1 = vec3(a0.z, a0.w, h.y);
		vec3 p2 = vec3(a1.x, a1.y, h.z);
		vec3 p3 = vec3(a1.z, a1.w, h.w);

		vec4 norm = 1.79284291400159f - 0.85373472095314f * vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3));
		p0 *= norm.x;
		p1 *= norm.y;
		p2 *= norm.z;
		p3 *= norm.w;

		vec4 m = max(0.6f - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0f);
		return 42.0f * dot(m*m*m*m, vec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3)));
	}
	double Simplex(dvec3 pos){
		const dvec2 C = dvec2(1.0/6.0, 1.0/3.0);
		const dvec4 D = dvec4(0.0, 0.5, 1.0, 2.0);

		dvec3 i = floor(pos + dot(pos, dvec3(C.y)));
		dvec3 x0 = pos - i + dot(i, dvec3(C.x));

		dvec3 g = step(dvec3(x0.y, x0.z, x0.x), x0);
		dvec3 l = 1.0 - g;
		dvec3 i1 = min( g, dvec3(l.z, l.x, l.y));
		dvec3 i2 = max( g, dvec3(l.z, l.x, l.y));

		dvec3 x1 = x0 - i1 + 1.0 * dvec3(C.x);
		dvec3 x2 = x0 - i2 + 2.0 * dvec3(C.x);
		dvec3 x3 = x0 - 1.0 + 3.0 * dvec3(C.x);

		i = mod(i, 289.0); 
		dvec4 p = _permute(_permute(_permute(i.z + dvec4(0.0, i1.z, i2.z, 1.0)) + i.y + dvec4(0.0, i1.y, i2.y, 1.0)) + i.x + dvec4(0.0, i1.x, i2.x, 1.0));

		double n_ = 1.0/7.0;
		dvec3  ns = n_ * dvec3(D.w, D.y, D.z) - dvec3(D.x, D.z, D.x);

		dvec4 j = p - 49.0 * floor(p * ns.z *ns.z);

		dvec4 x_ = floor(j * ns.z);
		dvec4 y_ = floor(j - 7.0 * x_);

		dvec4 x = x_ * ns.x + dvec4(ns.y);
		dvec4 y = y_ * ns.x + dvec4(ns.y);
		dvec4 h = 1.0 - abs(x) - abs(y);

		dvec4 b0 = dvec4(x.x, x.y, y.x, y.y);
		dvec4 b1 = dvec4(x.z, x.w, y.z, y.w);

		dvec4 s0 = floor(b0)*2.0 + 1.0;
		dvec4 s1 = floor(b1)*2.0 + 1.0;
		dvec4 sh = -step(h, dvec4(0.0));

		dvec4 a0 = dvec4(b0.x,b0.z,b0.y,b0.w) + dvec4(s0.x,s0.z,s0.y,s0.w) * dvec4(sh.x,sh.x,sh.y,sh.y);
		dvec4 a1 = dvec4(b1.x,b1.z,b1.y,b1.w) + dvec4(s1.x,s1.z,s1.y,s1.w) * dvec4(sh.z,sh.z,sh.w,sh.w);

		dvec3 p0 = dvec3(a0.x, a0.y, h.x);
		dvec3 p1 = dvec3(a0.z, a0.w, h.y);
		dvec3 p2 = dvec3(a1.x, a1.y, h.z);
		dvec3 p3 = dvec3(a1.z, a1.w, h.w);

		dvec4 norm = 1.79284291400159 - 0.85373472095314 * dvec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3));
		p0 *= norm.x;
		p1 *= norm.y;
		p2 *= norm.z;
		p3 *= norm.w;

		dvec4 m = max(0.6 - dvec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
		return 42.0 * dot(m*m*m*m, dvec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3)));
	}
	
	float SimplexFractal(vec3 pos, int octaves) {
		if (octaves == 1) return Simplex(pos);
		float amplitude = 0.533333333333333f;
		float frequency = 1.0f;
		float f = Simplex(pos * frequency);
		for (int i = 1; i < octaves; ++i) {
			amplitude /= 2.0f;
			frequency *= 2.0f;
			f += amplitude * Simplex(pos * frequency);
		}
		return f;
	}
	double SimplexFractal(dvec3 pos, int octaves) {
		if (octaves == 1) return Simplex(pos);
		double amplitude = 0.533333333333333;
		double frequency = 1.0;
		double f = Simplex(pos * frequency);
		for (int i = 1; i < octaves; ++i) {
			amplitude /= 2.0;
			frequency *= 2.0;
			f += amplitude * Simplex(pos * frequency);
		}
		return f;
	}

	

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



	////////////////////////////////////////////////////
	// Universe density map
	
	// Get galaxy size factor from position in universe grid where 1.0 = 10MLY. 
	// Returns 0.0 (most likely) or somewhere between 0.01 and 0.999 inclusively (distributed a little more towards 0.1)
	float GalaxySizeFactorInUniverseGrid(vec3 pos) {
		float t = pow(1.0f - (
			  0.533333333333333f * abs(FastSimplex(pos * 0.5f))
			+ 0.266666666666667f * abs(FastSimplex(pos * 1.0f))
			+ 0.133333333333333f * abs(FastSimplex(pos * 2.0f))
			+ 0.066666666666667f * abs(FastSimplex(pos * 4.0f))
		) * 3.8f, 2.0f) * min(1.0f, FastSimplex(pos * 13.65f)/2.0f+0.8f);
		// Adjust distribution
		return max(0.0f, min(0.999f, (max(0.01f, pow(t, 1.2f)) * step(0.3f, t) - 0.3f - step(0.9f, t)) * 1.1f));
	}
	// Get universe subgrid size (number of divisions). 
	// Returns a value between 1 and 32 inclusively, distributed more towards the median (14 to 18 are the most likely, 1 and 32 are the least likely)
	int UniverseSubGridSize(vec3 pos) {
		return max(1, min(32, (int)floor((FastSimplex(pos * 0.5f) / 2.0f + 0.5f) * 43.0f - 5.0f)));
	}
	float UniverseSubGridWidthLY(int subGridSize) {
		return 10000000.0f / float(subGridSize);
	}
	float UniverseSubGridWidthLY(vec3 pos) {
		return UniverseSubGridWidthLY(UniverseSubGridSize(pos));
	}
	
	
	
	struct GalaxyInfo {
		float spiralCloudsFactor;
		float swirlTwist;
		float swirlDetail;
		float coreSize;
		float cloudsSize;
		float cloudsFrequency;
		float squish;
		float attenuationCloudsFrequency;
		float attenuationCloudsFactor;
		vec3 position;
		vec3 noiseOffset;
		float irregularities;
		mat4 rotation;
	};

	GalaxyInfo GetGalaxyInfo(vec3 galaxyPosition) {
		GalaxyInfo info;
		float type = QuickNoise(galaxyPosition / 10.0f);
		if (type < 0.2) {
			// Elliptical galaxy (20% probability)
			info.spiralCloudsFactor = 0.0;
			info.coreSize = QuickNoise(galaxyPosition);
			info.squish = QuickNoise(galaxyPosition+vec3(-0.33,-0.17,-0.51)) / 2.0f;
		} else {
			if (type > 0.3) {
				// Irregular galaxy (70% probability, within spiral)
				info.irregularities = QuickNoise(galaxyPosition+vec3(-0.65,0.69,-0.71));
			} else {
				info.irregularities = 0.0f;
			}
			// Spiral galaxy (80% probability, including irregular, only 10% will be regular)
			vec3 n1 = Noise3(galaxyPosition+vec3(0.01,0.43,-0.55)) / 2.0f + 0.5f;
			vec3 n2 = Noise3(galaxyPosition+vec3(-0.130,0.590,-0.550)) / 2.0f + 0.5f;
			vec3 n3 = Noise3(galaxyPosition+vec3(0.510,-0.310,0.512)) / 2.0f + 0.5f;
			info.spiralCloudsFactor = n1.x;
			info.swirlTwist = n1.y;
			info.swirlDetail = n1.z;
			info.coreSize = n2.x;
			info.cloudsSize = n2.y;
			info.cloudsFrequency = n2.z;
			info.squish = n3.x;
			info.attenuationCloudsFrequency = n3.y;
			info.attenuationCloudsFactor = n3.z;
			info.noiseOffset = Noise3(galaxyPosition);
		}
		if (info.spiralCloudsFactor > 0.0f || info.squish > 0.2f) {
			vec3 axis = normalize(Noise3(galaxyPosition+vec3(-0.212,0.864,0.892)));
			float angle = QuickNoise(galaxyPosition+vec3(0.176,0.917,1.337)) * 3.14159265459f;
			float s = sin(angle);
			float c = cos(angle);
			float oc = 1.0f - c;
			info.rotation = mat4(
				oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s, 0.0,
				oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s, 0.0,
				oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c, 0.0,
				0.0, 0.0, 0.0, 1.0
			);
		}
		return info;
	}

	
}
