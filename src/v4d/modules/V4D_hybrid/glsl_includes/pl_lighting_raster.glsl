#define GEOMETRY_BUFFERS_ACCESS readonly
#include "set0_base.glsl"

#ifdef SHADER_FRAG
	#include "set1_lighting_and_fog.glsl"
	layout(location = 0) out vec4 out_img_lit;
#endif
