#pragma once

#include "VulkanStructs.hpp"
#include "VulkanDevice.hpp"
#include "VulkanGraphicsPipeline.hpp"

class VulkanRenderPass {
private:
	VulkanDevice *device;

public:
	VkRenderPassCreateInfo renderPassInfo = {};
	vector<VkSubpassDescription> subpasses;
	vector<VkAttachmentDescription> attachments; // This struct defines the output data from the fragment shader (o_color)
	VkRenderPass handle;
	vector<VulkanGraphicsPipeline*> graphicsPipelines;

	VulkanRenderPass(VulkanDevice *device) : device(device) {
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

	}

	~VulkanRenderPass() {
		vkDestroyRenderPass(device->GetHandle(), handle, nullptr);
	}

	void Create() {
		renderPassInfo.attachmentCount = attachments.size();
		renderPassInfo.pAttachments = attachments.data();

		renderPassInfo.subpassCount = subpasses.size();
		renderPassInfo.pSubpasses = subpasses.data();

		if (vkCreateRenderPass(device->GetHandle(), &renderPassInfo, nullptr, &handle) != VK_SUCCESS) {
			throw runtime_error("Failed to create render pass!");
		}
	}

	void AddSubpass(VkSubpassDescription &subpass) {
		subpasses.push_back(subpass);
	}

	void AddAttachment(VkAttachmentDescription &attachment) {
		attachments.push_back(attachment);
	}

	void AddGraphicsPipeline(VulkanGraphicsPipeline *graphicsPipeline, uint32_t subpass = 0) {
		// Render passes 
		// The reference to the render pass and the index of the sub pass where this graphics pipeline will be used. 
		// It is also possible to use other render passes with this pipeline instead of this specific instance, but they have to be Compatible with renderPass.
		graphicsPipeline->pipelineCreateInfo.renderPass = handle;
		graphicsPipeline->pipelineCreateInfo.subpass = subpass;

		graphicsPipelines.push_back(move(graphicsPipeline));
	}

	void CreateGraphicsPipelines() {
		vector<VkGraphicsPipelineCreateInfo> pipelineCreates(graphicsPipelines.size());
		for (size_t i = 0; i < graphicsPipelines.size(); i++) {
			pipelineCreates[i] = ref(graphicsPipelines[i]->pipelineCreateInfo);
		}

		auto* pipelineHandles = new vector<VkPipeline>(graphicsPipelines.size());
		
		// Now create the graphics pipeline object
		// This function is designed to take multiple VkGraphicsPipelineCreateInfo objects and create multiple VkPipeline objects in a single call.
		if (vkCreateGraphicsPipelines(device->GetHandle(), VK_NULL_HANDLE/*pipelineCache*/, pipelineCreates.size(), pipelineCreates.data(), nullptr, pipelineHandles->data()) != VK_SUCCESS) {
			throw runtime_error("Failed to create Graphics Pipeline");
		}

		for (size_t i = 0; i < graphicsPipelines.size(); i++) {
			graphicsPipelines[i]->handle = ref((*pipelineHandles)[i]);
		}

		delete pipelineHandles;
	}

	void DestroyGraphicsPipelines() {
		for (auto* gp : graphicsPipelines)
			delete gp;
		graphicsPipelines.clear();
	}

};
