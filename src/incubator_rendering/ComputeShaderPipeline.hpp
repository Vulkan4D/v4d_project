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
		
	protected:
		VkPipeline* pipeline = nullptr;
		
		// pure virtual methods
		virtual void Bind(Device*, VkCommandBuffer) = 0;
		virtual void Render(Device*, VkCommandBuffer) = 0;
	};
	
	
	
	
	class V4DLIB ComputeShaderPipeline : public ShaderPipeline {
		uint32_t groupCountX = 0, groupCountY = 0, groupCountZ = 0;
		
	public:
		ComputeShaderPipeline(PipelineLayout* pipelineLayout, ShaderInfo shaderInfo) : ShaderPipeline(pipelineLayout, {shaderInfo}) {}
		virtual ~ComputeShaderPipeline() {}
		
		virtual void CreatePipeline(Device* device) {
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
			pipeline = new VkPipeline;
			device->CreateComputePipelines(VK_NULL_HANDLE, 1, &computeCreateInfo, nullptr, pipeline);
		}
		
		virtual void DestroyPipeline(Device* device) {
			device->DestroyPipeline(*pipeline, nullptr);
			delete pipeline;
			DestroyShaderStages(device);
		}
		
		VkPipeline GetComputePipeline() const {
			return *pipeline;
		}
		
		void SetGroupCounts(uint32_t x, uint32_t y, uint32_t z) {
			groupCountX = x;
			groupCountY = y;
			groupCountZ = z;
		}
		
	protected:
		virtual void Bind(Device* device, VkCommandBuffer cmdBuffer) override {
			device->CmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
			device->CmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, GetPipelineLayout()->handle, 0, GetPipelineLayout()->vkDescriptorSets.size(), GetPipelineLayout()->vkDescriptorSets.data(), 0, nullptr);
		}
		virtual void Render(Device* device, VkCommandBuffer cmdBuffer) override {
			device->CmdDispatch(cmdBuffer, groupCountX, groupCountY, groupCountZ);
		}
		
	};
	
	
	
	
	class V4DLIB RasterShaderPipeline : public ShaderPipeline {
	public:
		
		// Data to draw
		Buffer* vertexBuffer = nullptr;
		Buffer* indexBuffer = nullptr;
		uint32_t vertexCount = 0;
		
		// Rasterizer settings
		VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
		VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		VkBool32 depthWriteEnable = VK_TRUE;
		VkBool32 depthTestEnable = VK_TRUE;
		
		using ShaderPipeline::ShaderPipeline;
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

		virtual void ConfigureGraphicsPipeline(GraphicsPipeline* graphicsPipeline) {
			pipeline = &graphicsPipeline->handle;
			// Configure
			graphicsPipeline->inputAssembly.topology = topology;
			graphicsPipeline->rasterizer.cullMode = cullMode;
			graphicsPipeline->depthStencilState.depthWriteEnable = depthWriteEnable;
			graphicsPipeline->depthStencilState.depthTestEnable = depthTestEnable;
			graphicsPipeline->rasterizer.frontFace = frontFace;
		}
		
		
	protected:
		virtual void Bind(Device* device, VkCommandBuffer cmdBuffer) override {
			device->CmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
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
