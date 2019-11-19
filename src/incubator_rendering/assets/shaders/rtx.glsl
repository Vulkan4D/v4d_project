#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

// Ray Tracing Payload
struct RayPayload {
	vec3 color;
	vec3 origin;
	vec3 direction;
	float reflector;
	float distance;
};

// Layout Bindings
layout(binding = 0, set = 0) uniform accelerationStructureNV topLevelAS;
layout(binding = 1, set = 0, rgba8) uniform image2D image;
layout(binding = 2, set = 0) uniform UBO {
	mat4 view;
	mat4 proj;
    vec4 light;
	vec3 ambient;
	float time;
	int samplesPerPixel;
	int rtx_reflection_max_recursion;
	bool rtx_shadows;
} ubo;
layout(binding = 3, set = 0) buffer Vertices { vec4 vertexBuffer[]; };
layout(binding = 4, set = 0) buffer Indices { uint indexBuffer[]; };
layout(binding = 5, set = 0) readonly buffer Spheres { vec4 sphereBuffer[]; };





////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Noise functions

// https://gist.github.com/patriciogonzalezvivo/670c22f3966e662d2f83

float rand(float n){return fract(sin(n) * 43758.5453123);}

float rand(vec2 n) { 
	return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float noise(float p){
	float fl = floor(p);
	float fc = fract(p);
	return mix(rand(fl), rand(fl + 1.0), fc);
}

float noise(vec2 pos) {
	const vec2 d = vec2(0.0, 1.0);
	vec2 b = floor(pos), f = smoothstep(vec2(0.0), vec2(1.0), fract(pos));
	return mix(mix(rand(b), rand(b + d.yx), f.x), mix(rand(b + d.xy), rand(b + d.yy), f.x), f.y);
}

float noise(vec2 pos, int octaves) {
	if (octaves == 1) return noise(pos);
	float n = 0.0;
	for (float i = 1.0; i <= octaves; i++) {
		n += noise(pos * i) / i;
	}
	return n;
}

#extension GL_EXT_control_flow_attributes : require

// Generates a seed for a random number generator from 2 inputs plus a backoff
// https://github.com/nvpro-samples/optix_prime_baking/blob/master/random.h
// https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm
uint InitRandomSeed(uint val0, uint val1)
{
	uint v0 = val0, v1 = val1, s0 = 0;

	[[unroll]] 
	for (uint n = 0; n < 16; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}

	return v0;
}

uint RandomInt(inout uint seed)
{
	// LCG values from Numerical Recipes
    return (seed = 1664525 * seed + 1013904223);
}

float RandomFloat(inout uint seed)
{
	// Float version using bitmask from Numerical Recipes
	const uint one = 0x3f800000;
	const uint msk = 0x007fffff;
	return uintBitsToFloat(one | (msk & (RandomInt(seed) >> 9))) - 1;
}

vec2 RandomInUnitDisk(inout uint seed)
{
	for (;;)
	{
		const vec2 p = 2 * vec2(RandomFloat(seed), RandomFloat(seed)) - 1;
		if (dot(p, p) < 1)
		{
			return p;
		}
	}
}

vec3 RandomInUnitSphere(inout uint seed)
{
	for (;;)
	{
		const vec3 p = 2 * vec3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed)) - 1;
		if (dot(p, p) < 1)
		{
			return p;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
float permute(float x){return floor(mod(((x*34.0)+1.0)*x, 289.0));}
vec4 taylorInvSqrt(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}
float taylorInvSqrt(float r){return 1.79284291400159 - 0.85373472095314 * r;}

vec4 grad4(float j, vec4 ip){
  const vec4 ones = vec4(1.0, 1.0, 1.0, -1.0);
  vec4 p,s;

  p.xyz = floor( fract (vec3(j) * ip.xyz) * 7.0) * ip.z - 1.0;
  p.w = 1.5 - dot(abs(p.xyz), ones.xyz);
  s = vec4(lessThan(p, vec4(0.0)));
  p.xyz = p.xyz + (s.xyz*2.0 - 1.0) * s.www; 

  return p;
}

float snoise(vec4 v){
  const vec2  C = vec2( 0.138196601125010504,  // (5 - sqrt(5))/20  G4
                        0.309016994374947451); // (sqrt(5) - 1)/4   F4
// First corner
  vec4 i  = floor(v + dot(v, C.yyyy) );
  vec4 x0 = v -   i + dot(i, C.xxxx);

// Other corners

// Rank sorting originally contributed by Bill Licea-Kane, AMD (formerly ATI)
  vec4 i0;

  vec3 isX = step( x0.yzw, x0.xxx );
  vec3 isYZ = step( x0.zww, x0.yyz );
//  i0.x = dot( isX, vec3( 1.0 ) );
  i0.x = isX.x + isX.y + isX.z;
  i0.yzw = 1.0 - isX;

//  i0.y += dot( isYZ.xy, vec2( 1.0 ) );
  i0.y += isYZ.x + isYZ.y;
  i0.zw += 1.0 - isYZ.xy;

  i0.z += isYZ.z;
  i0.w += 1.0 - isYZ.z;

  // i0 now contains the unique values 0,1,2,3 in each channel
  vec4 i3 = clamp( i0, 0.0, 1.0 );
  vec4 i2 = clamp( i0-1.0, 0.0, 1.0 );
  vec4 i1 = clamp( i0-2.0, 0.0, 1.0 );

  //  x0 = x0 - 0.0 + 0.0 * C 
  vec4 x1 = x0 - i1 + 1.0 * C.xxxx;
  vec4 x2 = x0 - i2 + 2.0 * C.xxxx;
  vec4 x3 = x0 - i3 + 3.0 * C.xxxx;
  vec4 x4 = x0 - 1.0 + 4.0 * C.xxxx;

// Permutations
  i = mod(i, 289.0); 
  float j0 = permute( permute( permute( permute(i.w) + i.z) + i.y) + i.x);
  vec4 j1 = permute( permute( permute( permute (
             i.w + vec4(i1.w, i2.w, i3.w, 1.0 ))
           + i.z + vec4(i1.z, i2.z, i3.z, 1.0 ))
           + i.y + vec4(i1.y, i2.y, i3.y, 1.0 ))
           + i.x + vec4(i1.x, i2.x, i3.x, 1.0 ));
// Gradients
// ( 7*7*6 points uniformly over a cube, mapped onto a 4-octahedron.)
// 7*7*6 = 294, which is close to the ring size 17*17 = 289.

  vec4 ip = vec4(1.0/294.0, 1.0/49.0, 1.0/7.0, 0.0) ;

  vec4 p0 = grad4(j0,   ip);
  vec4 p1 = grad4(j1.x, ip);
  vec4 p2 = grad4(j1.y, ip);
  vec4 p3 = grad4(j1.z, ip);
  vec4 p4 = grad4(j1.w, ip);

// Normalise gradients
  vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;
  p4 *= taylorInvSqrt(dot(p4,p4));

// Mix contributions from the five corners
  vec3 m0 = max(0.6 - vec3(dot(x0,x0), dot(x1,x1), dot(x2,x2)), 0.0);
  vec2 m1 = max(0.6 - vec2(dot(x3,x3), dot(x4,x4)            ), 0.0);
  m0 = m0 * m0;
  m1 = m1 * m1;
  return 49.0 * ( dot(m0*m0, vec3( dot( p0, x0 ), dot( p1, x1 ), dot( p2, x2 )))
               + dot(m1*m1, vec2( dot( p3, x3 ), dot( p4, x4 ) ) ) ) ;

}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



#####################################################

#common .*rchit

layout(location = 0) rayPayloadInNV RayPayload ray;
layout(location = 2) rayPayloadNV bool shadowed;

void ApplyStandardShading(vec3 hitPoint, vec3 objPoint, vec4 color, vec3 normal, float emissive, float roughness, float specular, float metallic) {
	
	// Roughness
	if (roughness > 0.0) {
		float n1 = 1.0 + noise(objPoint.xy * 1000.0 * min(4.0, roughness) + 134.455, 3) * roughness / 2.0;
		float n2 = 1.0 + noise(objPoint.yz * 1000.0 * min(4.0, roughness) + 2.5478787, 3) * roughness / 2.0;
		float n3 = 1.0 + noise(objPoint.xz * 1000.0 * min(4.0, roughness) + -124.785, 3) * roughness / 2.0;
		normal = normalize(normal * vec3(n1, n2, n3)/2.0);
	}
	
	// Basic shading from light angle
	vec3 lightVector = normalize(ubo.light.xyz - hitPoint);
	const float dot_product = max(dot(lightVector, normal), 0.0);
	const float shade = pow(dot_product, specular);
	ray.color = mix(ubo.ambient, max(ubo.ambient, color.rgb * ubo.light.w), shade);
	
	// Receive Shadows
	if (shade > 0.0 && ubo.rtx_shadows) {
		shadowed = true;
		traceNV(topLevelAS, gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsOpaqueNV | gl_RayFlagsSkipClosestHitShaderNV, 0xFF, 0, 0, 1, hitPoint, 0.001, lightVector, length(ubo.light.xyz - hitPoint), 2);
		if (shadowed) {
			ray.color = ubo.ambient;
		}
	}
	
	//TODO opacity
	
	// Reflections
	if (ubo.rtx_reflection_max_recursion > 1) {
		ray.distance = gl_RayTmaxNV;
		ray.origin = hitPoint + normal * 0.001f;
		ray.direction = reflect(gl_WorldRayDirectionNV, normal);
		ray.reflector = metallic;
	}
}


#####################################################

#common sphere.*

// Sphere
struct Sphere {
	float emissive;
	float roughness;
	vec3 pos;
	float radius;
	vec4 color;
	float specular;
	float metallic;
	float refraction;
	float density;
};
uint sphereStructSize = 5;
Sphere unpackSphere(uint index) {
	Sphere s;
	s.emissive = sphereBuffer[sphereStructSize * index + 1].z;
	s.roughness = sphereBuffer[sphereStructSize * index + 1].w;
	s.pos = sphereBuffer[sphereStructSize * index + 2].xyz;
	s.radius = sphereBuffer[sphereStructSize * index + 2].w;
	s.color = sphereBuffer[sphereStructSize * index + 3];
	s.specular = sphereBuffer[sphereStructSize * index + 4].x;
	s.metallic = sphereBuffer[sphereStructSize * index + 4].y;
	s.refraction = sphereBuffer[sphereStructSize * index + 4].z;
	s.density = sphereBuffer[sphereStructSize * index + 4].w;
	return s;
}


#############################################################

#shader rgen

layout(location = 0) rayPayloadNV RayPayload ray;

void main() {
	const vec2 pixelCenter = vec2(gl_LaunchIDNV.xy) + vec2(0.5);
	const vec2 inUV = pixelCenter/vec2(gl_LaunchSizeNV.xy);
	const vec2 d = inUV * 2.0 - 1.0;

	const vec3 target = vec4(ubo.proj * vec4(d.x, d.y, 1, 1)).xyz;
	vec3 origin = vec4(ubo.view * vec4(0,0,0,1)).xyz;
	vec3 direction = vec4(ubo.view * vec4(normalize(target), 0)).xyz;
	
	vec3 finalColor = vec3(0.0);
	float max_distance = 10000.0;
	
	if (ubo.rtx_reflection_max_recursion > 1) {
		float reflection = 1.0;
		for (int i = 0; i < ubo.rtx_reflection_max_recursion; i++) {
			ray.reflector = 0.0;
			traceNV(topLevelAS, gl_RayFlagsOpaqueNV, 0xff, 0, 0, 0, origin, 0.001, direction, max_distance, 0);
			finalColor = mix(finalColor, ray.color, reflection);
			if (ray.reflector <= 0.0) break;
			reflection *= ray.reflector;
			if (reflection < 0.01) break;
			max_distance -= ray.distance;
			if (max_distance <= 0) break;
			origin = ray.origin;
			direction = ray.direction;
		}
	} else {
		traceNV(topLevelAS, gl_RayFlagsOpaqueNV, 0xff, 0, 0, 0, origin, 0.001, direction, max_distance, 0);
		finalColor = ray.color;
	}
	
	imageStore(image, ivec2(gl_LaunchIDNV.xy), vec4(finalColor, 0.0));
}


#############################################################

#shader rchit

hitAttributeNV vec3 attribs;

// Vertex
struct Vertex {
	vec3 pos;
	float roughness;
	vec3 normal;
	float emissive;
	vec4 color;
	vec2 uv;
	float specular;
	float metallic;
};
uint vertexStructSize = 4;
Vertex unpackVertex(uint index) {
	Vertex v;
	v.pos = vertexBuffer[vertexStructSize * index + 0].xyz;
	v.roughness = vertexBuffer[vertexStructSize * index + 0].w;
	v.normal = vertexBuffer[vertexStructSize * index + 1].xyz;
	v.emissive = vertexBuffer[vertexStructSize * index + 1].w;
	v.color = vertexBuffer[vertexStructSize * index + 2];
	v.uv = vertexBuffer[vertexStructSize * index + 3].xy;
	v.specular = vertexBuffer[vertexStructSize * index + 3].z;
	v.metallic = vertexBuffer[vertexStructSize * index + 3].w;
	return v;
}

void main() {
	// Hit Triangle vertices
	const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	const ivec3 index = ivec3(indexBuffer[3 * gl_PrimitiveID], indexBuffer[3 * gl_PrimitiveID + 1], indexBuffer[3 * gl_PrimitiveID + 2]);
	const Vertex v0 = unpackVertex(index.x);
	const Vertex v1 = unpackVertex(index.y);
	const Vertex v2 = unpackVertex(index.z);
	
	// Hit World Position
	const vec3 hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	// Hit object Position
	const vec3 objPoint = hitPoint - normalize(v0.pos * barycentricCoords.x + v1.pos * barycentricCoords.y + v2.pos * barycentricCoords.z);
	// Interpolate Vertex data
	const vec4 color = normalize(v0.color * barycentricCoords.x + v1.color * barycentricCoords.y + v2.color * barycentricCoords.z);
	const vec3 normal = normalize(v0.normal * barycentricCoords.x + v1.normal * barycentricCoords.y + v2.normal * barycentricCoords.z);
	const float emissive = v0.emissive * barycentricCoords.x + v1.emissive * barycentricCoords.y + v2.emissive * barycentricCoords.z;
	const float roughness = v0.roughness * barycentricCoords.x + v1.roughness * barycentricCoords.y + v2.roughness * barycentricCoords.z;
	const float specular = v0.specular * barycentricCoords.x + v1.specular * barycentricCoords.y + v2.specular * barycentricCoords.z;
	const float metallic = v0.metallic * barycentricCoords.x + v1.metallic * barycentricCoords.y + v2.metallic * barycentricCoords.z;

	ApplyStandardShading(hitPoint, objPoint, color, normal, emissive, roughness, specular, metallic);
}


#############################################################

#shader rmiss

layout(location = 0) rayPayloadInNV RayPayload ray;

void main() {
	// ray.color = vec3(0.7, 0.8, 1.0);
	ray.color = vec3(0.0, 0.0, 0.0);
}


#############################################################

#shader shadow.rmiss

layout(location = 2) rayPayloadInNV bool shadowed;

void main() {
	shadowed = false;
}


#############################################################

#shader sphere.rint

hitAttributeNV Sphere attribs;

void main() {
	// const Sphere sphere = unpackSphere(gl_InstanceCustomIndexNV);
	const Sphere sphere = unpackSphere(gl_PrimitiveID);
	const vec3 origin = gl_WorldRayOriginNV;
	const vec3 direction = gl_WorldRayDirectionNV;
	const float tMin = gl_RayTminNV;
	const float tMax = gl_RayTmaxNV;
	
	const vec3 oc = origin - sphere.pos;
	const float a = dot(direction, direction);
	const float b = dot(oc, direction);
	const float c = dot(oc, oc) - sphere.radius * sphere.radius;
	const float discriminant = b * b - a * c;

	if (discriminant >= 0) {
		const float discriminantSqrt = sqrt(discriminant);
		const float t1 = (-b - discriminantSqrt) / a;
		const float t2 = (-b + discriminantSqrt) / a;

		if ((tMin <= t1 && t1 < tMax) || (tMin <= t2 && t2 < tMax)) {
			attribs = sphere;
			reportIntersectionNV((tMin <= t1 && t1 < tMax) ? t1 : t2, 0);
		}
	}
}


#############################################################

#shader sphere.rchit

hitAttributeNV Sphere sphere;

float snoise(vec4 pos, float octaves) {
	if (octaves == 1) return snoise(pos);
	float n = 0.0;
	for (float i = 1.0; i <= octaves; i++) {
		n += snoise(pos * i) / i;
	}
	return n;
}

void main() {
	// Hit World Position
	const vec3 hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	// Hit object Position
	const vec3 objPoint = hitPoint - sphere.pos;
	// Normal
	const vec3 normal = objPoint / sphere.radius;
	
	// if (sphere.metallic == 0.0) {
	// 	float r = (snoise(vec4(objPoint, ubo.time / 60.0) * 10.0, 30) / 2.0 + 0.5) * 1.5;
	// 	float g = (snoise(vec4(objPoint, ubo.time / 60.0) * 5.0, 20) / 2.0 + 0.5) * min(r, 0.9);
	// 	float b = (snoise(vec4(objPoint, ubo.time / 60.0) * 15.0, 20) / 2.0 + 0.5) * min(g, 0.7);
	// 	ray.color = vec3(max(r + g + b, 0.3), min(r - 0.2, max(g, b + 0.3)), b) * 1.8;
	// } else {
	ApplyStandardShading(hitPoint, objPoint, sphere.color, normal, sphere.emissive, sphere.roughness, sphere.specular, sphere.metallic);
	// }
}
