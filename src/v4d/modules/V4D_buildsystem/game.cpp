#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"
#include "../V4D_hybrid/Texture2D.hpp"

const int NB_BLOCKS = 5;

std::array<Texture2D, NB_BLOCKS> blocks_tex {
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/0.png"), STBI_rgb_alpha},
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/1.png"), STBI_rgb_alpha},
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/2.png"), STBI_rgb_alpha},
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/3.png"), STBI_rgb_alpha},
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/4.png"), STBI_rgb_alpha},
};
std::array<ImTextureID, NB_BLOCKS> blocks_imGuiImg {};

int selectedBlockType = -1;

v4d::scene::Scene* scene = nullptr;

V4D_MODULE_CLASS(V4D_Game) {
	
	V4D_MODULE_FUNC(void, Init, v4d::scene::Scene* _s) {
		scene = _s;
	}
	
	V4D_MODULE_FUNC(void, RendererCreateResources, v4d::graphics::vulkan::Device* device) {
		for (auto& tex : blocks_tex) {
			tex.AllocateAndWriteStagingMemory(device);
			tex.CreateImage(device, VK_IMAGE_TILING_OPTIMAL);
			auto commandBuffer = device->BeginSingleTimeCommands(device->GetQueue("graphics"));
				tex.CopyStagingBufferToImage(device, commandBuffer);
			device->EndSingleTimeCommands(device->GetQueue("graphics"), commandBuffer);
			tex.FreeStagingMemory(device);
		}
	}
	V4D_MODULE_FUNC(void, RendererDestroyResources, v4d::graphics::vulkan::Device* device) {
		for (int i = 0; i < NB_BLOCKS; ++i) {
			if (blocks_imGuiImg[i]) {
				// ImGui_ImplVulkan_RemoveTexture(blocks_imGuiImg[i]); // Descriptor pool is already destroyed here, thus this line does not work... maybe find another way, or is it even necessary to free old descriptor sets?...
				blocks_imGuiImg[i] = nullptr;
			}
			blocks_tex[i].DestroyImage(device);
		}
	}
	
	V4D_MODULE_FUNC(void, RendererRunUi) {
		ImGui::Begin("Build System");
		for (int i = 0; i < NB_BLOCKS; ++i) {
			if (!blocks_imGuiImg[i]) blocks_imGuiImg[i] = (ImTextureID)ImGui_ImplVulkan_AddTexture(blocks_tex[i].GetImage()->sampler, blocks_tex[i].GetImage()->view, VK_IMAGE_LAYOUT_GENERAL);
			ImGui::SetCursorPos({float(i) * 74 + 2, 24});
			if (ImGui::ImageButton(blocks_imGuiImg[i], {64, 64}, {0,0}, {1,1}, -1, selectedBlockType==i?ImVec4{0,0.5,0,1}:ImVec4{0,0,0,1}, selectedBlockType==-1?ImVec4{1,1,1,1}:ImVec4{1,1,1,0.8})) {
				selectedBlockType = i;
			}
		}
		ImGui::End();
	}
	
	
	
	V4D_MODULE_FUNC(void, LoadScene) {
		scene->Lock();
			scene->AddObjectInstance()->Configure([](v4d::scene::ObjectInstance* obj){
				obj->rigidbodyType = v4d::scene::ObjectInstance::RigidBodyType::STATIC;
				
				std::vector<SHAPE> shapes = {
					SHAPE::CUBE,
					SHAPE::INVCORNER,
					SHAPE::SLOPE,
					SHAPE::CORNER,
					SHAPE::PYRAMID,
				};
				
				auto geom1 = obj->AddGeometry("transparent", Block::MAX_VERTICES*shapes.size(), Block::MAX_INDICES*shapes.size());
				
				uint nextVertex = 0;
				uint nextIndex = 0;
				for (int i = 0; i < shapes.size(); ++i) {
					Block block(shapes[i]);
					block.SetPosition({2.0f * i, 0.0f, 0.0f});
					block.SetColors(false, 1, 2, 3, 4, 5, 6);
					auto[vertexCount, indexCount] = block.GenerateGeometry(geom1->GetVertexPtr(nextVertex), geom1->GetIndexPtr(nextIndex), nextVertex, 0.6);
					nextVertex += vertexCount;
					nextIndex += indexCount;
				}
				
				// This is temporary for testing purposes. We need a geom1->Shrink() function to deallocate the unused end of the buffer.
				geom1->vertexCount = nextVertex;
				geom1->indexCount = nextIndex;
				
				for (int i = 0; i < geom1->vertexCount; ++i) {
					auto* vert = geom1->GetVertexPtr(i);
					geom1->boundingDistance = glm::max(geom1->boundingDistance, glm::length(vert->pos));
					geom1->boundingBoxSize = glm::max(glm::abs(vert->pos), geom1->boundingBoxSize);
				}
				geom1->isDirty = true;
				
			}, {-4,4,-1}, 180.0, {1,0,0});
		scene->Unlock();
	}
	
	
	
};
