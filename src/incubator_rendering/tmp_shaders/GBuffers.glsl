struct GBuffers {
	highp vec3 albedo;
	highp vec3 normal;
	highp vec3 emission;
	highp vec3 position;
	highp float dist;
	highp float alpha;
	lowp float roughness;
	lowp float metallic;
};
