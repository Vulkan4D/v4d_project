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
layout(binding = 2, set = 0) uniform UBO {
	mat4 view;
	mat4 proj;
    vec4 light;
	vec3 ambient;
	int rtx_reflection_max_recursion;
	bool rtx_shadows;
} ubo;
layout(binding = 1, set = 0, rgba8) uniform image2D image;
layout(binding = 3, set = 0) buffer Vertices { vec4 vertexBuffer[]; };
layout(binding = 4, set = 0) buffer Indices { uint indexBuffer[]; };
layout(binding = 5, set = 0) readonly buffer Spheres { vec4 sphereBuffer[]; };

// Vertex
struct Vertex {
	vec3 pos;
	float roughness;
	vec3 normal;
	float reflector;
	vec4 color;
};
uint vertexStructSize = 3;
Vertex unpackVertex(uint index) {
	Vertex v;
	v.pos = vertexBuffer[vertexStructSize * index + 0].xyz;
	v.roughness = vertexBuffer[vertexStructSize * index + 0].w;
	v.normal = vertexBuffer[vertexStructSize * index + 1].xyz;
	v.reflector = vertexBuffer[vertexStructSize * index + 1].w;
	v.color = vertexBuffer[vertexStructSize * index + 2];
	return v;
}

// Sphere
struct Sphere {
	vec3 pos;
	float radius;
	vec4 color;
	float reflector;
	float roughness;
};
uint sphereStructSize = 4;
Sphere unpackSphere(uint index) {
	Sphere s;
	s.reflector = sphereBuffer[sphereStructSize * index + 1].z;
	s.roughness = sphereBuffer[sphereStructSize * index + 1].w;
	s.pos = sphereBuffer[sphereStructSize * index + 2].xyz;
	s.radius = sphereBuffer[sphereStructSize * index + 2].w;
	s.color = sphereBuffer[sphereStructSize * index + 3];
	return s;
}




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
	
float noise(vec2 n) {
	const vec2 d = vec2(0.0, 1.0);
	vec2 b = floor(n), f = smoothstep(vec2(0.0), vec2(1.0), fract(n));
	return mix(mix(rand(b), rand(b + d.yx), f.x), mix(rand(b + d.xy), rand(b + d.yy), f.x), f.y);
}

// #define PI 3.14159265358979323846

// float rand(vec2 c){
// 	return fract(sin(dot(c.xy ,vec2(12.9898,78.233))) * 43758.5453);
// }

// float noise(vec2 p, float freq ){
// 	float unit = 1.0/freq;
// 	vec2 ij = floor(p/unit);
// 	vec2 xy = mod(p,unit)/unit;
// 	//xy = 3.*xy*xy-2.*xy*xy*xy;
// 	xy = .5*(1.-cos(PI*xy));
// 	float a = rand((ij+vec2(0.,0.)));
// 	float b = rand((ij+vec2(1.,0.)));
// 	float c = rand((ij+vec2(0.,1.)));
// 	float d = rand((ij+vec2(1.,1.)));
// 	float x1 = mix(a, b, xy.x);
// 	float x2 = mix(c, d, xy.x);
// 	return mix(x1, x2, xy.y);
// }

// float pNoise(vec2 p, int res){
// 	float persistance = .5;
// 	float n = 0.;
// 	float normK = 0.;
// 	float f = 4.;
// 	float amp = 1.;
// 	int iCount = 0;
// 	for (int i = 0; i<50; i++){
// 		n+=amp*noise(p, f);
// 		f*=2.;
// 		normK+=amp;
// 		amp*=persistance;
// 		if (iCount == res) break;
// 		iCount++;
// 	}
// 	float nf = n/normK;
// 	return nf*nf*nf*nf;
// }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




#############################################################

#shader rgen

layout(location = 0) rayPayloadNV RayPayload ray;

// Max. number of recursion is passed via a specialization constant
// layout (constant_id = 0) const int MAX_RECURSION = 0;

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

// Ray Tracing
layout(location = 0) rayPayloadInNV RayPayload ray;
layout(location = 2) rayPayloadNV bool shadowed;
hitAttributeNV vec3 attribs;

void main() {
	// Hit Triangle vertices
	const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	const ivec3 index = ivec3(indexBuffer[3 * gl_PrimitiveID], indexBuffer[3 * gl_PrimitiveID + 1], indexBuffer[3 * gl_PrimitiveID + 2]);
	const Vertex v0 = unpackVertex(index.x);
	const Vertex v1 = unpackVertex(index.y);
	const Vertex v2 = unpackVertex(index.z);
	
	// Hit World Position
	const vec3 hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	// Interpolate Vertex data
	const float roughness = v0.roughness * barycentricCoords.x + v1.roughness * barycentricCoords.y + v2.roughness * barycentricCoords.z;
	const vec3 normal = normalize(v0.normal * barycentricCoords.x + v1.normal * barycentricCoords.y + v2.normal * barycentricCoords.z);
	// const float reflector = v0.reflector * barycentricCoords.x + v1.reflector * barycentricCoords.y + v2.reflector * barycentricCoords.z;
	const vec4 color = normalize(v0.color * barycentricCoords.x + v1.color * barycentricCoords.y + v2.color * barycentricCoords.z);
	const float specular = 1;

	// Basic shading (with light angle)
	const vec3 lightVector = normalize(ubo.light.xyz - hitPoint);
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
	
	// Reflections
	if (ubo.rtx_reflection_max_recursion > 1) {
		ray.distance = gl_RayTmaxNV;
		ray.origin = hitPoint + normal * 0.001f;
		ray.direction = reflect(gl_WorldRayDirectionNV, normal);
		ray.reflector = v0.reflector * barycentricCoords.x + v1.reflector * barycentricCoords.y + v2.reflector * barycentricCoords.z;
	}
}


#############################################################

#shader rmiss

layout(location = 0) rayPayloadInNV RayPayload ray;

void main() {
	ray.color = vec3(0.7, 0.8, 1.0);
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

layout(location = 0) rayPayloadInNV RayPayload ray;
layout(location = 2) rayPayloadNV bool shadowed;

hitAttributeNV Sphere sphere;

void main() {
	// Hit World Position
	const vec3 hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	// Normal
	const vec3 normal = (hitPoint - sphere.pos) / sphere.radius;
	// Specular
	const float specular = 0.8;

	// Basic shading (with light angle)
	const vec3 lightVector = normalize(ubo.light.xyz - hitPoint);
	const float dot_product = max(dot(lightVector, normal), 0.0);
	const float shade = pow(dot_product, specular);
	ray.color = mix(ubo.ambient, max(ubo.ambient, sphere.color.rgb * ubo.light.w), shade);
	
	// Receive Shadows
	if (shade > 0.0 && ubo.rtx_shadows) {
		shadowed = true;  
		traceNV(topLevelAS, gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsOpaqueNV | gl_RayFlagsSkipClosestHitShaderNV, 0xFF, 0, 0, 1, hitPoint, 0.001, lightVector, length(ubo.light.xyz - hitPoint), 2);
		if (shadowed) {
			ray.color = ubo.ambient;
		}
	}
	
	// Reflections
	if (ubo.rtx_reflection_max_recursion > 1) {
		ray.distance = gl_RayTmaxNV;
		ray.origin = hitPoint + normal * 0.001f;
		ray.direction = reflect(gl_WorldRayDirectionNV, normal);
		ray.reflector = sphere.reflector;
	}
}
