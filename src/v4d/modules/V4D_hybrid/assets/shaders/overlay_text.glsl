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
	out_char = (data & 0x0000ff00) >> 8;
	out_flags = (data & 0x000000ff);
	gl_PointSize = out_size;
}

#shader frag

#include "v4d/modules/V4D_hybrid/glsl_includes/set1_overlay.glsl"

layout(location = 0) in vec4 in_color;
layout(location = 1) in float in_size;
layout(location = 2) in flat uint in_flags;
layout(location = 3) in flat uint in_char;

layout(location = 0) out vec4 out_color;

void main() {
	// out_color = in_color;
	float charPos = max(0, int(in_char) - 32); // will get a value between 0(space) and 94 (~)
	// The font atlas used is a 10x10 grid
	float grid = 10;
	vec2 coord = vec2(
		(gl_PointCoord.x+0.3)/1.5 / grid + floor(mod(charPos, grid))/grid,
		(gl_PointCoord.y+0.3)/1.7 / grid + floor(charPos/grid)/grid
	);
	float fill = texture(tex_img_font_atlas, coord).r;
	if (fill == 0) discard;
	out_color = in_color * fill;
}
