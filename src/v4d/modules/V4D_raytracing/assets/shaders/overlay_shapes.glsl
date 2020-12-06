#include "core.glsl"

#shader vert

layout(location = 0) in vec4 shape;

layout(location = 0) out vec4 out_color;
layout(location = 1) out uvec2 out_size;
layout(location = 2) out uint out_border;

void main() {
	gl_Position = vec4(shape.x, shape.y, 0, 1);
	out_color = UnpackColorFromFloat(shape.z);
	uint data = floatBitsToUint(shape.w);
	out_size = uvec2((data & 0xfff00000) >> 20, (data & 0x000fff00) >> 8);
	out_border = (data & 0x000000ff);
	gl_PointSize = max(float(out_size.x), float(out_size.y));
}

#shader square.frag

layout(location = 0) in vec4 in_color;
layout(location = 1) in flat uvec2 in_size;
layout(location = 2) in flat uint in_border;

layout(location = 0) out vec4 out_color;

float FragCoordWithinSquare(vec2 size, float margins) {
	float ratio = size.x / size.y;
	float xcoord;
	float ycoord;
	if (ratio > 1) {
		xcoord = abs(gl_PointCoord.x-0.5) * size.x < size.x/2 - margins ? 1 : 0;
		ycoord = abs(gl_PointCoord.y-0.5) * size.y * ratio < size.y/2 - margins ? 1 : 0;
	} else {
		xcoord = abs(gl_PointCoord.x-0.5) * size.x / ratio < size.x/2 - margins ? 1 : 0;
		ycoord = abs(gl_PointCoord.y-0.5) * size.y < size.y/2 - margins ? 1 : 0;
	}
	return xcoord * ycoord;
}

void main() {
	float insideSquareWithContour = FragCoordWithinSquare(vec2(in_size), 0);
	float insideFill = FragCoordWithinSquare(vec2(in_size), float(in_border));
	float fill = mix(insideSquareWithContour, insideSquareWithContour - insideFill, sign(in_border));
	out_color = in_color * fill;
}

#shader circle.frag

layout(location = 0) in vec4 in_color;
layout(location = 1) in flat uvec2 in_size;
layout(location = 2) in flat uint in_border;

layout(location = 0) out vec4 out_color;

void main() {
	float center = 1.0 - length(gl_PointCoord * 2 - 1);
	float fill = sign(center) - sign(in_border)*step(float(in_border+1.0)/in_size.x, center);
	out_color = vec4(clamp(in_color.rgb, vec3(0), vec3(1)), in_color.a) * clamp(fill, 0, 1);
}
