#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"
#include "noise.glsl"

#shader rcall

layout(location = CALL_DATA_LOCATION_MATERIAL) callableDataInEXT ProceduralMaterialCall mat;
layout(location = CALL_DATA_LOCATION_TEXTURE) callableDataEXT ProceduralTextureCall tex;

void main() {
	Material material = GetGeometry(mat.entityInstanceIndex, mat.geometryIndex).material;
	
	tex.position = mat.position;
	tex.normal = mat.normal;
	tex.color = mat.color * vec4(material.visibility.baseColor) / 255.0;
	tex.metallic = float(material.visibility.metallic) / 255.0;
	tex.roughness = float(material.visibility.roughness) / 255.0;
	tex.emission = material.visibility.emission;
	for (int i = 0; i < 8; ++i) if (material.visibility.texFactors[i] > 0) {
		tex.factor = material.visibility.texFactors[i];
		executeCallableEXT(material.visibility.textures[i], CALL_DATA_LOCATION_TEXTURE);
	}
	
	vec4 color = tex.color;
	float metallic = tex.metallic;
	float roughness = tex.roughness;
	
	if (tex.emission > 0) {
		color *= tex.emission;
	} else {
		
		// 	if (metallic > 0) {
		// 		ray.color.a = 1.0 - FresnelReflectAmount(1.0, indexOfRefraction, normal, gl_WorldRayDirectionEXT, metallic);
		// 		ScatterMetallic(ray, roughness, gl_WorldRayDirectionEXT, normal);
		// 	} else if (color.a < 1) {
		// 		ScatterDieletric(ray, indexOfRefraction, gl_WorldRayDirectionEXT, normal);
		// 	} else {
		// 		ScatterLambertian(ray, roughness, normal);
		// 	}
		
	}
	
	mat.color = color;
	mat.normal.xyz = tex.normal.xyz;
	
	DebugMaterial(mat, tex);
}
