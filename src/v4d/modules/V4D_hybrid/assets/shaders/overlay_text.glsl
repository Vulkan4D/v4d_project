#include "core.glsl"

#shader vert

layout(location = 0) in vec4 text;

layout(location = 0) out vec4 out_color;
layout(location = 1) out float out_size;
layout(location = 2) out uint out_flags;
layout(location = 3) out uint out_char;

void main() {
	gl_Position = vec4(text.x, text.y, 0, 1);
	
	out_color = UnpackColorFromFloat(text.z);
	uint data = floatBitsToUint(text.w);
	out_size = float((data & 0xffff0000) >> 16);
	out_flags = (data & 0x0000ff00) >> 8;
	out_char = (data & 0x000000ff);
	gl_PointSize = out_size;
}

#shader frag

layout(location = 0) in vec4 in_color;
layout(location = 1) in float in_size;
layout(location = 2) in flat uint in_flags;
layout(location = 3) in flat uint in_char;

layout(location = 0) out vec4 out_color;

void main() {
	out_color = in_color;
	// in_size
	// in_flags
	// in_char
}
