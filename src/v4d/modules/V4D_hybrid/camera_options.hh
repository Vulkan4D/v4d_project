// This file is included in GLSL and C++ sources

// maximum of 32 render options
#define RENDER_OPTION_RAY_TRACED_VISIBILITY 0x00000001
#define RENDER_OPTION_RAY_TRACED_LIGHTING 0x00000002
#define RENDER_OPTION_HARD_SHADOWS 0x00000004
#define RENDER_OPTION_TXAA 0x00000008
#define RENDER_OPTION_HDR_TONE_MAPPING 0x00000010
#define RENDER_OPTION_GAMMA_CORRECTION 0x00000020

// maximum of 32 debug options
#define DEBUG_OPTION_WIREFRAME 0x00000001


// C++ Only, within a module
#ifdef _V4D_MODULE
namespace RENDER_OPTIONS {
	bool RAY_TRACED_VISIBILITY = false;
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
	
	uint32_t Get() {
		uint32_t flags = 0;
		if (WIREFRAME) flags |= DEBUG_OPTION_WIREFRAME;
		return flags;
	}
}
#endif
