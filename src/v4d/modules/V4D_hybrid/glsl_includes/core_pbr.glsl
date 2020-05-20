// PBR rendering

// settings
const float radianceThreshold = 1e-10;

const float PI = 3.1415926543;

// PBR formulas
float distributionGGX(float NdotH, float roughness) {
	float a = roughness*roughness;
	float a2 = a*a;
	float denom = NdotH*NdotH * (a2 - 1.0) + 1.0;
	denom = PI * denom*denom;
	return a2 / max(denom, 0.0000001);
}
float geometrySmith(float NdotV, float NdotL, float roughness) {
	float r = roughness + 1.0;
	float k = (r*r) / 8.0;
	float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
	float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
	return ggx1 * ggx2;
}
vec3 fresnelSchlick(float HdotV, vec3 baseReflectivity) {
	// baseReflectivity in range 0 to 1
	// returns range baseReflectivity to 1
	// increases as HdotV decreases (more reflectivity when surface viewed at larger angles)
	return baseReflectivity + (1.0 - baseReflectivity) * pow(1.0 - HdotV, 5.0);
}
