layout(std430, push_constant) uniform GeometryPushConstant{
	vec4 wireframeColor;
	uint objectIndex;
	uint geometryIndex;
};
