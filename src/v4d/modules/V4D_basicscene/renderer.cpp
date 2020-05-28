#define _V4D_MODULE
#include <v4d.h>

using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::vulkan::rtx;

// PipelineLayout testAabbLayout;
// ShaderBindingTable testAabb{testAabbLayout, "modules/V4D_basicscene/assets/shaders/testAabb.rgen"};

V4D_MODULE_CLASS(V4D_Renderer) {
	
	V4D_MODULE_FUNC(void, Init, v4d::graphics::Renderer*, v4d::scene::Scene*) {
		LOG("It Works !!!")
	}
	V4D_MODULE_FUNC(void, LoadScene) {
		
	}
	V4D_MODULE_FUNC(void, UnloadScene) {
		
	}
	
};
