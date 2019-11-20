#pragma once

struct VulkanPipelineLayout {
	
	std::vector<VulkanDescriptorSet*> descriptorSets {};
	std::vector<VkDescriptorSetLayout> layouts {};


	std::vector<VkDescriptorSetLayout>* GetDescriptorSetLayouts() {
		return &layouts;
	}

	void AddDescriptorSet(VulkanDescriptorSet* descriptorSet) {
		descriptorSets.push_back(descriptorSet);
	}
	
	void LoadSetLayouts() {
		layouts.clear();
		for (auto* set : descriptorSets) {
			layouts.push_back(set->GetDescriptorSetLayout());
		}
	}
	
	void UnloadSetLayouts() {
		layouts.clear();
	}
	
};
