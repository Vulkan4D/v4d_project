#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

const float EPSILON = 0.00001;

struct ConeAttr {
	float ra;
	float rb;
	float len;
	vec3 normal;
};

#############################################################
#shader rint

hitAttributeEXT ConeAttr coneAttr;

layout(buffer_reference, std430, buffer_reference_align = 4) buffer CustomData {
	float rb[];
};

float dot2(in vec3 v) {
	return dot(v,v);
}

void main() {
	const vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	const vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	const vec3 ro = gl_ObjectRayOriginEXT;
	const vec3 rd = gl_ObjectRayDirectionEXT;
	uint64_t customData_rb = GetCustomData();
	
	// Compute pa, pb, ra  from just the aabb info
	float ra;
	vec3 pa;
	vec3 pb;
	{
		const float x = aabb_max.x - aabb_min.x;
		const float y = aabb_max.y - aabb_min.y;
		const float z = aabb_max.z - aabb_min.z;
		if (abs(x-y) < EPSILON) { // Z is length
			ra = (aabb_max.x - aabb_min.x) / 2.0;
			pa.xy = (aabb_min.xy + aabb_max.xy) / 2.0;
			pb.xy = pa.xy;
			pa.z = aabb_min.z;
			pb.z = aabb_max.z;
		} else if (abs(x-z) < EPSILON) { // Y is length
			ra = (aabb_max.x - aabb_min.x) / 2.0;
			pa.xz = (aabb_min.xz + aabb_max.xz) / 2.0;
			pb.xz = pa.xz;
			pa.y = aabb_min.y;
			pb.y = aabb_max.y;
		} else { // X is length
			ra = (aabb_max.y - aabb_min.y) / 2.0;
			pa.yz = (aabb_min.yz + aabb_max.yz) / 2.0;
			pb.yz = pa.yz;
			pa.x = aabb_min.x;
			pb.x = aabb_max.x;
		}
	}
	// optionally have a second radius from an additional custom float. If it is positive use it as the B end radius (at aabb_max), if negative use its absolute as the A end radius (at aabb_min).
	// Not using the additional custom float with result in a perfect cylinder
	float rb = customData_rb!=0? CustomData(customData_rb).rb[gl_PrimitiveID] : ra;
	if (rb < 0) {
		float tmp = -rb;
		rb = ra;
		ra = tmp;
	}
	
	// Ray-Cone Intersection without the caps (ro, rd, pa, pb, ra, rb)
	const vec3 ba = pb - pa;
	const vec3 oa = ro - pa;
	const vec3 ob = ro - pb;
	const float m0 = dot(ba,ba);
	const float m1 = dot(oa,ba);
	const float m2 = dot(ob,ba);
	const float m3 = dot(rd,ba);
	const float m4 = dot(rd,oa);
	const float m5 = dot(oa,oa);
	const float rr = ra-rb;
	const float hy = m0 + rr*rr;
	const float k2 = m0*m0 - m3*m3*hy;
	const float k1 = m0*m0*m4 - m1*m3*hy + m0*ra*rr*m3;
	const float k0 = m0*m0*m5 - m1*m1*hy + m0*ra*(rr*m1*2.0 - m0*ra);
	const float h = k1*k1 - k2*k0;
	if (h < 0.0) return;
	const float t1 = (-k1-sqrt(h)) / k2;
	const float t2 = (-k1+sqrt(h)) / k2;
	const float y1 = m1 + m3*t1;
	const float y2 = m1 + m3*t2;
	
	// Outside surface
	if (y1 > 0.0 && y1 < m0) {
		if (gl_RayTminEXT <= t1) {
			coneAttr.ra = ra;
			coneAttr.rb = rb;
			coneAttr.len = length(ba);
			coneAttr.normal = normalize(m0*(m0*(oa+rd*t1)+rr*ba*ra) - ba*hy*y1);
			reportIntersectionEXT(t1, 0);
			return;
		}
	}
	
	// Inside surface
	if (y2 > 0.0 && y2 < m0) {
		if (gl_RayTminEXT <= t2) {
			coneAttr.ra = ra;
			coneAttr.rb = rb;
			coneAttr.len = length(ba);
			coneAttr.normal = -normalize(m0*(m0*(oa+rd*t2)+rr*ba*ra) - ba*hy*y2);
			reportIntersectionEXT(t2, 1);
			return;
		}
	}
}


#############################################################
#shader visibility.rchit

hitAttributeEXT ConeAttr coneAttr;

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
	ray.normal.xyz = coneAttr.normal;
	
	// Store useful information in the UV payload member
	ray.uv = vec2(coneAttr.ra, coneAttr.len);
}

