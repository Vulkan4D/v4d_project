#pragma once

#include <v4d.h>

namespace v4d::graphics::vulkan {
	
	class V4DLIB ShaderPipeline : public ShaderProgram {
	public:
		using ShaderProgram::ShaderProgram;
		virtual ~ShaderPipeline() {};
		
		virtual void Execute(Device* device, VkCommandBuffer cmdBuffer) {
			Bind(device, cmdBuffer);
			Render(device, cmdBuffer);
		}
		
		virtual void CreatePipeline(Device*) = 0;
		virtual void DestroyPipeline(Device*) = 0;
		
	protected:
		VkPipeline pipeline = VK_NULL_HANDLE;
		
		// pure virtual methods
		virtual void Bind(Device*, VkCommandBuffer) = 0;
		virtual void Render(Device*, VkCommandBuffer) = 0;
	};
	
	
	
	
	class V4DLIB ComputeShaderPipeline : public ShaderPipeline {
		uint32_t groupCountX = 0, groupCountY = 0, groupCountZ = 0;
		
	public:
		ComputeShaderPipeline(PipelineLayout* pipelineLayout, ShaderInfo shaderInfo) : ShaderPipeline(pipelineLayout, {shaderInfo}) {}
		virtual ~ComputeShaderPipeline() {}
		
		virtual void CreatePipeline(Device* device) override {
			CreateShaderStages(device);
			VkComputePipelineCreateInfo computeCreateInfo {
				VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,// VkStructureType sType
				nullptr,// const void* pNext
				0,// VkPipelineCreateFlags flags
				GetStages()->at(0),// VkPipelineShaderStageCreateInfo stage
				GetPipelineLayout()->handle,// VkPipelineLayout layout
				VK_NULL_HANDLE,// VkPipeline basePipelineHandle
				0// int32_t basePipelineIndex
			};
			device->CreateComputePipelines(VK_NULL_HANDLE, 1, &computeCreateInfo, nullptr, &pipeline);
		}
		
		virtual void DestroyPipeline(Device* device) override {
			device->DestroyPipeline(pipeline, nullptr);
			DestroyShaderStages(device);
		}
		
		// VkPipeline GetComputePipeline() const {
		// 	return pipeline;
		// }
		
		void SetGroupCounts(uint32_t x, uint32_t y, uint32_t z) {
			groupCountX = x;
			groupCountY = y;
			groupCountZ = z;
		}
		
