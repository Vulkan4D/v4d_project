// helpers functions
vec4 _permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);} // used for Simplex
dvec4 _permute(dvec4 x){return mod(((x*34.0)+1.0)*x, 289.0);} // used for Simplex
vec3 Noise3(vec3 pos) { // used for FastSimplex
	float j = 4096.0*sin(dot(pos,vec3(17.0, 59.4, 15.0)));
	vec3 r;
	r.z = fract(512.0*j);
	j *= .125;
	r.x = fract(512.0*j);
	j *= .125;
	r.y = fract(512.0*j);
	return r-0.5;
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

// Very quick pseudo-random generator, suitable for pos range (-100k, +100k) with a step of at least 0.01
// Returns a uniformly distributed float value between 0.00 and 1.00
float QuickNoise(float pos){
	return fract(sin(pos) * 13159.52714);
}
double QuickNoise(double pos){
	return fract(sin(pos) * 13159.527140783267);
}
float QuickNoise(vec3 pos) {
	return fract(sin(dot(pos, vec3(13.657,9.558,11.606))) * 24097.524);
}
float QuickNoise(vec2 pos) {
	return fract(sin(dot(pos, vec2(13.657,9.558))) * 24097.524);
}
double QuickNoise(dvec3 pos) {
	return fract(sin(dot(pos, dvec3(13.657023817,9.5580981772,11.606065918))) * 24097.5240569198);
}
double QuickNoise(dvec2 pos) {
	return fract(sin(dot(pos, dvec2(13.657023817,9.5580981772))) * 24097.5240569198);
}


// 1-dimentional noise, suitable for pos range (-1M, +1M) with a step of 0.1 and gradient of 1.0
// Returns a float value between 0.00 and 1.00 with a somewhat uniform distribution
float Noise(float pos) {
	float fl = floor(pos);
	float fc = fract(pos);
	return mix(QuickNoise(fl), QuickNoise(fl + 1.0), fc);
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
	return mix(mix(QuickNoise(b), QuickNoise(b + d.yx), f.x), mix(QuickNoise(b + d.xy), QuickNoise(b + d.yy), f.x), f.y);
}
double Noise(dvec2 pos) {
	const dvec2 d = dvec2(0.0, 1.0);
	dvec2 b = floor(pos), f = smoothstep(dvec2(0.0), dvec2(1.0), fract(pos));
	return mix(mix(QuickNoise(b), QuickNoise(b + d.yx), f.x), mix(QuickNoise(b + d.xy), QuickNoise(b + d.yy), f.x), f.y);
}


// simple-precision Simplex noise, suitable for pos range (-1M, +1M) with a step of 0.001 and gradient of 1.0
// Returns a float value between -1.000 and +1.000 with a distribution that strongly tends towards the center (0.5)
float Simplex(vec3 pos){
	const vec2 C = vec2(1.0/6.0, 1.0/3.0);
	const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

	vec3 i = floor(pos + dot(pos, C.yyy));
	vec3 x0 = pos - i + dot(i, C.xxx);

	vec3 g = step(x0.yzx, x0.xyz);
	vec3 l = 1.0 - g;
	vec3 i1 = min( g.xyz, l.zxy);
	vec3 i2 = max( g.xyz, l.zxy);

	vec3 x1 = x0 - i1 + 1.0 * C.xxx;
	vec3 x2 = x0 - i2 + 2.0 * C.xxx;
	vec3 x3 = x0 - 1. + 3.0 * C.xxx;

	i = mod(i, 289.0); 
	vec4 p = _permute(_permute(_permute(i.z + vec4(0.0, i1.z, i2.z, 1.0)) + i.y + vec4(0.0, i1.y, i2.y, 1.0)) + i.x + vec4(0.0, i1.x, i2.x, 1.0));

	float n_ = 1.0/7.0;
	vec3  ns = n_ * D.wyz - D.xzx;

	vec4 j = p - 49.0 * floor(p * ns.z *ns.z);

	vec4 x_ = floor(j * ns.z);
	vec4 y_ = floor(j - 7.0 * x_);

	vec4 x = x_ *ns.x + ns.yyyy;
	vec4 y = y_ *ns.x + ns.yyyy;
	vec4 h = 1.0 - abs(x) - abs(y);

	vec4 b0 = vec4(x.xy, y.xy);
	vec4 b1 = vec4(x.zw, y.zw);

	vec4 s0 = floor(b0)*2.0 + 1.0;
	vec4 s1 = floor(b1)*2.0 + 1.0;
	vec4 sh = -step(h, vec4(0.0));

	vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy;
	vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww;

	vec3 p0 = vec3(a0.xy,h.x);
	vec3 p1 = vec3(a0.zw,h.y);
	vec3 p2 = vec3(a1.xy,h.z);
	vec3 p3 = vec3(a1.zw,h.w);

	vec4 norm = 1.79284291400159 - 0.85373472095314 * vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3));
	p0 *= norm.x;
	p1 *= norm.y;
	p2 *= norm.z;
	p3 *= norm.w;

	vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
	return 42.0 * dot(m*m*m*m, vec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3)));
}
double Simplex(dvec3 pos){
	const dvec2 C = dvec2(1.0/6.0, 1.0/3.0);
	const dvec4 D = dvec4(0.0, 0.5, 1.0, 2.0);

	dvec3 i = floor(pos + dot(pos, C.yyy));
	dvec3 x0 = pos - i + dot(i, C.xxx);

	dvec3 g = step(x0.yzx, x0.xyz);
	dvec3 l = 1.0 - g;
	dvec3 i1 = min( g.xyz, l.zxy);
	dvec3 i2 = max( g.xyz, l.zxy);

	dvec3 x1 = x0 - i1 + 1.0 * C.xxx;
	dvec3 x2 = x0 - i2 + 2.0 * C.xxx;
	dvec3 x3 = x0 - 1. + 3.0 * C.xxx;

	i = mod(i, 289.0); 
	dvec4 p = _permute(_permute(_permute(i.z + dvec4(0.0, i1.z, i2.z, 1.0)) + i.y + dvec4(0.0, i1.y, i2.y, 1.0)) + i.x + dvec4(0.0, i1.x, i2.x, 1.0));

	double n_ = 1.0/7.0;
	dvec3  ns = n_ * D.wyz - D.xzx;

	dvec4 j = p - 49.0 * floor(p * ns.z *ns.z);

	dvec4 x_ = floor(j * ns.z);
	dvec4 y_ = floor(j - 7.0 * x_);

	dvec4 x = x_ *ns.x + ns.yyyy;
	dvec4 y = y_ *ns.x + ns.yyyy;
	dvec4 h = 1.0 - abs(x) - abs(y);

	dvec4 b0 = dvec4(x.xy, y.xy);
	dvec4 b1 = dvec4(x.zw, y.zw);

	dvec4 s0 = floor(b0)*2.0 + 1.0;
	dvec4 s1 = floor(b1)*2.0 + 1.0;
	dvec4 sh = -step(h, dvec4(0.0));

	dvec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy;
	dvec4 a1 = b1.xzyw + s1.xzyw*sh.zzww;

	dvec3 p0 = dvec3(a0.xy,h.x);
	dvec3 p1 = dvec3(a0.zw,h.y);
	dvec3 p2 = dvec3(a1.xy,h.z);
	dvec3 p3 = dvec3(a1.zw,h.w);

	dvec4 norm = 1.79284291400159 - 0.85373472095314 * dvec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3));
	p0 *= norm.x;
	p1 *= norm.y;
	p2 *= norm.z;
	p3 *= norm.w;

	dvec4 m = max(0.6 - dvec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
	return 42.0 * dot(m*m*m*m, dvec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3)));
}


