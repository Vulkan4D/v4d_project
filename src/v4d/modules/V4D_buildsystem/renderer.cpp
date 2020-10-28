#include "common.hh"

using namespace v4d::scene;
using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::vulkan::rtx;

RasterShaderPipeline* blocksRasterShader = nullptr;

Renderer* r = nullptr;
Scene* scene = nullptr;
V4D_Renderer* mainRenderModule = nullptr;

V4D_MODULE_CLASS(V4D_Renderer) {
	
	V4D_MODULE_FUNC(void, ModuleUnload) {
		if (blocksRasterShader) delete blocksRasterShader;
	}
	
	V4D_MODULE_FUNC(void, Init, Renderer* _r, Scene* _s) {
		r = _r;
		scene = _s;
		mainRenderModule = V4D_Renderer::GetPrimaryModule();
	}
	
	V4D_MODULE_FUNC(void, InitLayouts) {
		auto* rasterVisibilityPipelineLayout = mainRenderModule->GetPipelineLayout("pl_visibility_raster");
		
		// Blocks raster shader
		blocksRasterShader = new RasterShaderPipeline(*rasterVisibilityPipelineLayout, {
			"modules/V4D_buildsystem/assets/shaders/blocks.vert",
			"modules/V4D_buildsystem/assets/shaders/blocks.frag",
		});
		mainRenderModule->AddShader("sg_visibility", blocksRasterShader);
	}
	
	V4D_MODULE_FUNC(void, ConfigureShaders) {
		auto* shaderBindingTableVisibility = mainRenderModule->GetShaderBindingTable("sbt_visibility");
		auto* shaderBindingTableLighting = mainRenderModule->GetShaderBindingTable("sbt_lighting");
		
		Geometry::geometryRenderTypes["block"].sbtOffset = 
			shaderBindingTableVisibility->AddHitShader("modules/V4D_buildsystem/assets/shaders/blocks.rchit");
			shaderBindingTableLighting->AddHitShader("modules/V4D_buildsystem/assets/shaders/blocks.rchit");
		Geometry::geometryRenderTypes["block"].rasterShader = blocksRasterShader;
		
		blocksRasterShader->AddVertexInputBinding(sizeof(Geometry::VertexBuffer_T), VK_VERTEX_INPUT_RATE_VERTEX, Geometry::VertexBuffer_T::GetInputAttributes());
		blocksRasterShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
			blocksRasterShader->SetData(&Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer, 0);
		#else
			blocksRasterShader->SetData(&Geometry::globalBuffers.vertexBuffer, &Geometry::globalBuffers.indexBuffer, 0);
		#endif
	}
	
};
