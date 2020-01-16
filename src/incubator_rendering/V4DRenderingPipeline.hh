#pragma once

namespace v4d::graphics {

	struct GalaxyGenPushConstant {
		glm::dvec3 cameraPosition; // 24
		int frameIndex; // 4
		int resolution; // 4
	};
	
	struct GalaxyBoxPushConstant {
		glm::dmat4 inverseProjectionView {1}; // 128
	};
	
	// struct StandardPushConstant {
	// 	glm::
	// };

	// enum RENDER_PIPELINE_TYPE {
	// 	RECORDED,
	// 	DYNAMIC,
	// };

	// Rendering pipeline execution in order of dependencies
	enum class RENDER_PIPELINE_STEP {
		// low-priority queue, async
		UI, // Write to UI image (RGBA8)
			/*
				all UI elements rendered in a separate image with transparent background
			*/
		COMPUTE, // Low-priority compute programs
			/*
				Input thumbnail image from previously rendered frame and Compute necessary tone mapping adjustments
				Generate stars buffers
			*/
		GALAXY, // Write to Galaxy CubeMap and Depth CubeMap
			/*
				Draw stars (no depth testing, only depth writing)
				Draw galaxy clouds (with depth testing and writing)
				Fade in a second pass
			*/
		
		// main graphics queue
		OPAQUE_RASTER, // Write to G-Buffers+depth/stencil for deferred rendering of opaque geometries
			/* 
				Draw all Opaque Blocks
				Draw all Other Opaque Objects (individual surface shaders)
				Draw Planets Solids (terrain, snow, sand, ash, vegetation, ...)
			*/
		OPAQUE_RAYTRACING, // Using RTX extension, write to rayTracing image
			/*
				Trace Shadows
				Trace reflections
			*/
		OPAQUE_LIGHTING, // Input G-Buffers, Write to tmp RGBA32 color attachment (a=1.0)
			/*
				Sample G-Buffers, Apply Lighting (shading, shadows and opaque reflections)
				Sample Galaxy CubeMap where depth buffer is clear (1.0)
				Draw Sun Flares
				Draw Atmospheric effects and Fog
			*/
		TRANSPARENT_RASTER, // Clear and Write new G-Buffers, Also Write to tmp RGBA32 color attachment with blending, unculled rendering, depth testing but not writing
			/*
				Draw Planet Transparents (liquids, atmospheres, plasma, ...)
				Draw Transparent block surfaces
				Draw all Other transparent objects (individual surface shaders)
				Draw FX (dust, particles, fire, lightning, electricity, lasers, ...)
			*/
		TRANSPARENT_RAYTRACING, // Using RTX extension, write to rayTracing image
			/*
				Trace reflections on transparent surfaces
				Trace atmospheric scattering
			*/
		TRANSPARENT_LIGHTING, // Write to tmp RGBA32 color attachment with blending
			/*
				Sample G-Buffers, Apply Lighting and reflections for transparent surfaces
			*/
		THUMBNAIL, // Asynchronously Blit to thumbnail image
			/*
				Blit tmp color attachment to a 1/8 thumbnail RGBA32 for computing required tone mapping later in the COMPUTE step
			*/
		POSTPROCESSING, // Input tmp color attachment and UI image, Write directly to swapchain image
			/*
				Screen-space reflections
				Other post-processing effects
				Bloom effects
				HDR Tone-Mapping & Gamma Corrections
				Apply UI on top with alpha blending
			*/
	};
}