// Faster Simplex noise, less precise and not well tested, seems suitable for pos ranges (-10k, +10k) with a step of 0.01 and gradient of 0.5
// Returns a float value between -1.00 and +1.00 with a distribution that strongly tends towards the center (0.5)
float FastSimplex(vec3 pos) {
	const float F3 = 0.3333333;
	const float G3 = 0.1666667;

	vec3 s = floor(pos + dot(pos, vec3(F3)));
	vec3 x = pos - s + dot(s, vec3(G3));

	vec3 e = step(vec3(0.0), x - x.yzx);
	vec3 i1 = e * (1.0 - e.zxy);
	vec3 i2 = 1.0 - e.zxy * (1.0 - e);

	vec3 x1 = x - i1 + G3;
	vec3 x2 = x - i2 + 2.0 * G3;
	vec3 x3 = x - 1.0 + 3.0 * G3;

	vec4 w, d;

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

	return dot(d, vec4(52.0));
}
double FastSimplex(dvec3 pos) {
	const double F3 = 0.33333333333333333;
	const double G3 = 0.16666666666666667;

	dvec3 s = floor(pos + dot(pos, dvec3(F3)));
	dvec3 x = pos - s + dot(s, dvec3(G3));

	dvec3 e = step(dvec3(0.0), x - x.yzx);
	dvec3 i1 = e * (1.0 - e.zxy);
	dvec3 i2 = 1.0 - e.zxy * (1.0 - e);

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







///////////////////////////////////////



float easeOut(float t) {
	return sin(t * 3.14159265459 * 0.5);
}
float easeIn(float t) {
	return 1.0 - cos(t * 3.14159265459 * 0.5);
}
float clamp01(float v) {
	return max(0.0, min(1.0, v));
}

float FastSimplexFractal(vec3 pos, int octaves) {
	float amplitude = 0.5333333333;
	float frequency = 1.0;
	float f = FastSimplex(pos * frequency);
	if (octaves > 1) for (int i = 1; i < octaves; ++i) {
		amplitude /= 2.0;
		frequency *= 2.0;
		f += amplitude * FastSimplex(pos * frequency);
	}
	return f;
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
	float type = QuickNoise(galaxyPosition / 10.0);
	if (type < 0.2) {
		// Elliptical galaxy (20% probability)
		info.spiralCloudsFactor = 0.0;
		info.coreSize = QuickNoise(galaxyPosition);
		info.squish = QuickNoise(galaxyPosition+vec3(-0.33,-0.17,-0.51)) / 2.0;
	} else {
		if (type > 0.3) {
			// Irregular galaxy (70% probability, within spiral)
			info.irregularities = QuickNoise(galaxyPosition+vec3(-0.65,0.69,-0.71));
		} else {
			info.irregularities = 0.0;
		}
		// Spiral galaxy (80% probability, including irregular, only 10% will be regular)
		vec3 n1 = Noise3(galaxyPosition+vec3(0.01,0.43,-0.55)) / 2.0 + 0.5;
		vec3 n2 = Noise3(galaxyPosition+vec3(-0.130,0.590,-0.550)) / 2.0 + 0.5;
		vec3 n3 = Noise3(galaxyPosition+vec3(0.510,-0.310,0.512)) / 2.0 + 0.5;
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
	if (info.spiralCloudsFactor > 0.0 || info.squish > 0.2) {
		vec3 axis = normalize(Noise3(galaxyPosition+vec3(-0.212,0.864,0.892)));
		float angle = QuickNoise(galaxyPosition+vec3(0.176,0.917,1.337)) * 3.14159265459;
		float s = sin(angle);
		float c = cos(angle);
		float oc = 1.0 - c;
		info.rotation = mat4(
			oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s, 0.0,
			oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s, 0.0,
			oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c, 0.0,
			0.0, 0.0, 0.0, 1.0
		);
	}
	return info;
}

float GalaxyStarDensity(in vec3 pos, in GalaxyInfo info, int octaves) {
	float len = length(pos);
	if (len > 1.0) return 0.0;

	// Rotation
	if (info.spiralCloudsFactor > 0.0 || info.squish > 0.2) pos = (info.rotation * vec4(pos, 1.0)).xyz;

	float squish = info.squish * 2.0;
	float lenSquished = length(pos*vec3(1.0, squish + 1.0, 1.0));
	float radiusGradient = 1.0 - pow(clamp01(len + abs(pos.y)*squish), 5.0);

	float core = clamp01(pow(1.0-lenSquished/info.coreSize, 5.0) + pow(1.0-lenSquished/info.coreSize, 10.0));
	if (core + radiusGradient <= 0.0) return 0.0;
	float finalDensity = core + pow(max(0.0, radiusGradient - 0.2), 10.0);

	if (info.spiralCloudsFactor == 0.0) {
		return finalDensity;
	}

	vec3 noiseOffset = info.noiseOffset * 65.4105;

	// Irregular
	if (info.irregularities > 0.0) {
		vec3 irregular = info.noiseOffset/2.0+.5;
		pos = mix(pos, pos*irregular, info.irregularities);
		info.spiralCloudsFactor = mix(info.spiralCloudsFactor, sin(irregular.y), info.irregularities);
		info.swirlTwist = mix(info.swirlTwist, irregular.x, info.irregularities);
		info.cloudsSize = mix(info.cloudsSize, irregular.y, info.irregularities);
		info.attenuationCloudsFrequency = mix(info.attenuationCloudsFrequency, irregular.z, info.irregularities);
		info.attenuationCloudsFactor = mix(info.attenuationCloudsFactor, irregular.x, info.irregularities);
		core += clamp01(pow(1.0-length(pos+info.noiseOffset)/info.coreSize*irregular.x, (sin(irregular.x)+1.0)*3.0));
		finalDensity += core * pow(max(0.0, radiusGradient), 1.5*irregular.x+1.0);
	}

	// Spiral
	float swirl = len * info.swirlTwist * 10.0;
	float spiralNoise = FastSimplexFractal((vec3(
		pos.x * cos(swirl) - pos.z * sin(swirl),
		pos.y * (squish * 10.0 + 1.0),
		pos.z * cos(swirl) + pos.x * sin(swirl)
	)+noiseOffset)*info.cloudsFrequency*5.0, octaves)/2.0+0.5;
	float spirale = clamp01(pow(spiralNoise, (1.1-info.swirlDetail)*5.0) + (info.cloudsSize*1.5) - len*1.5 - (abs(pos.y)*squish*10.0)) * radiusGradient;
	finalDensity += 1.0-pow(1.0-spirale, info.spiralCloudsFactor*4.0);
	if (finalDensity <= 0.0) return 0.0;

	finalDensity *= min(1.0, FastSimplexFractal(pos * 234.31, octaves)/2.0+0.8);

	// Attenuation Clouds
	float attenClouds = pow(clamp01(1.0-abs(FastSimplexFractal((vec3(
		pos.x * cos(swirl / 2.5) - pos.z * sin(swirl / 2.5),
		pos.y * (squish * 2.0 + 1.0),
		pos.z * cos(swirl / 2.5) + pos.x * sin(swirl / 2.5)
	)+noiseOffset)*info.attenuationCloudsFrequency*20.0, octaves + 1))-core*3.0) * easeIn(radiusGradient), (3.0-info.attenuationCloudsFactor*2.0)) * info.attenuationCloudsFactor * 2.0;
	if (info.attenuationCloudsFactor > 0.0) finalDensity -= attenClouds * clamp01((FastSimplex((pos+info.noiseOffset)*info.attenuationCloudsFrequency*9.0)/2.0+0.5) * radiusGradient - (abs(pos.y)*squish*3.0));

	return finalDensity;
}

vec3 GalaxyStarColor(in vec3 pos, in GalaxyInfo info) {
	vec4 starType = normalize(vec4(
		/*red*/		QuickNoise(pos+info.noiseOffset+vec3(1.337,0.612,1.065)) * 0.5+pow(1.0-length(pos), 4.0)*2.0,
		/*yellow*/	QuickNoise(pos+info.noiseOffset+vec3(0.176,1.337,0.099)) * 1.4,
		/*blue*/	QuickNoise(pos+info.noiseOffset+vec3(1.337,0.420,1.099)) * 0.8+pow(length(pos), 2.0)*5.0,
		/*white*/	QuickNoise(pos+info.noiseOffset+vec3(1.337,1.185,0.474)) * 1.0 
	));
	return normalize((normalize(
		/*red*/		vec3( 1.0 , 0.4 , 0.2 ) * starType.x +
		/*yellow*/	vec3( 1.0 , 1.0 , 0.3 ) * starType.y +
		/*blue*/	vec3( 0.2 , 0.4 , 1.0 ) * starType.z +
		/*white*/	vec3( 1.0 , 1.0 , 1.0 ) * starType.w ))
		+ Noise3(pos * 64.31)/2.0);
}


