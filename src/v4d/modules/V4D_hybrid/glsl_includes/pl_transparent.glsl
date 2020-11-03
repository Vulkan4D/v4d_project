#include "set0_base.glsl"

#ifdef SHADER_FRAG
	#include "set1_transparent.glsl"
	#include "GeometryPushConstant.glsl"
	layout(location = 0) out vec4 out_img_lit;
#endif
