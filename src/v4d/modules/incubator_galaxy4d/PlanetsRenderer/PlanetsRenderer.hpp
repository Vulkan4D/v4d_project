#pragma once
#include <v4d.h>

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

#include "PlanetTerrain.hpp"
#include "PlanetAtmosphere.hpp"
#include "PlanetTerrainShaderPipeline.hpp"
#include "PlanetAtmosphereShaderPipeline.hpp"

class PlanetsRenderer : public v4d::modules::Rendering {
	
	#pragma region Graphics
	PipelineLayout planetTerrainPipelineLayout;
	PlanetTerrainShaderPipeline planetTerrainShader {planetTerrainPipelineLayout, {
		"modules/incubator_galaxy4d/assets/shaders/planetTerrain.vert",
		"modules/incubator_galaxy4d/assets/shaders/planetTerrain.surface.frag",
	}};
	PipelineLayout planetAtmospherePipelineLayout;
	PlanetAtmosphereShaderPipeline planetAtmosphereShader {planetAtmospherePipelineLayout, {
		"modules/incubator_galaxy4d/assets/shaders/planetAtmosphere.vert",
		"modules/incubator_galaxy4d/assets/shaders/planetAtmosphere.frag",
	}};
	RenderPass	atmospherePass;
	#pragma endregion
	
public:

	int OrderIndex() const override {return 10;}
	
	std::vector<PlanetTerrain*> planetTerrains {};

	// // Executed when calling InitRenderer() on the main Renderer
	// void Init() override {}
	
	void InitLayouts(std::vector<DescriptorSet*>& descriptorSets, std::unordered_map<std::string, Image*>& images) override {
		// auto* planetDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		auto* atmosphereDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		
		planetTerrainPipelineLayout.AddDescriptorSet(descriptorSets[0]);
		// planetTerrainPipelineLayout.AddDescriptorSet(planetDescriptorSet_1);
		planetTerrainPipelineLayout.AddPushConstant<PlanetTerrainShaderPipeline::PlanetChunkPushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		
		atmosphereDescriptorSet_1->AddBinding_inputAttachment(0, &images["gBuffer_position"]->view, VK_SHADER_STAGE_FRAGMENT_BIT);
		planetAtmospherePipelineLayout.AddDescriptorSet(descriptorSets[0]);
		planetAtmospherePipelineLayout.AddDescriptorSet(atmosphereDescriptorSet_1);
		planetAtmospherePipelineLayout.AddPushConstant<PlanetAtmosphereShaderPipeline::PlanetAtmospherePushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		
		if (sizeof(PlanetTerrainShaderPipeline::PlanetChunkPushConstant) > 128) {
			INVALIDCODE("PlanetChunkPushConstant should be no more than 128 bytes. Current size is " << sizeof(PlanetTerrainShaderPipeline::PlanetChunkPushConstant))
		}
		if (sizeof(PlanetAtmosphereShaderPipeline::PlanetAtmospherePushConstant) > 128) {
			INVALIDCODE("PlanetAtmospherePushConstant should be no more than 128 bytes. Current size is " << sizeof(PlanetAtmosphereShaderPipeline::PlanetAtmospherePushConstant))
		}
	}
	
