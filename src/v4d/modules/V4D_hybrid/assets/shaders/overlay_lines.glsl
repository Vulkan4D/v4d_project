#include "core.glsl"

#shader vert

layout(location = 0) in vec4 line;
layout(location = 0) out vec4 out_color;

void main() {
	gl_Position = vec4(line.x, line.y, 0, 1);
	out_color = UnpackColorFromFloat(line.z);
	gl_PointSize = line.w;
}

#shader frag

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void main() {
	out_color = in_color;
}
