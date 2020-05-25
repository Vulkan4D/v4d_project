// This file is included in GLSL and C++ sources

// maximum of 32 render options
#define RENDER_OPTION_RAY_TRACED_VISIBILITY 1<< 1
#define RENDER_OPTION_RAY_TRACED_LIGHTING 1<< 2
#define RENDER_OPTION_HARD_SHADOWS 1<< 3
#define RENDER_OPTION_TXAA 1<< 4
#define RENDER_OPTION_HDR_TONE_MAPPING 1<< 5
#define RENDER_OPTION_GAMMA_CORRECTION 1<< 6

// maximum of 32 debug options
#define DEBUG_OPTION_WIREFRAME 1<< 1
#define DEBUG_OPTION_PHYSICS 1<< 2


// C++ Only, within this module only
#ifdef V4D_HYBRID_RENDERER_MODULE
namespace RENDER_OPTIONS {
	bool RAY_TRACED_VISIBILITY = true;
	bool RAY_TRACED_LIGHTING = true;
	bool HARD_SHADOWS = true;
	bool TXAA = true;
	bool HDR_TONE_MAPPING = true;
	bool GAMMA_CORRECTION = true;
	
	uint32_t Get() {
		uint32_t flags = 0;
		if (RAY_TRACED_VISIBILITY) flags |= RENDER_OPTION_RAY_TRACED_VISIBILITY;
		if (RAY_TRACED_LIGHTING) flags |= RENDER_OPTION_RAY_TRACED_LIGHTING;
		if (HARD_SHADOWS) flags |= RENDER_OPTION_HARD_SHADOWS;
		if (TXAA) flags |= RENDER_OPTION_TXAA;
		if (HDR_TONE_MAPPING) flags |= RENDER_OPTION_HDR_TONE_MAPPING;
		if (GAMMA_CORRECTION) flags |= RENDER_OPTION_GAMMA_CORRECTION;
		return flags;
	}
}
namespace DEBUG_OPTIONS {
	bool WIREFRAME = false;
	bool PHYSICS = false;
	
	uint32_t Get() {
		uint32_t flags = 0;
		if (WIREFRAME) flags |= DEBUG_OPTION_WIREFRAME;
		if (PHYSICS) flags |= DEBUG_OPTION_PHYSICS;
		return flags;
	}
}
#endif