	protected:
		virtual void Bind(Device* device, VkCommandBuffer cmdBuffer) override {
			device->CmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
			device->CmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipelineLayout()->handle, 0, GetPipelineLayout()->vkDescriptorSets.size(), GetPipelineLayout()->vkDescriptorSets.data(), 0, nullptr);
		}
		virtual void Render(Device* device, VkCommandBuffer cmdBuffer) override {
			device->CmdDispatch(cmdBuffer, groupCountX, groupCountY, groupCountZ);
		}
		
	};
	
	
	
	
	class V4DLIB RasterShaderPipeline : public ShaderPipeline {
		VkGraphicsPipelineCreateInfo pipelineCreateInfo {};
	public:
		
		// Data to draw
		Buffer* vertexBuffer = nullptr;
		Buffer* indexBuffer = nullptr;
		uint32_t vertexCount = 0;
		
		// Graphics Pipeline information
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
		
		VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
		std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments {};
		VkPipelineColorBlendStateCreateInfo colorBlending {};
		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo {};
		std::vector<VkDynamicState> dynamicStates {}; // Dynamic settings that CAN be changed at runtime but NOT every frame

		
		RasterShaderPipeline(PipelineLayout* pipelineLayout, const std::vector<ShaderInfo>& shaderInfo) : ShaderPipeline(pipelineLayout, shaderInfo) {
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
		
		virtual ~RasterShaderPipeline() {}
		
		void SetData(Buffer* vertexBuffer, Buffer* indexBuffer) {
			this->vertexBuffer = vertexBuffer;
			this->indexBuffer = indexBuffer;
		}
		
		void SetData(Buffer* vertexBuffer, uint32_t vertexCount) {
			this->vertexBuffer = vertexBuffer;
			this->vertexCount = vertexCount;
		}
		
		void SetData(uint32_t vertexCount) {
			this->vertexCount = vertexCount;
		}

		virtual void CreatePipeline(Device* device) override {
			CreateShaderStages(device);
			
			// Bindings and Attributes
			vertexInputInfo.vertexBindingDescriptionCount = GetBindings()->size();
			vertexInputInfo.pVertexBindingDescriptions = GetBindings()->data();
			vertexInputInfo.vertexAttributeDescriptionCount = GetAttributes()->size();
			vertexInputInfo.pVertexAttributeDescriptions = GetAttributes()->data();

			// Dynamic states
			if (dynamicStates.size() > 0) {
				dynamicStateCreateInfo.dynamicStateCount = (uint)dynamicStates.size();
				dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();
				pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
			} else {
				pipelineCreateInfo.pDynamicState = nullptr;
			}
			
			pipelineCreateInfo.layout = GetPipelineLayout()->handle;

			// Fixed functions
			pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
			pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
			pipelineCreateInfo.pRasterizationState = &rasterizer;
			pipelineCreateInfo.pMultisampleState = &multisampling;
			colorBlending.attachmentCount = colorBlendAttachments.size();
			colorBlending.pAttachments = colorBlendAttachments.data();
			pipelineCreateInfo.pColorBlendState = &colorBlending;

			// Shaders
			pipelineCreateInfo.stageCount = GetStages()->size();
			pipelineCreateInfo.pStages = GetStages()->data();
			
			// Create the actual pipeline
			if (device->CreateGraphicsPipelines(VK_NULL_HANDLE/*pipelineCache*/, 1, &pipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create Graphics Pipeline");
			}
		}
		
		virtual void DestroyPipeline(Device* device) override {
			device->DestroyPipeline(pipeline, nullptr);
			DestroyShaderStages(device);
			colorBlendAttachments.clear();
		}
		
		void SetRenderPass(VkPipelineViewportStateCreateInfo* viewportState, VkRenderPass renderPass, uint32_t subpass = 0) {
			pipelineCreateInfo.pViewportState = viewportState;
			pipelineCreateInfo.renderPass = renderPass;
			pipelineCreateInfo.subpass = subpass;
		}
		
		void AddColorBlendAttachmentState(
			VkBool32 blendEnable = VK_TRUE,
			VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			VkBlendOp colorBlendOp = VK_BLEND_OP_ADD,
			VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD,
			VkColorComponentFlags colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		) {
			colorBlendAttachments.push_back({
				blendEnable, // VkBool32
				srcColorBlendFactor, // VkBlendFactor
				dstColorBlendFactor, // VkBlendFactor
				colorBlendOp, // VkBlendOp
				srcAlphaBlendFactor, // VkBlendFactor
				dstAlphaBlendFactor, // VkBlendFactor
				alphaBlendOp, // VkBlendOp
				colorWriteMask // VkColorComponentFlags
			});
		}
		
		
	protected:
		virtual void Bind(Device* device, VkCommandBuffer cmdBuffer) override {
			device->CmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			GetPipelineLayout()->Bind(device, cmdBuffer);
		}
		
		virtual void Render(Device* device, VkCommandBuffer cmdBuffer) override {
			VkDeviceSize offsets[] = {0};
			if (vertexBuffer == nullptr) {
				device->CmdDraw(cmdBuffer,
					vertexCount, // vertexCount
					1, // instanceCount
					0, // firstVertex (defines the lowest value of gl_VertexIndex)
					0  // firstInstance (defines the lowest value of gl_InstanceIndex)
				);
			} else {
				device->CmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer->buffer, offsets);
				if (indexBuffer == nullptr) {
					// Draw vertices
					device->CmdDraw(cmdBuffer,
						vertexCount, // vertexCount
						1, // instanceCount
						0, // firstVertex (defines the lowest value of gl_VertexIndex)
						0  // firstInstance (defines the lowest value of gl_InstanceIndex)
					);
				} else {
					// Draw indices
					device->CmdBindIndexBuffer(cmdBuffer, indexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
					device->CmdDrawIndexed(cmdBuffer,
						static_cast<uint32_t>(indexBuffer->size / sizeof(uint32_t)), // indexCount
						1, // instanceCount
						0, // firstVertex (defines the lowest value of gl_VertexIndex)
						0, // vertexOffset
						0  // firstInstance (defines the lowest value of gl_InstanceIndex)
					);
				}
			}
		}
		
	};
	
	
}
