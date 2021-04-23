#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

const float EPSILON = 0.00001;

struct CapsuleAttr {
	float radius;
	float len;
	vec3 normal;
};

#############################################################
#shader rint

hitAttributeEXT CapsuleAttr capsuleAttr;

void main() {
	const vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	const vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	const vec3 ro = gl_ObjectRayOriginEXT;
	const vec3 rd = gl_ObjectRayDirectionEXT;
	
	// Compute pa, pb, r  from just the aabb info
	float r;
	vec3 pa;
	vec3 pb;
	{
		const float x = aabb_max.x - aabb_min.x;
		const float y = aabb_max.y - aabb_min.y;
		const float z = aabb_max.z - aabb_min.z;
		if (abs(x-y) < EPSILON) { // Z is length
			r = (aabb_max.x - aabb_min.x) / 2.0;
			pa.xy = (aabb_min.xy + aabb_max.xy) / 2.0;
			pb.xy = pa.xy;
			pa.z = aabb_min.z + sign(aabb_max.z - aabb_min.z) * r;
			pb.z = aabb_max.z + sign(aabb_min.z - aabb_max.z) * r;
		} else if (abs(x-z) < EPSILON) { // Y is length
			r = (aabb_max.x - aabb_min.x) / 2.0;
			pa.xz = (aabb_min.xz + aabb_max.xz) / 2.0;
			pb.xz = pa.xz;
			pa.y = aabb_min.y + sign(aabb_max.y - aabb_min.y) * r;
			pb.y = aabb_max.y + sign(aabb_min.y - aabb_max.y) * r;
		} else { // X is length
			r = (aabb_max.y - aabb_min.y) / 2.0;
			pa.yz = (aabb_min.yz + aabb_max.yz) / 2.0;
			pb.yz = pa.yz;
			pa.x = aabb_min.x + sign(aabb_max.x - aabb_min.x) * r;
			pb.x = aabb_max.x + sign(aabb_min.x - aabb_max.x) * r;
		}
	}
	
	// Ray-Capsule Intersection (ro, rd, pa, pb, r)
	const vec3 ba = pb - pa;
	const vec3 oa = ro - pa;
	const float baba = dot(ba, ba);
	const float bard = dot(ba, rd);
	const float baoa = dot(ba, oa);
	const float rdoa = dot(rd, oa);
	const float oaoa = dot(oa, oa);
	float a = baba - bard*bard;
	float b = baba*rdoa - baoa*bard;
	float c = baba*oaoa - baoa*baoa - r*r*baba;
	float h = b*b - a*c;
	
	if (h >= 0.0) {
		const float t1 = (-b-sqrt(h)) / a;
		const float t2 = (-b+sqrt(h)) / a;
		const float y1 = baoa + t1*bard;
		const float y2 = baoa + t2*bard;
		
		// cylinder body Outside surface
		if (y1 > 0.0 && y1 < baba && gl_RayTminEXT <= t1) {
			capsuleAttr.radius = r;
			capsuleAttr.len = length(ba);
			const vec3 posa = (gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * t1) - pa;
			capsuleAttr.normal = normalize((posa - clamp(dot(posa, ba) / dot(ba, ba), 0.0, 1.0) * ba) / r);
			reportIntersectionEXT(t1, 0);
			return;
		}
		
		vec3 oc;
		
		// rounded caps Outside surface
		// BUG: There is currently an issue with this when the ray origin starts inside the cylinder between points A and B, we can see part of the sphere of cap A. This should not be a problem if we always render the outside surfaces or for collision detection.
		oc = (y1 <= 0.0)? oa : ro - pb;
		b = dot(rd, oc);
		c = dot(oc, oc) - r*r;
		h = b*b - c;
		if (h > 0.0) {
			const float t = -b - sqrt(h);
			if (gl_RayTminEXT <= t) {
				capsuleAttr.radius = r;
				const vec3 posa = (gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * t) - pa;
				capsuleAttr.normal = normalize((posa - clamp(dot(posa, ba) / dot(ba, ba), 0.0, 1.0) * ba) / r);
				if (dot(capsuleAttr.normal, rd) < 0) {
					capsuleAttr.len = length(ba);
					reportIntersectionEXT(t, 2);
					return;
				}
			}
		}
		
		// cylinder body Inside surface
		if (y2 > 0.0 && y2 < baba && gl_RayTminEXT <= t2) {
			capsuleAttr.radius = r;
			capsuleAttr.len = length(ba);
			const vec3 posa = (gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * t2) - pa;
			capsuleAttr.normal = -normalize((posa - clamp(dot(posa, ba) / dot(ba, ba), 0.0, 1.0) * ba) / r);
			reportIntersectionEXT(t2, 1);
			return;
		}
		
		// rounded caps Inside surface
		oc = (y2 <= 0.0)? oa : ro - pb;
		b = dot(rd, oc);
		c = dot(oc, oc) - r*r;
		h = b*b - c;
		if (h > 0.0) {
			const float t = -b + sqrt(h);
			if (gl_RayTminEXT <= t) {
				capsuleAttr.radius = r;
				capsuleAttr.len = length(ba);
				const vec3 posa = (gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * t) - pa;
				capsuleAttr.normal = -normalize((posa - clamp(dot(posa, ba) / dot(ba, ba), 0.0, 1.0) * ba) / r);
				reportIntersectionEXT(t, 3);
				return;
			}
		}
		
	}
}


#############################################################
#shader visibility.rchit

hitAttributeEXT CapsuleAttr capsuleAttr;

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

void main() {
	WriteRayPayload(ray);
	
	// Vertex Colors
	if (HasVertexColor()) {
		ray.color = GetVertexColor(gl_PrimitiveID);
	} else {
		ray.color = vec4(1);
	}
	
	// normal
	ray.normal.xyz = capsuleAttr.normal;
	
	// Store useful information in the UV payload member
	ray.uv = vec2(capsuleAttr.radius, capsuleAttr.len);
}

