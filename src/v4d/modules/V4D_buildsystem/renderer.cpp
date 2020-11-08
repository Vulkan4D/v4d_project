#include "common.hh"

using namespace v4d::scene;
using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::vulkan::rtx;

RasterShaderPipeline* blockShader = nullptr;
RasterShaderPipeline* crosshairShader = nullptr;

Renderer* r = nullptr;
Scene* scene = nullptr;
V4D_Renderer* mainRenderModule = nullptr;

V4D_MODULE_CLASS(V4D_Renderer) {
	
	V4D_MODULE_FUNC(void, ModuleUnload) {
		if (blockShader) delete blockShader;
		if (crosshairShader) delete crosshairShader;
	}
	
	V4D_MODULE_FUNC(void, Init, Renderer* _r, Scene* _s) {
		r = _r;
		scene = _s;
		mainRenderModule = V4D_Renderer::GetPrimaryModule();
	}
	
	V4D_MODULE_FUNC(void, InitLayouts) {
		
		// Blocks shader
		auto* rasterVisibilityPipelineLayout = mainRenderModule->GetPipelineLayout("pl_visibility_raster");
		blockShader = new RasterShaderPipeline(*rasterVisibilityPipelineLayout, {
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/blocks.vert"),
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/blocks.frag"),
		});
		mainRenderModule->AddShader("sg_visibility", blockShader);
		
		// Crosshair shader
		auto* overlayPipelineLayout = mainRenderModule->GetPipelineLayout("pl_overlay");
		crosshairShader = new RasterShaderPipeline(*overlayPipelineLayout, {
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/crosshair.vert"),
			V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/crosshair.frag"),
		});
		mainRenderModule->AddShader("sg_overlay", crosshairShader);
	}
	
	V4D_MODULE_FUNC(void, ConfigureShaders) {
		auto* shaderBindingTableVisibility = mainRenderModule->GetShaderBindingTable("sbt_visibility");
		auto* shaderBindingTableLighting = mainRenderModule->GetShaderBindingTable("sbt_lighting");
		
		// Blocks
		Geometry::geometryRenderTypes["block"].sbtOffset = 
			shaderBindingTableVisibility->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/blocks.rchit"));
			shaderBindingTableLighting->AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/blocks.rchit"));
		Geometry::geometryRenderTypes["block"].rasterShader = blockShader;
		blockShader->AddVertexInputBinding(sizeof(Geometry::VertexBuffer_T), VK_VERTEX_INPUT_RATE_VERTEX, Geometry::VertexBuffer_T::GetInputAttributes());
		blockShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
			blockShader->SetData(&Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer, 0);
		#else
			blockShader->SetData(&Geometry::globalBuffers.vertexBuffer, &Geometry::globalBuffers.indexBuffer, 0);
		#endif
		
		// Crosshair
		crosshairShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		crosshairShader->depthStencilState.depthTestEnable = VK_FALSE;
		crosshairShader->depthStencilState.depthWriteEnable = VK_FALSE;
		crosshairShader->rasterizer.cullMode = VK_CULL_MODE_NONE;
		crosshairShader->SetData(1);
	}
	
};
