#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"
#include "../V4D_hybrid/Texture2D.hpp"

std::array<Texture2D, NB_BLOCKS> blocks_tex {
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/0.png"), STBI_rgb_alpha},
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/1.png"), STBI_rgb_alpha},
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/2.png"), STBI_rgb_alpha},
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/3.png"), STBI_rgb_alpha},
	Texture2D {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/blocks/4.png"), STBI_rgb_alpha},
};
std::array<ImTextureID, NB_BLOCKS> blocks_imGuiImg {};

v4d::scene::Scene* scene = nullptr;
CachedData* cachedData = nullptr;

BuildInterface buildInterface {};

V4D_MODULE_CLASS(V4D_Game) {
	
	V4D_MODULE_FUNC(void*, ModuleGetCustomPtr, int) {
		return &buildInterface;
	}
	
	V4D_MODULE_FUNC(void, Init, v4d::scene::Scene* _s) {
		scene = _s;
		buildInterface.scene = scene;
		cachedData = (CachedData*)V4D_Objects::LoadModule(THIS_MODULE)->ModuleGetCustomPtr(0);
		
		static const int maxObjectsInMemory = 10; // 256 objects = 229 Mb
		static const int maxBlocksInMemory = maxObjectsInMemory * 1024;
		v4d::scene::Geometry::globalBuffers.objectBuffer.Extend(maxObjectsInMemory);
		v4d::scene::Geometry::globalBuffers.geometryBuffer.Extend(maxObjectsInMemory);
		v4d::scene::Geometry::globalBuffers.vertexBuffer.Extend(maxBlocksInMemory * Block::MAX_VERTICES);
		v4d::scene::Geometry::globalBuffers.indexBuffer.Extend(maxBlocksInMemory * Block::MAX_INDICES);
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
		ImGui::SetNextWindowPos({20, 150}, ImGuiCond_FirstUseEver); // or ImGuiCond_Once
		ImGui::SetNextWindowSize({380, 160}, ImGuiCond_FirstUseEver);
		ImGui::Begin("Build System");
		for (int i = 0; i < NB_BLOCKS; ++i) {
			if (!blocks_imGuiImg[i]) blocks_imGuiImg[i] = (ImTextureID)ImGui_ImplVulkan_AddTexture(blocks_tex[i].GetImage()->sampler, blocks_tex[i].GetImage()->view, VK_IMAGE_LAYOUT_GENERAL);
			ImGui::SetCursorPos({float(i) * 74 + 2, 24});
			if (ImGui::ImageButton(blocks_imGuiImg[i], {64, 64}, {0,0}, {1,1}, -1, buildInterface.selectedBlockType==i?ImVec4{0,0.5,0,1}:ImVec4{0,0,0,1}, buildInterface.selectedBlockType==-1?ImVec4{1,1,1,1}:ImVec4{1,1,1,0.8})) {
				buildInterface.selectedBlockType = i;
				buildInterface.RemakeTmpBlock();
			}
		}
		if (buildInterface.selectedBlockType != -1) {
			auto activeColor = ImVec4{0,1,0, 1.0};
			auto inactiveColor = ImVec4{1,1,1, 0.6};
			
			ImGui::SetCursorPos({5, 100});
			ImGui::SetNextItemWidth(90);
			ImGui::PushStyleColor(ImGuiCol_Text, buildInterface.selectedEditValue==0? activeColor : inactiveColor);
			if (ImGui::InputFloat("X", &buildInterface.blockSize[buildInterface.selectedBlockType][0], 0.1f, 1.0f, 1, ImGuiInputTextFlags_None)) {
				buildInterface.RemakeTmpBlock();
			}
			ImGui::PopStyleColor();
			
			ImGui::SetCursorPos({135, 100});
			ImGui::SetNextItemWidth(90);
			ImGui::PushStyleColor(ImGuiCol_Text, buildInterface.selectedEditValue==1? activeColor : inactiveColor);
			if (ImGui::InputFloat("Y", &buildInterface.blockSize[buildInterface.selectedBlockType][1], 0.1f, 1.0f, 1, ImGuiInputTextFlags_None)) {
				buildInterface.RemakeTmpBlock();
			}
			ImGui::PopStyleColor();
			
			ImGui::SetCursorPos({260, 100});
			ImGui::SetNextItemWidth(90);
			ImGui::PushStyleColor(ImGuiCol_Text, buildInterface.selectedEditValue==2? activeColor : inactiveColor);
			if (ImGui::InputFloat("Z", &buildInterface.blockSize[buildInterface.selectedBlockType][2], 0.1f, 1.0f, 1, ImGuiInputTextFlags_None)) {
				buildInterface.RemakeTmpBlock();
			}
			ImGui::PopStyleColor();
			
			ImGui::SetCursorPos({5, 125});
			ImGui::PushStyleColor(ImGuiCol_Text, buildInterface.selectedEditValue==3? activeColor : inactiveColor);
			if (ImGui::Button("Rotate")) {
				buildInterface.NextBlockRotation();
				buildInterface.RemakeTmpBlock();
			}
			ImGui::PopStyleColor();
		}
		ImGui::End();
	}
	
	V4D_MODULE_FUNC(void, LoadScene) {}
	
	V4D_MODULE_FUNC(void, UnloadScene) {
		buildInterface.UnloadScene();
	}
	
	V4D_MODULE_FUNC(void, RendererFrameUpdate) {
		std::lock_guard lock(buildInterface.mu);
		buildInterface.UpdateTmpBlock();
		buildInterface.hitBlock = std::nullopt;
		buildInterface.hitBuild = nullptr;
	}
	
	V4D_MODULE_FUNC(void, RendererRayCast, v4d::graphics::RenderRayCastHit hit) {
		std::scoped_lock lock(buildInterface.mu, cachedData->objectMapsMutex);
		try {
			auto build = cachedData->builds.at(hit.objId);
			buildInterface.hitBuild = build;
			buildInterface.hitBlock = hit;
		} catch(...){}
	}
	
};
