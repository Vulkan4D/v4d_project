#pragma once

#include "VulkanStructs.hpp"
#include "VulkanDevice.hpp"
#include "VulkanShader.hpp"

class VulkanGraphicsPipeline {
private:
	VulkanDevice* device;

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;

public:
	VkPipeline handle = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo {};
	
	VkPipelineRasterizationStateCreateInfo rasterizer {};
	VkPipelineMultisampleStateCreateInfo multisampling {};
	VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
	VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
	VkPipelineColorBlendStateCreateInfo colorBlending {};
	VkPipelineDepthStencilStateCreateInfo depthStencilState;
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo {};
	std::vector<VkDynamicState> dynamicStates;

	VulkanGraphicsPipeline(VulkanDevice* device) : device(device) {
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

		colorBlending.logicOpEnable = VK_FALSE; // If enabled, will effectively replace and disable blendingAttachmentState.blendEnable
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // optional, if enabled above
		colorBlending.blendConstants[0] = 0; // optional
		colorBlending.blendConstants[1] = 0; // optional
		colorBlending.blendConstants[2] = 0; // optional
		colorBlending.blendConstants[3] = 0; // optional
		
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.pNext = nullptr;
		depthStencilState.flags = 0;
	}

	void Prepare() {

		// Bindings and Attributes
		vertexInputInfo.vertexBindingDescriptionCount = bindings.size();
		vertexInputInfo.pVertexBindingDescriptions = bindings.data();
		vertexInputInfo.vertexAttributeDescriptionCount = attributes.size();
		vertexInputInfo.pVertexAttributeDescriptions = attributes.data();

		// Pipeline Layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		if (descriptorSetLayouts.size() > 0) {
			pipelineLayoutInfo.setLayoutCount = descriptorSetLayouts.size();
			pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
		}

		// Dynamic states
		if (dynamicStates.size() > 0) {
			dynamicStateCreateInfo.dynamicStateCount = (uint)dynamicStates.size();
			dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();
			pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
		} else {
			pipelineCreateInfo.pDynamicState = nullptr;
		}
		
		//TODO
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

		if (device->CreatePipelineLayout(&pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create pipeline layout");
		}
		pipelineCreateInfo.layout = pipelineLayout;

		// Fixed functions
		pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
		pipelineCreateInfo.pRasterizationState = &rasterizer;
		pipelineCreateInfo.pMultisampleState = &multisampling;
		colorBlending.attachmentCount = colorBlendAttachments.size();
		colorBlending.pAttachments = colorBlendAttachments.data();
		pipelineCreateInfo.pColorBlendState = &colorBlending;

		// Shaders
		pipelineCreateInfo.stageCount = shaderStages.size();
		pipelineCreateInfo.pStages = shaderStages.data();
	}

	void Create() {
		if (device->CreateGraphicsPipelines(VK_NULL_HANDLE/*pipelineCache*/, 1, &pipelineCreateInfo, nullptr, &handle) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Graphics Pipeline");
		}
	}

	~VulkanGraphicsPipeline() {
		device->DestroyPipeline(handle, nullptr);
		device->DestroyPipelineLayout(pipelineLayout, nullptr);
	}

	void AddShaderStage(VulkanShader* shader) {
		shaderStages.push_back(shader->stageInfo);
	}

	void SetShaderProgram(VulkanShaderProgram* shaderProgram) {
		shaderStages = ref(shaderProgram->GetStages());
		bindings = ref(shaderProgram->GetBindings());
		attributes = ref(shaderProgram->GetAttributes());
		descriptorSetLayouts = ref(shaderProgram->GetDescriptorSetLayouts());
	}

	void AddVertexInputBinding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate, std::vector<VulkankVertexInputAttributeDescription> attrs) {
		bindings.emplace_back(VkVertexInputBindingDescription{binding, stride, inputRate});
		for (auto attr : attrs) {
			attributes.emplace_back(VkVertexInputAttributeDescription{attr.location, binding, attr.format, attr.offset});
		}
	}

	inline void AddVertexInputBinding(uint32_t stride, VkVertexInputRate inputRate, std::vector<VulkankVertexInputAttributeDescription> attrs) {
		AddVertexInputBinding(bindings.size(), stride, inputRate, attrs);
	}

	void AddAlphaBlendingAttachment() {
		VkPipelineColorBlendAttachmentState blendingAttachmentState {};
		blendingAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendingAttachmentState.blendEnable = VK_TRUE;
		blendingAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendingAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendingAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendingAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendingAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendingAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachments.push_back(blendingAttachmentState);
	}

	void AddOpaqueAttachment() {
		VkPipelineColorBlendAttachmentState blendingAttachmentState {};
		blendingAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendingAttachmentState.blendEnable = VK_FALSE;
		blendingAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendingAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendingAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendingAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendingAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendingAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachments.push_back(blendingAttachmentState);
	}

};
