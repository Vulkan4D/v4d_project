#pragma once
#include <v4d.h>

using namespace v4d::graphics;

class V4DRenderer : public v4d::graphics::Renderer {
	using v4d::graphics::Renderer::Renderer;
	
	// enum RENDER_PIPELINE_TYPE {
	// 	RECORDED,
	// 	DYNAMIC,
	// };
	
	// Rendering pipeline execution in order of dependencies
	enum RENDER_PIPELINE_STEP {
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
		OPAQUE_RASTER, // Write to G-Buffers for deferred rendering of opaque geometries
			/* G-Buffers: vec3(Albedo).rgb, vec3(normal).xyz, vec4(R=roughness, G=metallic, B=scattering, A=occlusion), vec3(emission).rgb, vec3(pos).xyz,  depth+stencil
				Draw all Opaque Blocks
				Draw all Other Opaque Objects (individual surface shaders)
				Draw Planets Solids (terrain, snow, sand, ash, vegetation, ...)
			*/
		OPAQUE_RAYTRACING, // Using RTX extension
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
		TRANSPARENT_RAYTRACING, // Using RTX extension
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
	
	// RenderPass renderPass;
	// PipelineLayout testLayout;
	// RasterShaderPipeline* testShader;

private: // Init
	void ScorePhysicalDeviceSelection(int& score, PhysicalDevice* physicalDevice) override {}
	void Init() override {}
	void Info() override {}

private: // Resources
	void CreateResources() override {}
	void DestroyResources() override {}
	void AllocateBuffers() override {}
	void FreeBuffers() override {}

private: // Graphics configuration
	void CreatePipelines() override {}
	void DestroyPipelines() override {}
	
private: // Commands
	void RecordComputeCommandBuffer(VkCommandBuffer, int imageIndex) override {}
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		TransitionImageLayout(commandBuffer, swapChain->images[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}
	void RecordLowPriorityComputeCommandBuffer(VkCommandBuffer) override {}
	void RecordLowPriorityGraphicsCommandBuffer(VkCommandBuffer) override {}
	void RunDynamicCompute(VkCommandBuffer) override {}
	void RunDynamicGraphics(VkCommandBuffer) override {}
	void RunDynamicLowPriorityCompute(VkCommandBuffer) override {}
	void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {}
	
public: // Scene configuration
	void LoadScene() override {}
	void UnloadScene() override {}
	
public: // Update
	void FrameUpdate(uint imageIndex) override {}
	void LowPriorityFrameUpdate() override {}
	
public: // ubo/conditional member variables
	bool continuousGalaxyGen = true;
	
};
