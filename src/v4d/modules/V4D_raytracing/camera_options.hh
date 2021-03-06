// This file is included in GLSL and C++ sources

// maximum of 32 render options
// #define RENDER_OPTION_TXAA 1<< 0
#define RENDER_OPTION_GAMMA_CORRECTION 1<< 1
#define RENDER_OPTION_HDR_TONE_MAPPING 1<< 2
#define RENDER_OPTION_SOFT_SHADOWS 1<< 3
#define RENDER_OPTION_PATH_TRACING 1<< 4

// maximum of 32 debug options
#define DEBUG_OPTION_WIREFRAME 1<< 0
#define DEBUG_OPTION_PHYSICS 1<< 1

// Limits
#define MAX_ACTIVE_LIGHTS 16

// Render modes
#define RENDER_MODE_NOTHING 0
#define RENDER_MODE_STANDARD 1
#define RENDER_MODE_NORMALS 2
#define RENDER_MODE_ALBEDO 3
#define RENDER_MODE_EMISSION 4
#define RENDER_MODE_DEPTH 5
#define RENDER_MODE_DISTANCE 6
#define RENDER_MODE_METALLIC 7
#define RENDER_MODE_ROUGNESS 8
#define RENDER_MODE_TIME 9
#define RENDER_MODE_BOUNCES 10

// C++ Only, within this module only
#ifdef V4D_RAYTRACING_RENDERER_MODULE
namespace RENDER_OPTIONS {
	bool SOFT_SHADOWS = false;
	bool PATH_TRACING = false;
	// bool TXAA = false;
	bool HDR_TONE_MAPPING = true;
	bool GAMMA_CORRECTION = true;
	
	uint32_t Get() {
		uint32_t flags = 0;
		if (SOFT_SHADOWS) flags |= RENDER_OPTION_SOFT_SHADOWS;
		if (PATH_TRACING) flags |= RENDER_OPTION_PATH_TRACING;
		// if (TXAA) flags |= RENDER_OPTION_TXAA;
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


// Attributes to use for geometry creation (only one must be used)
#define RAY_TRACED_ENTITY_DEFAULT 1<< 0
#define RAY_TRACED_ENTITY_TERRAIN 1<< 1
#define RAY_TRACED_ENTITY_TERRAIN_NEGATE 1<< 2
#define RAY_TRACED_ENTITY_LIQUID 1<< 3
#define RAY_TRACED_ENTITY_ATMOSPHERE 1<< 4

#define RAY_TRACED_ENTITY_FOG 1<< 5
#define RAY_TRACED_ENTITY_LIGHT 1<< 6
#define RAY_TRACED_ENTITY_SOUND 1<< 7

// Masks to use for ray-tracing calls
#define RAY_TRACE_MASK_VISIBLE			RAY_TRACED_ENTITY_DEFAULT|RAY_TRACED_ENTITY_TERRAIN|RAY_TRACED_ENTITY_TERRAIN_NEGATE|RAY_TRACED_ENTITY_LIQUID|RAY_TRACED_ENTITY_ATMOSPHERE|RAY_TRACED_ENTITY_FOG|RAY_TRACED_ENTITY_LIGHT

