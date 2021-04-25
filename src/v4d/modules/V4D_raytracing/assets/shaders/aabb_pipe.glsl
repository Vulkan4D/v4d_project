#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

const float EPSILON = 0.00001;

struct PipeAttr {
	vec3 normal;
};

#############################################################
#shader rint

hitAttributeEXT PipeAttr pipeAttr;

float CapsuleSDF(in vec3 a, in vec3 b, in float r, in vec3 p) {
	const vec3 pa = p-a;
	const vec3 ba = b-a;
	const float h = clamp(dot(pa, ba) / dot(ba, ba), 0, 1);
	return length(pa-h*(ba)) - r;
}

void main() {
	const vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	const vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	const vec3 dir = gl_ObjectRayDirectionEXT;
	const vec3 o = gl_ObjectRayOriginEXT;
	
	// Compute a, b, r  from just the aabb info
	float r;
	vec3 a;
	vec3 b;
	{
		const float x = aabb_max.x - aabb_min.x;
		const float y = aabb_max.y - aabb_min.y;
		const float z = aabb_max.z - aabb_min.z;
		if (abs(x-y) < EPSILON) { // Z is length
			r = (aabb_max.x - aabb_min.x) / 2.0;
			a.xy = (aabb_min.xy + aabb_max.xy) / 2.0;
			b.xy = a.xy;
			a.z = aabb_min.z + sign(aabb_max.z - aabb_min.z) * r;
			b.z = aabb_max.z + sign(aabb_min.z - aabb_max.z) * r;
		} else if (abs(x-z) < EPSILON) { // Y is length
			r = (aabb_max.x - aabb_min.x) / 2.0;
			a.xz = (aabb_min.xz + aabb_max.xz) / 2.0;
			b.xz = a.xz;
			a.y = aabb_min.y + sign(aabb_max.y - aabb_min.y) * r;
			b.y = aabb_max.y + sign(aabb_min.y - aabb_max.y) * r;
		} else { // X is length
			r = (aabb_max.y - aabb_min.y) / 2.0;
			a.yz = (aabb_min.yz + aabb_max.yz) / 2.0;
			b.yz = a.yz;
			a.x = aabb_min.x + sign(aabb_max.x - aabb_min.x) * r;
			b.x = aabb_max.x + sign(aabb_min.x - aabb_max.x) * r;
		}
	}
	
	// Capsule Ray-Marching (p, dir, a, b, r)
	const int MAX_MARCH_COUNT = 16;
	const float DIST_THRESHOLD = 0.001;
	float dist = gl_RayTmaxEXT;
	float t = 0;
	vec3 p = o;
	for (int i = 0; i < MAX_MARCH_COUNT; ++i) {
		float d = CapsuleSDF(a, b, r, p);
		if (d > dist) return;
		dist = d;
		p += dir * d;
		t += d;
		if (t > gl_RayTmaxEXT) return;
		if (d < DIST_THRESHOLD) break;
	}
	if (t < gl_RayTminEXT) return;
	
	// Compute normal
	const vec3 ba = b - a;
	const vec3 oa = o - a;
	const float badir = dot(ba, dir);
	const float baoa = dot(ba, oa);
	const float baba = dot(ba, ba);
	const float y = baoa + badir * t;
	pipeAttr.normal = normalize((oa + dir*t - ba*y/baba) / r);
	
	reportIntersectionEXT(t, 0);
}


#############################################################
#shader visibility.rchit

hitAttributeEXT PipeAttr pipeAttr;

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

void main() {
	WriteRayPayload(ray);
	
	// Vertex Colors
	if (HasVertexColor()) {
		ray.color = GetVertexColor(gl_PrimitiveID);
	} else {
		ray.color = vec4(0,0,0,1);
	}
	
	// normal
	ray.normal.xyz = pipeAttr.normal;
	
	// Store useful information in the UV payload member ?
	// ray.uv = vec2(...);
}

