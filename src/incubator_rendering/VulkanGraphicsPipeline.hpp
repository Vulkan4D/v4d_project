#pragma once

#include "VulkanStructs.hpp"
#include "VulkanDevice.hpp"
#include "VulkanShader.hpp"
#include "VulkanPipelineLayout.hpp"

class VulkanGraphicsPipeline {
private:
	VulkanDevice* device;

	std::vector<VkPipelineShaderStageCreateInfo>* shaderStages;
	std::vector<VkVertexInputBindingDescription>* bindings;
	std::vector<VkVertexInputAttributeDescription>* attributes;

public:
	VkPipeline handle = VK_NULL_HANDLE;
	VulkanPipelineLayout* pipelineLayout;

	VkPipelineRasterizationStateCreateInfo rasterizer {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		nullptr, // const void* pNext
		0, // VkPipelineRasterizationStateCreateFlags flags
		VK_FALSE, // VkBool32 depthClampEnable
		VK_FALSE, // VkBool32 rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL, // VkPolygonMode polygonMode
		VK_CULL_MODE_BACK_BIT, // VkCullModeFlags cullMode
		VK_FRONT_FACE_COUNTER_CLOCKWISE, // VkFrontFace frontFace
		VK_FALSE, // VkBool32 depthBiasEnable
		0, // float depthBiasConstantFactor
		0, // float depthBiasClamp
		0, // float depthBiasSlopeFactor
		1 // float lineWidth
	};
	VkPipelineMultisampleStateCreateInfo multisampling {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		nullptr, // const void* pNext
		0, // VkPipelineMultisampleStateCreateFlags flags
		VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits rasterizationSamples
		VK_TRUE, // VkBool32 sampleShadingEnable
		0.2f, // float minSampleShading
		nullptr, // const VkSampleMask* pSampleMask
		VK_FALSE, // VkBool32 alphaToCoverageEnable
		VK_FALSE // VkBool32 alphaToOneEnable
	};
	VkPipelineInputAssemblyStateCreateInfo inputAssembly {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		nullptr, // const void* pNext
		0, // VkPipelineInputAssemblyStateCreateFlags flags
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // VkPrimitiveTopology topology
		VK_FALSE, // VkBool32 primitiveRestartEnable  // If set to VK_TRUE, then it's possible to break up lines and triangles in the _STRIP topology modes by using a special index of 0xFFFF or 0xFFFFFFFF.
	};
	VkPipelineDepthStencilStateCreateInfo depthStencilState {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		nullptr, // const void* pNext
		0, // VkPipelineDepthStencilStateCreateFlags flags
		VK_TRUE, // VkBool32 depthTestEnable
		VK_TRUE, // VkBool32 depthWriteEnable
		VK_COMPARE_OP_LESS, // VkCompareOp depthCompareOp
		VK_FALSE, // VkBool32 depthBoundsTestEnable
		VK_FALSE, // VkBool32 stencilTestEnable
		{}, // VkStencilOpState front
		{}, // VkStencilOpState back
		0.0f, // float minDepthBounds
		1.0f, // float maxDepthBounds
	};
	
	VkGraphicsPipelineCreateInfo pipelineCreateInfo {};
	VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments {};
	VkPipelineColorBlendStateCreateInfo colorBlending {};
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo {};
	std::vector<VkDynamicState> dynamicStates {}; // Dynamic settings that CAN be changed at runtime but NOT every frame

	VulkanGraphicsPipeline(VulkanDevice* device) : device(device) {
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

		colorBlending.logicOpEnable = VK_FALSE; // If enabled, will effectively replace and disable blendingAttachmentState.blendEnable
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // optional, if enabled above
		colorBlending.blendConstants[0] = 0; // optional
		colorBlending.blendConstants[1] = 0; // optional
		colorBlending.blendConstants[2] = 0; // optional
		colorBlending.blendConstants[3] = 0; // optional
		
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		
		// Optional
		// Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline. 
		// The idea of pipeline derivatives is that it is less expensive to set up pipelines when they have much functionality in common with an existing pipeline and switching between pipelines from the same parent can also be done quicker.
		// You can either specify the handle of an existing pipeline with basePipelineHandle or reference another pipeline that is about to be created by index with basePipelineIndex. 
		// These are only used if VK_PIPELINE_CREATE_DERIVATIVE_BIT flag is also specified in the flags field of VkGraphicsPipelineCreateInfo.
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex = -1;
	}

	void Prepare() {

		// Bindings and Attributes
		vertexInputInfo.vertexBindingDescriptionCount = bindings->size();
		vertexInputInfo.pVertexBindingDescriptions = bindings->data();
		vertexInputInfo.vertexAttributeDescriptionCount = attributes->size();
		vertexInputInfo.pVertexAttributeDescriptions = attributes->data();

		// Dynamic states
		if (dynamicStates.size() > 0) {
			dynamicStateCreateInfo.dynamicStateCount = (uint)dynamicStates.size();
			dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();
			pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
		} else {
			pipelineCreateInfo.pDynamicState = nullptr;
		}
		
		pipelineCreateInfo.layout = pipelineLayout->handle;

		// Fixed functions
		pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
		pipelineCreateInfo.pRasterizationState = &rasterizer;
		pipelineCreateInfo.pMultisampleState = &multisampling;
		colorBlending.attachmentCount = colorBlendAttachments.size();
		colorBlending.pAttachments = colorBlendAttachments.data();
		pipelineCreateInfo.pColorBlendState = &colorBlending;

		// Shaders
		pipelineCreateInfo.stageCount = shaderStages->size();
		pipelineCreateInfo.pStages = shaderStages->data();
	}

	void Create() {
		if (device->CreateGraphicsPipelines(VK_NULL_HANDLE/*pipelineCache*/, 1, &pipelineCreateInfo, nullptr, &handle) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Graphics Pipeline");
		}
	}

	~VulkanGraphicsPipeline() {
		device->DestroyPipeline(handle, nullptr);
	}

	void SetShaderProgram(VulkanShaderProgram* shaderProgram) {
		shaderStages = shaderProgram->GetStages();
		bindings = shaderProgram->GetBindings();
		attributes = shaderProgram->GetAttributes();
		pipelineLayout = shaderProgram->GetPipelineLayout();
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
	
	void Bind(VulkanDevice* device, VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS) {
		device->CmdBindPipeline(commandBuffer, bindPoint, handle);
		pipelineLayout->Bind(device, commandBuffer);
	}

};
