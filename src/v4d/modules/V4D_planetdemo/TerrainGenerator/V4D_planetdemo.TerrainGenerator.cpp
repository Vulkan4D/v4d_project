#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_INTRINSICS
#define GLM_FORCE_SIMD_AVX2
#define GLM_FORCE_CXX17
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/texture.hpp>
using namespace glm;

#pragma region noise functions

dvec4 mod(const dvec4& x, double f) {return x - f * floor(x/f);}
vec4 mod(const vec4& x, float f) {return x - f * floor(x/f);}
vec4 _permute(const vec4& x){return mod(((x*34.0f)+1.0f)*x, 289.0f);} // used for Simplex noise
dvec4 _permute(const dvec4& x){return mod(((x*34.0)+1.0)*x, 289.0);}
// Returns a well-distributed range between -0.5 and +0.5
vec3 Noise3(const vec3& pos) { // used for FastSimplex
	float j = 4096.0f*sin(dot(pos,vec3(17.0, 59.4, 15.0)));
	vec3 r;
	r.z = fract(512.0f*j);
	j *= .125f;
	r.x = fract(512.0f*j);
	j *= .125f;
	r.y = fract(512.0f*j);
	return r-0.5f;
}
dvec3 Noise3(const dvec3& pos) { // used for FastSimplex
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

// Very quick pseudo-random generator, suitable for pos range (-100k, +100k) with a step of at least 0.01
// Returns a uniformly distributed float value between 0.00 and 1.00
float QuickNoise(float pos){
	return fract(sin(pos) * 13159.52714f);
}
double QuickNoise(double pos){
	return fract(sin(pos) * 13159.527140783267);
}
float QuickNoise(const vec2& pos) {
	return fract(sin(dot(pos, vec2(13.657f,9.558f))) * 24097.524f);
}
float QuickNoise(const vec3& pos) {
	return fract(sin(dot(pos, vec3(13.657f,9.558f,11.606f))) * 24097.524f);
}
double QuickNoise(const dvec3& pos) {
	return fract(sin(dot(pos, dvec3(13.657023817,9.5580981772,11.606065918))) * 24097.5240569198);
}
double QuickNoise(const dvec2& pos) {
	return fract(sin(dot(pos, dvec2(13.657023817,9.5580981772))) * 24097.5240569198);
}
// Returns a uniformly distributed integer value between 0 and n-1, suitable for pseudo-random enum value where n is the number of elements
int QuickNoiseIntegral(const vec2& pos, int n) {
	return (int)floor(QuickNoise(pos)*n);
}
int QuickNoiseIntegral(const vec3& pos, int n) {
	return (int)floor(QuickNoise(pos)*n);
}
int QuickNoiseIntegral(const dvec2& pos, int n) {
	return (int)floor(QuickNoise(pos)*n);
}
int QuickNoiseIntegral(const dvec3& pos, int n) {
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
float Noise(const vec2& pos) {
	const vec2 d = vec2(0.0, 1.0);
	vec2 b = floor(pos), f = smoothstep(vec2(0.0), vec2(1.0), fract(pos));
	return mix(mix(QuickNoise(b), QuickNoise(b + vec2(d.y, d.x)), f.x), mix(QuickNoise(b + vec2(d.x, d.y)), QuickNoise(b + vec2(d.y)), f.x), f.y);
}
double Noise(const dvec2& pos) {
	const dvec2 d = dvec2(0.0, 1.0);
	dvec2 b = floor(pos), f = smoothstep(dvec2(0.0), dvec2(1.0), fract(pos));
	return mix(mix(QuickNoise(b), QuickNoise(b + dvec2(d.y, d.x)), f.x), mix(QuickNoise(b + dvec2(d.x, d.y)), QuickNoise(b + dvec2(d.y)), f.x), f.y);
}
double Noise(const dvec3& pos3) {
	dvec2 pos = dvec2(pos3) + pos3.z;
	const dvec2 d = dvec2(0.0, 1.0);
	dvec2 b = floor(pos), f = smoothstep(dvec2(0.0), dvec2(1.0), fract(pos));
	return mix(mix(QuickNoise(b), QuickNoise(b + dvec2(d.y, d.x)), f.x), mix(QuickNoise(b + dvec2(d.x, d.y)), QuickNoise(b + dvec2(d.y)), f.x), f.y);
}

// Faster Simplex noise, less precise and not well tested, seems suitable for pos ranges (-10k, +10k) with a step of 0.01 and gradient of 0.5
// Returns a float value between -1.00 and +1.00 with a distribution that strongly tends towards the center (0.5)
double FastSimplex(const dvec3& pos) {
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



double FastSimplexFractal(const dvec3& pos, int octaves = 1, double lacunarity = 2.0, double gain = 0.5, std::function<double(double)> func = [](double x){return x;}) {
	double amplitude = 0.53333333333333333333333;
	double frequency = 1.0;
	double f = func(FastSimplex(pos * frequency));
	for (int i = 1; i < octaves; ++i) {
		frequency *= lacunarity;
		amplitude *= gain;
		f += amplitude * func(FastSimplex(pos * frequency));
	}
	return f;
}
double FastSimplexFractal(const dvec3& pos, int octaves, std::function<double(double)> func) {
	return FastSimplexFractal(pos, octaves, 2.0, 0.5, func);
}

dvec4 noised(const dvec3& x) {
	dvec3 p = floor(x);
	dvec3 w = fract(x);

	dvec3 u = w*w*w*(w*(w*6.0-15.0)+10.0);
	dvec3 du = 30.0*w*w*(w*(w-2.0)+1.0);

	double a = QuickNoise( p+dvec3(0,0,0) );
	double b = QuickNoise( p+dvec3(1,0,0) );
	double c = QuickNoise( p+dvec3(0,1,0) );
	double d = QuickNoise( p+dvec3(1,1,0) );
	double e = QuickNoise( p+dvec3(0,0,1) );
	double f = QuickNoise( p+dvec3(1,0,1) );
	double g = QuickNoise( p+dvec3(0,1,1) );
	double h = QuickNoise( p+dvec3(1,1,1) );

	double k0 =   a;
	double k1 =   b - a;
	double k2 =   c - a;
	double k3 =   e - a;
	double k4 =   a - b - c + d;
	double k5 =   a - c - e + g;
	double k6 =   a - b - e + f;
	double k7 = - a + b + c - d + e - f - g + h;

	return dvec4( -1.0+2.0*(k0 + k1*u.x + k2*u.y + k3*u.z + k4*u.x*u.y + k5*u.y*u.z + k6*u.z*u.x + k7*u.x*u.y*u.z),
				2.0* du * dvec3( k1 + k4*u.y + k6*u.z + k7*u.y*u.z,
								k2 + k5*u.z + k4*u.x + k7*u.z*u.x,
								k3 + k6*u.x + k5*u.y + k7*u.x*u.y ) );
}

// dvec4 fbm( dvec3 x, int octaves ) {
// 	const dmat3 m3  = dmat3( 0.00,  0.80,  0.60,
// 						-0.80,  0.36, -0.48,
// 						-0.60, -0.48,  0.64 );
// 	const dmat3 m3i = dmat3( 0.00, -0.80, -0.60,
// 						0.80,  0.36, -0.48,
// 						0.60, -0.48,  0.64 );
						
// 	double f = 1.98;  // could be 2.0
// 	double s = 0.49;  // could be 0.5
// 	double a = 0.0;
// 	double b = 0.5;
// 	dvec3  d = dvec3(0.0);
// 	dmat3  m = dmat3(1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0);
// 	for( int i=0; i < octaves; i++ )
// 	{
// 		dvec4 n = noised(x);
// 		a += b*n.x;          // accumulate values
// 		d += b*m*dvec3(n.y,n.z,n.w);      // accumulate derivatives
// 		b *= s;
// 		x = f*m3*x;
// 		m = f*m3i*m;
// 	}
// 	return dvec4( a, d );
// }

double terrain( const dvec3& p, int octaves) {
	const dmat3 m  = dmat3( 0.00,  0.80,  0.60,
						-0.80,  0.36, -0.48,
						-0.60, -0.48,  0.64 );
	double a = 0.0;
	double b = 1.0;
	dvec3  d = dvec3(0.0);
	dvec3 pp = p;
	for( int i=0; i<octaves; i++ ) {
		dvec4 n=noised(pp);
		d += dvec3(n.y,n.z,n.w);
		a += b*n.x/(1.0+dot(d,d));
		b *= 0.5;
		pp = m*pp*2.0;
	}
	return a;
}
#pragma endregion

double solidRadius = 8000000;
double heightVariation = 10000;

/* // Tectonic plates
// double avgPlateDiameter = 2000000;
// const int plateDistributionMaxRetries = 1000;
// // const int nbClosestPlates = 4;
// const double plateDistributionRetryDistanceThreshold = avgPlateDiameter/solidRadius * 0.5; // 0.75 is very homogeneous and can fit all
// const dvec3 referenceArbitraryVector = normalize(dvec3{0.01,0.01,1.01});
// const double EPSILON = 0.00001;

// double surfaceArea = 4.0*pi<double>()*solidRadius*solidRadius;
// double avgPlateArea = avgPlateDiameter*avgPlateDiameter;
// const int nbPlates = max(5, (int)round(surfaceArea/avgPlateArea));
// // const double plateSizeFactor = sqrt(float(nbPlates))/pi<double>();

// // struct PlateInteraction {
// // 	int index;
// // 	double convergence;
// // };
// struct Plate {
// 	dvec3 pos;
// 	dvec3 tan {0};
// 	dvec3 bitan {0};
// 	// dvec3 tectonicDir {0};
// 	// PlateInteraction closestPlates[nbClosestPlates];
// 	// int convergingNeigbour = -1;
// 	Plate(dvec3 pos) : pos(pos) {
// 		tan = normalize(cross(referenceArbitraryVector, pos));
// 		bitan = normalize(cross(pos, tan));
// 		tan = normalize(cross(bitan, pos));
		
// 		// tectonicDir = normalize(mix(mix(tan,-tan, (double)QuickNoiseIntegral(pos, 2)), mix(bitan, -bitan, (double)QuickNoiseIntegral(pos+tan, 2)), QuickNoise(pos+bitan)));
// 	}
// };

// std::vector<Plate> plates {};

// struct RelativePlate {
// 	int index;
// 	double distance;
// };

// inline RelativePlate GetClosestPlate(dvec3& normalizedPos) {
// 	RelativePlate plate {-1, 1e100};
// 	for (int i = 0; i < plates.size(); ++i) {
// 		auto dist = distance(normalizedPos, plates[i].pos);
// 		if (dist < plate.distance) plate = {i, dist};
// 	}
// 	return plate;
// }

// inline std::tuple<RelativePlate,RelativePlate> GetTwoClosestPlates(dvec3& normalizedPos) {
// 	RelativePlate first {-1, 1e100};
// 	RelativePlate second {-1, 1e100};
// 	for (int i = 0; i < plates.size(); ++i) {
		
// 		// double dist = distance(normalizedPos, plates[i].pos);
// 		dvec3 diff = normalizedPos - plates[i].pos;
// 		double dist = dot(diff, diff);
		
// 		if (dist < first.distance) {
// 			second = first;
// 			first = {i, dist};
// 		} else if (dist < second.distance && i != first.index) {
// 			second = {i, dist};
// 		}
// 	}
// 	return {first, second};
// }

// inline std::tuple<RelativePlate,RelativePlate,RelativePlate> GetThreeClosestPlates(dvec3& normalizedPos) {
// 	RelativePlate first {-1, 1e100};
// 	RelativePlate second {-1, 1e100};
// 	RelativePlate third {-1, 1e100};
// 	for (int i = 0; i < plates.size(); ++i) {
		
// 		dvec3 diff = normalizedPos - plates[i].pos;
// 		double dist = dot(diff, diff);
		
// 		if (dist < first.distance) {
// 			third = second;
// 			second = first;
// 			first = {i, dist};
// 		} else if (dist < second.distance && i != first.index) {
// 			third = second;
// 			second = {i, dist};
// 		} else if (dist < third.distance && i != first.index && i != second.index) {
// 			third = {i, dist};
// 		}
// 	}
// 	return {first, second, third};
// }
*/


extern "C" {
	void Init() {
		
		
		/*// // Tectonic plates
		// if (plates.size() == 0) {
		// 	plates.reserve(nbPlates);
			
		// 	// Position each plate
		// 	for (int i = 0; i < nbPlates; ++i) {
		// 		dvec3 platePos {};
		// 		int retries = 0;
		// 		for (;;++retries) {
		// 			platePos = normalize(dvec3{
		// 				QuickNoise(128.54+i*2.45+retries*1.68),
		// 				QuickNoise(982.1+i*1.87+retries*3.1),
		// 				QuickNoise(34.9+i*7.22+retries*7.34),
		// 			}*2.0-1.0);
		// 			if (retries > plateDistributionMaxRetries) {
		// 				std::cout << "plating not optimal" << std::endl;
		// 				break;
		// 			}
		// 			if (std::find_if(plates.begin(), plates.end(), [&platePos](auto& p)->bool{
		// 				return distance(p.pos, platePos) < plateDistributionRetryDistanceThreshold;
		// 			}) == plates.end()) break;
		// 		};
		// 		plates.emplace_back(platePos);
		// 	}
		
		// 	// // Set closest plates to each plate
		// 	// std::vector<RelativePlate> closestPlates {};
		// 	// closestPlates.resize(nbPlates);
		// 	// for (int i = 0; i < plates.size(); ++i) {
		// 	// 	for (int y = 0; y < plates.size();++y) {
		// 	// 		closestPlates[y] = {y, distance(plates[i].pos, plates[y].pos)};
		// 	// 	}
		// 	// 	std::sort(closestPlates.begin(), closestPlates.end(), [](RelativePlate& p1, RelativePlate& p2)->bool{return p1.distance < p2.distance;});
		// 	// 	for (int ci = 0; ci < nbClosestPlates; ++ci) {
		// 	// 		int neighbourIndex = closestPlates[ci+1].index;
		// 	// 		plates[i].closestPlates[ci].index = neighbourIndex;
		// 	// 		plates[i].closestPlates[ci].convergence = -dot(plates[i].tectonicDir,plates[neighbourIndex].tectonicDir);
		// 	// 		if (plates[i].closestPlates[ci].convergence > 0.0 && plates[i].convergingNeigbour == -1 && plates[neighbourIndex].convergingNeigbour == -1) {
		// 	// 			plates[i].convergingNeigbour = neighbourIndex;
		// 	// 			plates[neighbourIndex].convergingNeigbour = i;
		// 	// 		}
		// 	// 	}
		// 	// }
		// }
		*/
	}

	/*double GetHeightMap(dvec3* const pos) {
		double baseHeight = 0; // expected to be a value between 0.0 and 1.0
		double detail = 0; // expected to be a value between ~ -10 to +10 km
		dvec3& normalizedPos = *pos;
		
		// dvec3 normalizedPosDistorted = normalizedPos;
		// dvec3 tan = normalize(cross(referenceArbitraryVector, normalizedPos));
		// dvec3 bitan = normalize(cross(normalizedPos, tan));
		// tan = normalize(cross(bitan, normalizedPos));
		// {
		// 	const int nOctaves = 4;
		// 	double nTan = FastSimplexFractal(normalizedPos*6.2, nOctaves);
		// 	double nBitan = FastSimplexFractal(normalizedPos*5.9, nOctaves);
		// 	normalizedPosDistorted = normalize(normalizedPosDistorted + (tan*nTan + bitan*nBitan)/20.0);
		// }
		
		// auto[closestPlate, secondClosestPlate, thirdClosestPlate] = GetThreeClosestPlates(normalizedPosDistorted);
		// double platePower = (secondClosestPlate.distance - closestPlate.distance) * float(nbPlates)/16.0; // ranges from 0.0 to ~ 1.0, absolutely starts at 0.0, avg about 0.3, may go up to ~0.6 or sometimes a little over 1.0
		
		
		// // // nice medium terrain with derivatives
		// detail += terrain(normalizedPosDistorted*solidRadius/4000.0, 12)*200.0 + 200.0;
	
		
		// // double mountains = 0;
		// // mountains += -abs(FastSimplexFractal(normalizedPosDistorted*solidRadius/100000.0, 4, 2.0, 0.5, [](double x){return (x);}));
		// // mountains += FastSimplexFractal(normalizedPos*solidRadius/10000.0, 6, 4.2, 0.3, [](double x){return -abs(x);})/4.0;
		// // baseHeight = platePower*platePower * (mountains/2.0+0.5);
		
		
		// baseHeight = pow((closestPlate.distance + secondClosestPlate.distance + thirdClosestPlate.distance)*3.1415926536, 6);
		
		double mountainsStrength = max(0.0, FastSimplexFractal(normalizedPos*solidRadius/100000.0, 2));
		double mountains = 0;
		if (mountainsStrength > 0.0) {
			mountains += mountainsStrength * max(0.0, FastSimplexFractal(normalizedPos*solidRadius/100000.0, 2)*heightVariation);
			mountains += mountainsStrength * abs(FastSimplexFractal(normalizedPos*solidRadius/10000.0, 8, 2.4, 0.4)*heightVariation/2.0);
		}
		
		return
			+ mountains
			+ (FastSimplexFractal(normalizedPos*solidRadius/1.0, 2))*0.06
		;
		
		
		// convert baseHeight from 0.0_+1.0 to -heightVariation_+heightVariation
		return (baseHeight*2.0-1.0)*heightVariation*1.0 + detail;
	}*/
	
	
	double GetHeightMap(dvec3* const pos) {
		dvec3& normalizedPos = *pos;
		double res = 0;
		
		double biome = FastSimplexFractal(normalizedPos*solidRadius/1000000.0, 5);
		double biome1 = glm::max(0.0, biome);
		double biome2 = glm::max(0.0, -biome);
		
		double groundDetail = (FastSimplexFractal(normalizedPos*solidRadius*2.0, 2))*0.04;
		
		
		if (biome1 > 0) {
			double mountainsStrength = max(0.0, FastSimplexFractal(normalizedPos*solidRadius/100000.0, 2));
			double mountains = 0;
			if (mountainsStrength > 0.0) {
				mountains += mountainsStrength * max(0.0, FastSimplexFractal(normalizedPos*solidRadius/100000.0, 2)*heightVariation);
				mountains += mountainsStrength * abs(FastSimplexFractal(normalizedPos*solidRadius/10000.0, 8, 2.4, 0.4)*heightVariation);
			}
			res += biome1 * (mountains);
		}
		
		if (biome2 > 0) {
			double peaks = FastSimplexFractal(normalizedPos*solidRadius/70000.0, 10, 2.2, 0.4, [](double d){return 1.0-glm::abs(d);}) * 20000;
			res += biome2 * (peaks);
		}
		
		res += groundDetail;
		
		res += terrain(normalizedPos*solidRadius/4000.0, 10)*200.0;
		
		return res;
	}
	
	
	vec3 GetColor(double heightMap) {
		
		
		// if (heightMap >= heightVariation) return {1,1,0}; // yellow when too high
		// if (heightMap <= -heightVariation) return {1,0,0}; // red when too low
		// if (heightMap < -heightVariation+1.0) return {1,0,1}; // pink when below one meter from bottom altitude
		// if (heightMap == 0.0) return {0,1,0}; // green when zero
		// if (heightMap > -10.0 && heightMap < 10.0) return {0,0,1}; // blue when near zero
		// if (heightMap > heightVariation*0.5) return {0,1,1}; // turquoise when 75% from top altitude
		// // convert heightMap from -heightVariation_+heightVariation to 0.0_+1.0
		// return vec3(heightMap/heightVariation / 2.0 + 0.5); // grayscale for between -heightVariation and +heightVariation
		
		
		
		// return {0.5f, 0.4f, 0.3f};
		
		return {0.4f, 0.15f, 0.03f};
	}
}
