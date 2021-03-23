// #include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

#extension GL_ARB_shader_viewport_layer_array : enable

layout(std430, push_constant) uniform GalaxyPushConstant {
	// used for stars and fade
	float screenSize;
	float brightnessFactor;
	// used only for stars
	float minViewDistance;
	float maxViewDistance;
	vec4 relativePosition; // w = sizeFactor
};

float maxDistanceSqr = maxViewDistance*maxViewDistance;
float minDistanceSqr = minViewDistance*minViewDistance;
float sizeFactor = relativePosition.w;


#############################################################
#shader vert

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 0) out vec4 out_color;

		// vec3 pos = relativePosition.xyz + in_position.xyz;
		// float pointDistanceSqr = dot(pos, pos);
		// float dd = smoothstep(maxDistanceSqr, minDistanceSqr, pointDistanceSqr);
		// vec4 pointColor = vec4(in_color.rgb, in_color.a * dd*dd * brightnessFactor);
		// float pointSize = max(1, in_position.w * dd) * sizeFactor * 2.0;

		// void EmmitStar(int layer, vec2 coord) {
		// 	gl_Layer = layer;
		// 	gl_Position = vec4(coord,0,1);
		// 	gl_PointSize = pointSize * (1.0+sin(dot(coord.xy, coord.xy))); // adjust point size for corners
		// 	out_color = pointColor;
		// }

void main() {
	gl_Position = in_position;
	out_color = in_color;
			
			// if (pointDistanceSqr > maxDistanceSqr || pointDistanceSqr < minDistanceSqr) {
			// 	// Discard this star
			// 	out_color.a = 0;
			// 	gl_PointSize = 0;
			// 	gl_Position = vec4(2,2,2,0);
			// 	return;
			// }
			
			// if (pos.x > 0 && abs(pos.z) <= pos.x && abs(pos.y) <= pos.x)
			// 	EmmitStar(0, vec2(-pos.z / pos.x, -pos.y / pos.x));
			// else if (-pos.x > 0 && abs(pos.z) <= -pos.x && abs(pos.y) <= -pos.x)
			// 	EmmitStar(1, vec2(-pos.z / pos.x, pos.y / pos.x));
			// else if (pos.y > 0 && abs(pos.z) <= pos.y && abs(pos.x) <= pos.y)
			// 	EmmitStar(2, vec2(pos.x / pos.y, pos.z / pos.y));
			// else if (-pos.y > 0 && abs(pos.z) <= -pos.y && abs(pos.x) <= -pos.y)
			// 	EmmitStar(3, vec2(-pos.x / pos.y, pos.z / pos.y));
			// else if (pos.z > 0 && abs(pos.x) <= pos.z && abs(pos.y) <= pos.z)
			// 	EmmitStar(4, vec2(pos.x / pos.z, -pos.y / pos.z));
			// else if (-pos.z > 0 && abs(pos.x) <= -pos.z && abs(pos.y) <= -pos.z)
			// 	EmmitStar(5, vec2(pos.x / pos.z, pos.y / pos.z));
}


#############################################################
#shader geom

layout(points) in;
layout(points, max_vertices = 6) out;
layout(location = 0) in vec4 in_color[];
layout(location = 0) out vec4 out_color;

vec3 pos = relativePosition.xyz + gl_in[0].gl_Position.xyz;
float pointDistanceSqr = dot(pos, pos);
float dd = maxViewDistance==0? 1.0 : clamp(smoothstep(maxDistanceSqr, minDistanceSqr, pointDistanceSqr), 0, 1);
float dd2 = maxViewDistance==0? 0.0 : clamp(smoothstep(minDistanceSqr, 0, pointDistanceSqr), 0, 1);
vec4 pointColor = vec4(in_color[0].rgb, clamp(in_color[0].a * dd*dd * brightnessFactor * (1-dd2*dd2), 0.0, 3.5));

void EmmitStar(int layer, vec2 coord) {
	gl_Layer = layer;
	gl_Position = vec4(coord,0,1);
	gl_PointSize = gl_in[0].gl_Position.w * sizeFactor * (1.5 + sin(dot(coord.xy, coord.xy))); // adjust point size for corners
	out_color = pointColor;
	EmitVertex();
}

void main(void) {
	if (maxViewDistance > 0 && pointDistanceSqr > maxDistanceSqr)
		return;
	if (pointColor.a < 0.001)
		return;
		
	const float threshold = 0.5; // for a point to appear on multiple sides of the cubemap
	
	if (pos.x > 0 && abs(pos.z)-threshold <= pos.x && abs(pos.y)-threshold <= pos.x)
		EmmitStar(0, vec2(-pos.z / pos.x, -pos.y / pos.x));
	if (-pos.x > 0 && abs(pos.z)-threshold <= -pos.x && abs(pos.y)-threshold <= -pos.x)
		EmmitStar(1, vec2(-pos.z / pos.x, pos.y / pos.x));
	if (pos.y > 0 && abs(pos.z)-threshold <= pos.y && abs(pos.x)-threshold <= pos.y)
		EmmitStar(2, vec2(pos.x / pos.y, pos.z / pos.y));
	if (-pos.y > 0 && abs(pos.z)-threshold <= -pos.y && abs(pos.x)-threshold <= -pos.y)
		EmmitStar(3, vec2(-pos.x / pos.y, pos.z / pos.y));
	if (pos.z > 0 && abs(pos.x)-threshold <= pos.z && abs(pos.y)-threshold <= pos.z)
		EmmitStar(4, vec2(pos.x / pos.z, -pos.y / pos.z));
	if (-pos.z > 0 && abs(pos.x)-threshold <= -pos.z && abs(pos.y)-threshold <= -pos.z)
		EmmitStar(5, vec2(pos.x / pos.z, pos.y / pos.z));
}


#############################################################
#shader frag

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void main() {
	if (out_color.a < 0.001) discard;
	float center = max(0, 1.0 - pow(length(gl_PointCoord * 2 - 1), 1.0 / max(0.7, in_color.a)));
	out_color = vec4(in_color.rgb * in_color.a, in_color.a) * pow(center, 4.0-in_color.a);
}


#############################################################
#############################################################

#shader fade.vert
void main() {
	gl_Layer = gl_VertexIndex;
	gl_Position = vec4(0,0,0,1);
	gl_PointSize = screenSize;
}
#shader fade.frag
layout(location = 0) out vec4 out_color;
void main() {
	out_color = vec4(vec3(brightnessFactor * 0.005), 0);
	// out_color = vec4(1,1,1,0); // clear
}
