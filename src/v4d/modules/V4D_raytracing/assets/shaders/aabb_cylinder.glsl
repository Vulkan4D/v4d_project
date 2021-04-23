#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

const float EPSILON = 0.00001;

struct CylinderAttr {
	float radius;
	float len;
	vec3 normal;
};

#############################################################
#shader rint

hitAttributeEXT CylinderAttr cylinderAttr;

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
			pa.z = aabb_min.z;
			pb.z = aabb_max.z;
		} else if (abs(x-z) < EPSILON) { // Y is length
			r = (aabb_max.x - aabb_min.x) / 2.0;
			pa.xz = (aabb_min.xz + aabb_max.xz) / 2.0;
			pb.xz = pa.xz;
			pa.y = aabb_min.y;
			pb.y = aabb_max.y;
		} else { // X is length
			r = (aabb_max.y - aabb_min.y) / 2.0;
			pa.yz = (aabb_min.yz + aabb_max.yz) / 2.0;
			pb.yz = pa.yz;
			pa.x = aabb_min.x;
			pb.x = aabb_max.x;
		}
	}
	
	// Ray-Cylinder Intersection (ro, rd, pa, pb, r)
	const vec3 ba = pb - pa;
	const vec3 oc = ro - pa;
	const float baba = dot(ba, ba);
	const float bard = dot(ba, rd);
	const float baoc = dot(ba, oc);
	const float k2 = baba - bard*bard;
	const float k1 = baba * dot(oc, rd) - baoc*bard;
	const float k0 = baba * dot(oc, oc) - baoc*baoc - r*r*baba;
	float h = k1*k1 - k2*k0;
	
	if (h < 0.0) return;
	h = sqrt(h);
	
	const float t1 = (-k1-h) / k2;
	const float t2 = (-k1+h) / k2;
	const float y1 = baoc + bard * t1;
	const float y2 = baoc + bard * t2;
	
	// Cylinder body Outside surface
	if (y1 > 0.0 && y1 < baba) {
		if (gl_RayTminEXT <= t1) {
			cylinderAttr.radius = r;
			cylinderAttr.len = length(ba);
			cylinderAttr.normal = normalize((oc + rd*t1 - ba*y1/baba) / r);
			reportIntersectionEXT(t1, 0);
			return;
		}
	}
	
	// Flat caps Outside surface
	const float capsT1 = (((y1<0.0)? 0.0 : baba) - baoc) / bard;
	if (abs(k1+k2*capsT1) < h) {
		if (gl_RayTminEXT <= capsT1) {
			cylinderAttr.radius = r;
			cylinderAttr.normal = normalize(ba*sign(y1)/baba);
			if (dot(cylinderAttr.normal, rd) < 0) {
				cylinderAttr.len = length(ba);
				reportIntersectionEXT(capsT1, 2);
				return;
			}
		}
	}
	
	// Cylinder body Inside surface
	if (y2 > 0.0 && y2 < baba) {
		if (gl_RayTminEXT <= t2) {
			cylinderAttr.radius = r;
			cylinderAttr.len = length(ba);
			cylinderAttr.normal = -normalize((oc + rd*t2 - ba*y2/baba) / r);
			reportIntersectionEXT(t2, 1);
			return;
		}
	}
	
	// Flat caps Inside surface
	const float capsT2 = (((y2<0.0)? 0.0 : baba) - baoc) / bard;
	if (abs(k1+k2*capsT2) < h) {
		if (gl_RayTminEXT <= capsT2) {
			cylinderAttr.radius = r;
			cylinderAttr.len = length(ba);
			cylinderAttr.normal = -normalize(ba*sign(y2)/baba);
			reportIntersectionEXT(capsT2, 3);
			return;
		}
	}
	
}


#############################################################
#shader visibility.rchit

hitAttributeEXT CylinderAttr cylinderAttr;

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
	ray.normal.xyz = cylinderAttr.normal;
	
	// Store useful information in the UV payload member
	ray.uv = vec2(cylinderAttr.radius, cylinderAttr.len);
}

