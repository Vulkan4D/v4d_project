#include "set0.glsl"

#ifdef SHADER_FRAG
	layout(set = 1, binding = 0) uniform sampler2D tex_img_depth;
	layout(location = 0) out vec4 out_img_lit;
#endif
