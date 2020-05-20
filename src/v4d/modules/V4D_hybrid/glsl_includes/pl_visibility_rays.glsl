#include "set0_base.glsl"
#include "set1_visibility_rays.glsl"

Vertex GetVertex(uint index) {
	Vertex v;
	v.pos = vertices[index*2].xyz;
	v.color = UnpackColorFromFloat(vertices[index*2].w);
	v.normal = vertices[index*2+1].xyz;
	v.uv = UnpackUVfromFloat(vertices[index*2+1].w);
	return v;
}

struct ProceduralGeometry {
	GeometryInstance geometryInstance;
	
	uint vertexOffset;
	uint objectIndex;
	uint material;
	
	vec3 aabbMin;
	vec3 aabbMax;
	vec4 color;
	float custom1;
};

ProceduralGeometry GetProceduralGeometry(uint geometryIndex) {
	ProceduralGeometry geom;
	
	geom.geometryInstance = GetGeometryInstance(geometryIndex);
	
	geom.vertexOffset = geom.geometryInstance.vertexOffset;
	geom.objectIndex = geom.geometryInstance.objectIndex;
	geom.material = geom.geometryInstance.material;
	
	geom.aabbMin = vertices[geom.vertexOffset*2].xyz;
	geom.aabbMax = vec3(vertices[geom.vertexOffset*2].w, vertices[geom.vertexOffset*2+1].xy);
	geom.color = UnpackColorFromFloat(vertices[geom.vertexOffset*2+1].z);
	geom.custom1 = vertices[geom.vertexOffset*2+1].w;
	
	return geom;
}

#ifdef SHADER_RGEN

	struct GBuffersPbr {
		vec3 viewSpacePosition;
		vec3 viewSpaceNormal;
		vec3 albedo;
		float emit;
		vec2 uv;
		float metallic;
		float roughness;
		float realDistanceFromCamera;
	} pbrGBuffers;
	
	void WritePbrGBuffers() {
		const ivec2 coords = ivec2(gl_LaunchIDEXT.xy);
		imageStore(img_gBuffer_0, coords, vec4(pbrGBuffers.metallic, pbrGBuffers.roughness, 0, 0));
		imageStore(img_gBuffer_1, coords, vec4(pbrGBuffers.viewSpaceNormal, PackUVasFloat(pbrGBuffers.uv)));
		imageStore(img_gBuffer_2, coords, vec4(pbrGBuffers.viewSpacePosition, pbrGBuffers.realDistanceFromCamera));
		imageStore(img_gBuffer_3, coords, vec4(pbrGBuffers.albedo, pbrGBuffers.emit));
	}
	
	void WriteDepthFromHitDistance(float realDistanceFromCamera) {
		float depth = float(GetDepthBufferFromTrueDistance(realDistanceFromCamera));
		imageStore(img_depth, ivec2(gl_LaunchIDEXT.xy), vec4(depth, 0,0,0));
	}
	
#endif