	void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders, v4d::graphics::vulkan::rtx::ShaderBindingTable*) override {
		
		// Terrain
		shaders["opaqueRasterization"].push_back(&planetTerrainShader);
		planetTerrainShader.planets = &planetTerrains;
		planetTerrainShader.AddVertexInputBinding(sizeof(PlanetTerrain::Vertex), VK_VERTEX_INPUT_RATE_VERTEX, PlanetTerrain::Vertex::GetInputAttributes());
		// planetTerrainShader.rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
		// planetTerrainShader.rasterizer.lineWidth = 1;
		// Mesh Topology
		#ifdef PLANETARY_TERRAIN_MESH_USE_TRIANGLE_STRIPS
			planetTerrainShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			planetTerrainShader.inputAssembly.primitiveRestartEnable = VK_TRUE;
		#else
			planetTerrainShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		#endif
		
		// Atmosphere
		planetAtmosphereShader.planets = &planetTerrains;
		planetAtmosphereShader.rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
		planetAtmosphereShader.depthStencilState.depthTestEnable = VK_FALSE;
		planetAtmosphereShader.depthStencilState.depthWriteEnable = VK_FALSE;
		planetAtmosphereShader.AddVertexInputBinding(sizeof(PlanetAtmosphere::Vertex), VK_VERTEX_INPUT_RATE_VERTEX, PlanetAtmosphere::Vertex::GetInputAttributes());
		// planetAtmosphereShader.rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
		// planetAtmosphereShader.rasterizer.lineWidth = 1;
		// Mesh Topology
		#ifdef PLANETARY_ATMOSPHERE_MESH_USE_TRIANGLE_STRIPS
			planetAtmosphereShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			planetAtmosphereShader.inputAssembly.primitiveRestartEnable = VK_TRUE;
		#else
			planetAtmosphereShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		#endif
	}
	
	// // Executed when calling their respective methods on the main Renderer
	void ReadShaders() override {
		planetAtmosphereShader.ReadShaders();
	}
	
	// LightSource sun = {
	// 	POINT_LIGHT,
	// 	{0, -1.496e+11, 0},
	// 	{1,1,1}, // color
	// 	1e24f
	// };
	
	void LoadScene(Scene& scene) override {
		// Sun(s)
		// scene.lightSources["sun"] = &sun;
		
		planetTerrainShader.camera = &scene.camera;
		
		// Planets
		for (auto* planetaryTerrain : planetTerrains) {
			// if (planetaryTerrain->lightIntensity > 0) {
			// 	planetaryTerrain->lightSource = {
			// 		POINT_LIGHT,
			// 		planetaryTerrain->absolutePosition,
			// 		{1,1,1}, // color
			// 		planetaryTerrain->lightIntensity
			// 	};
			// 	// Sun(s)
			// 	scene.lightSources["sun"] = &planetaryTerrain->lightSource;
			// }
			// planetaryTerrain->suns = {&sun};
			planetaryTerrain->cameraPos = glm::dvec3(scene.camera.worldPosition) - planetaryTerrain->absolutePosition;
			for (auto* chunk : planetaryTerrain->chunks) {
				chunk->BeforeRender(renderingDevice, transferQueue);
			}
		}
	}
	
	void UnloadScene(Scene& scene) override {
		// // Planets
		// for (auto* planetaryTerrain : planetTerrains) {
		// 	if (planetaryTerrain->lightIntensity > 0) {
		// 		// Sun(s)
		// 		scene.lightSources["sun"] = nullptr;
		// 		scene.lightSources.erase("sun");
		// 	}
		// }
		
		// Chunks Generator
		if (PlanetTerrain::chunkGenerator) {
			delete PlanetTerrain::chunkGenerator;
			PlanetTerrain::chunkGenerator = nullptr;
		}
	}
	
	// // Executed when calling LoadRenderer()
	// void ScorePhysicalDeviceSelection(int& score, PhysicalDevice*) override {}
	// // after selecting rendering device and queues
	// void Info() override {}
	void CreateResources() {
		for (auto* planetTerrain : planetTerrains) {
			planetTerrain->CreateMaps(renderingDevice);
		}
	}
	void DestroyResources() override {
		for (auto* planetTerrain : planetTerrains) {
			planetTerrain->DestroyMaps(renderingDevice);
		}
	}
	// void AllocateBuffers() override {}
	
	void FreeBuffers() override {
		for (auto* planetaryTerrain : planetTerrains) {
			for (auto* chunk : planetaryTerrain->chunks) {
				chunk->Remove();
			}
		}
		PlanetTerrain::vertexBufferPool.FreePool(renderingDevice);
		PlanetTerrain::indexBufferPool.FreePool(renderingDevice);
		
		// Planets
		for (auto* planetaryTerrain : planetTerrains) {
			if (planetaryTerrain->atmosphere) {
				if (planetaryTerrain->atmosphere->allocated) {
					planetaryTerrain->atmosphere->Free(renderingDevice);
				}
			}
		}
	}
	
	void CreatePipelines(std::unordered_map<std::string, Image*>& images) override {
		planetTerrainPipelineLayout.Create(renderingDevice);
		planetAtmospherePipelineLayout.Create(renderingDevice);
		
		{// Atmosphere pass
			VkAttachmentDescription colorAttachment {};
			// Format
			colorAttachment.format = images["litImage"]->format;
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			// Color
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			// Layout
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			VkAttachmentReference colorAttachmentRef = {
				atmospherePass.AddAttachment(colorAttachment),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			
			VkAttachmentDescription inputAttachment {};
			// Format
			inputAttachment.format = images["gBuffer_position"]->format;
			inputAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			// Color
			inputAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			inputAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			// Layout
			inputAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
			inputAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			VkAttachmentReference inputAttachmentRef = {
				atmospherePass.AddAttachment(inputAttachment),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
			
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
				subpass.inputAttachmentCount = 1;
				subpass.pInputAttachments = &inputAttachmentRef;
			atmospherePass.AddSubpass(subpass);
			
			// Create the render pass
			atmospherePass.Create(renderingDevice);
			atmospherePass.CreateFrameBuffers(renderingDevice, {images["litImage"], images["gBuffer_position"]});
			
			// Shaders
			planetAtmosphereShader.SetRenderPass(images["litImage"], atmospherePass.handle, 0);
			planetAtmosphereShader.AddColorBlendAttachmentState();
			planetAtmosphereShader.CreatePipeline(renderingDevice);
		}
		
	}
	
	void DestroyPipelines() override {
		planetTerrainPipelineLayout.Destroy(renderingDevice);
		planetAtmospherePipelineLayout.Destroy(renderingDevice);
		
		planetAtmosphereShader.DestroyPipeline(renderingDevice);
		atmospherePass.DestroyFrameBuffers(renderingDevice);
		atmospherePass.Destroy(renderingDevice);
	}
	
	// // Rendering methods potentially executed on each frame
	// void RunDynamicGraphicsTop(VkCommandBuffer, std::unordered_map<std::string, Image*>&) override {}
	void RunDynamicGraphicsMiddle(VkCommandBuffer commandBuffer, std::unordered_map<std::string, Image*>& images) override {
		// Atmosphere
		atmospherePass.Begin(renderingDevice, commandBuffer, *images["litImage"]);
		planetAtmosphereShader.Execute(renderingDevice, commandBuffer);
		atmospherePass.End(renderingDevice, commandBuffer);
	}
	// void RunDynamicGraphicsBottom(VkCommandBuffer, std::unordered_map<std::string, Image*>&) override {}
	// void RunDynamicLowPriorityCompute(VkCommandBuffer) override {}
	// void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {}

	#ifdef _ENABLE_IMGUI
		void RunImGui() override {
			for (auto* planet : planetTerrains) {
				ImGui::Separator();
				ImGui::Text("Planet");
				ImGui::Text("Terrain Radius: %d km", (int)planet->solidRadius/1000);
				ImGui::Text("Terrain Diameter: %d km", (int)planet->solidRadius/500);
				if (planet->chunkGenerator) {
					ImGui::Text("Chunk generator queue : %d", (int)planet->chunkGenerator->Count());
					ImGui::Text("Chunks: %d (%d active, %d rendered)", planet->totalChunks, planet->activeChunks, planet->renderedChunks);
				}
				float altitude = (float)planet->cameraAltitudeAboveTerrain;
				if (altitude < 1.0) {
					ImGui::Text("Altitude above terrain: %d mm", (int)std::ceil(altitude*1000.0));
				} else if (altitude < 1000.0) {
					ImGui::Text("Altitude above terrain: %d m", (int)std::ceil(altitude));
				} else {
					ImGui::Text("Altitude above terrain: %d km", (int)std::ceil(altitude/1000.0));
				}
				if (planet->atmosphere) {
					ImGui::Text("Atmosphere");
					ImGui::SliderFloat("density", &planet->atmosphere->densityFactor, 0.0f, 1.0f);
					ImGui::ColorEdit3("color", (float*)&planet->atmosphere->color);
					ImGui::Separator();
				}
				// ImGui::Text("Sun");
				// static glm::vec3 sunPosition = glm::normalize(sun.worldPosition);
				// ImGui::SliderFloat("Intensity", (float*)&sun.intensity, 1e20f, 1e26f);
				// ImGui::ColorEdit3("Position", (float*)&sunPosition);
				// sun.worldPosition = planet->absolutePosition + glm::normalize(glm::dvec3(sunPosition)) * 1.496e+11;
			}
		}
	#endif
	
	// Executed before each frame
	void FrameUpdate(Scene& scene) override {
		scene.camera.worldPosition += glm::dvec3(planetTerrains[0]->matrix * glm::dvec4(0, -12000000, 0, 1));
		scene.camera.RefreshViewMatrix();
		
		planetTerrainShader.viewMatrix = scene.camera.viewMatrix;
		planetAtmosphereShader.viewMatrix = scene.camera.viewMatrix;
		
		// Planets
		for (auto* planetaryTerrain : planetTerrains) {
			std::lock_guard lock(planetaryTerrain->chunksMutex);
			
			// //TODO Planet rotation
			// static v4d::Timer time(true);
			// // planetaryTerrain->rotationAngle = time.GetElapsedSeconds()/1000000000;
			// // planetaryTerrain->rotationAngle = time.GetElapsedSeconds()/30;
			// planetaryTerrain->rotationAngle += 0.0001;
			// planetaryTerrain->RefreshMatrix();
			
			// Camera position relative to planet
			planetaryTerrain->cameraPos = glm::inverse(planetaryTerrain->matrix) * glm::dvec4(scene.camera.worldPosition, 1);
			planetaryTerrain->cameraAltitudeAboveTerrain = glm::length(planetaryTerrain->cameraPos) - planetaryTerrain->GetHeightMap(glm::normalize(planetaryTerrain->cameraPos), 0.5);
			
			for (auto* chunk : planetaryTerrain->chunks) {
				chunk->BeforeRender(renderingDevice, transferQueue);
			}
			
			if (planetaryTerrain->atmosphere) {
				if (!planetaryTerrain->atmosphere->allocated) {
					auto cmdBuffer = renderingDevice->BeginSingleTimeCommands(*transferQueue);
					planetaryTerrain->atmosphere->Allocate(renderingDevice, cmdBuffer);
					renderingDevice->EndSingleTimeCommands(*transferQueue, cmdBuffer);
				}
			}
		}
	}
	
	void LowPriorityFrameUpdate() override {
		for (auto* planetaryTerrain : planetTerrains) {
			std::lock_guard lock(planetaryTerrain->chunksMutex);
			for (auto* chunk : planetaryTerrain->chunks) {
				chunk->Process(renderingDevice, transferQueue);
			}
			planetaryTerrain->Optimize();
		}
		PlanetTerrain::CollectGarbage(renderingDevice);
	}
	
};
