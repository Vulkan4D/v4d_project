// #include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

// #extension GL_ARB_separate_shader_objects : enable
// #extension GL_ARB_shader_viewport_layer_array : enable

layout(std430, push_constant) uniform GalaxyPushConstant {
	float minViewDistance;
	float maxViewDistance;
	float sizeFactor;
	float brightnessFactor;
	vec4 relativePosition;
};
float maxDistanceSqr = maxViewDistance*maxViewDistance;
float minDistanceSqr = minViewDistance*minViewDistance;


#############################################################
#shader vert

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void main() {
	gl_Position = in_position;
	out_color = in_color;
}


#############################################################
#shader geom

layout(points) in;
layout(points, max_vertices = 6) out;
layout(location = 0) in vec4 in_color[];
layout(location = 0) out vec4 out_color;

vec3 pos = relativePosition.xyz + gl_in[0].gl_Position.xyz;
float pointDistanceSqr = dot(pos, pos);
float dd = smoothstep(maxDistanceSqr, minDistanceSqr, pointDistanceSqr);
vec4 pointColor = vec4(in_color[0].rgb, in_color[0].a * dd*dd * brightnessFactor);
float pointSize = max(1, gl_in[0].gl_Position.w * dd) * sizeFactor;

void EmmitStar(int layer, vec2 coord) {
	gl_Layer = layer;
	gl_Position = vec4(coord,0,1);
	gl_PointSize = pointSize * (tan(length(coord.xy)/sqrt(2.0))+1.0);
	out_color = pointColor;
	EmitVertex();
}

void main(void) {
	if (pointDistanceSqr > maxDistanceSqr || pointDistanceSqr < minDistanceSqr)
		return;
	
	vec3 pos = pos;
	
	if (pos.x > 0 && abs(pos.z) <= pos.x && abs(pos.y) <= pos.x)
		EmmitStar(0, vec2(-pos.z / pos.x, -pos.y / pos.x));
	else if (-pos.x > 0 && abs(pos.z) <= -pos.x && abs(pos.y) <= -pos.x)
		EmmitStar(1, vec2(-pos.z / pos.x, pos.y / pos.x));
	else if (pos.y > 0 && abs(pos.z) <= pos.y && abs(pos.x) <= pos.y)
		EmmitStar(2, vec2(pos.x / pos.y, pos.z / pos.y));
	else if (-pos.y > 0 && abs(pos.z) <= -pos.y && abs(pos.x) <= -pos.y)
		EmmitStar(3, vec2(-pos.x / pos.y, pos.z / pos.y));
	else if (pos.z > 0 && abs(pos.x) <= pos.z && abs(pos.y) <= pos.z)
		EmmitStar(4, vec2(pos.x / pos.z, -pos.y / pos.z));
	else if (-pos.z > 0 && abs(pos.x) <= -pos.z && abs(pos.y) <= -pos.z)
		EmmitStar(5, vec2(pos.x / pos.z, pos.y / pos.z));
}


#############################################################
#shader frag

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void main() {
	float center = max(0, 1.0 - pow(length(gl_PointCoord * 2 - 1), 1.0 / max(0.7, in_color.a)));
	out_color = vec4(in_color.rgb * in_color.a, in_color.a) * center;
}
