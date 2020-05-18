#include "set0_base.glsl"

#ifdef SHADER_FRAG
	#include "set1_post.glsl"

	#ifdef SHADER_SUBPASS_0
		layout(location = 0) out vec4 out_img_pp;
	#endif
	#ifdef SHADER_SUBPASS_1
		layout(location = 0) out vec4 out_img_history;
	#endif
	#ifdef SHADER_SUBPASS_2
		layout(location = 0) out vec4 out_swapchain;
	#endif
#endif
