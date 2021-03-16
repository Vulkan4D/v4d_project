#include "core.glsl"

#################################################################################
#shader vert

void main() {
	gl_Position = vec4(0,0,0,0);
	gl_PointSize = 20;
}

#################################################################################
#shader frag

layout(location = 0) out vec4 out_color;

const vec4 color = vec4(0,1,1, 0.5);

void main() {
	float center = length(1.0-abs(gl_PointCoord-0.5)*2.5);
	out_color = color * (pow(center, 8) - pow(center, 20)/10);
}
