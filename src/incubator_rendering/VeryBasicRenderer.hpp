#pragma once

#include "VulkanRenderer.hpp"

class VeryBasicRenderer : public VulkanRenderer {
	using VulkanRenderer::VulkanRenderer;
	
	RasterShaderPipeline* testShader;
	RenderPass renderPass;
	
public: // Scene configuration methods

	void LoadScene() override {
		// Shader program
		testShader = new RasterShaderPipeline(pipelineLayouts.emplace_back(new PipelineLayout()), {
			"incubator_rendering/assets/shaders/verybasic.vert",
			"incubator_rendering/assets/shaders/verybasic.frag",
		});
		testShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		testShader->depthStencilState.depthTestEnable = VK_FALSE;
		testShader->depthStencilState.depthWriteEnable = VK_FALSE;
		testShader->SetData(4);
		testShader->LoadShaders();
	}

	void UnloadScene() override {
		// Shaders
		delete testShader;
		
		// Pipeline Layouts
		for (auto* layout : pipelineLayouts) {
			delete layout;
		}
		pipelineLayouts.clear();
	}

protected: // Graphics configuration

	void CreatePipelines() override {
		
		// Color Attachment (Fragment shader Standard Output)
		VkAttachmentDescription colorAttachment = {};
			colorAttachment.format = swapChain->format.format;
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			// Color and depth data
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			// Stencil data
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		VkAttachmentReference colorAttachmentRef = {
			renderPass.AddAttachment(colorAttachment),
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};
		
		// SubPass
		VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
		renderPass.AddSubpass(subpass);
		
		// Create the render pass
		renderPass.Create(renderingDevice);
		
		// Frame Buffers
		renderPass.frameBuffers.resize(swapChain->imageViews.size());
		for (size_t i = 0; i < swapChain->imageViews.size(); i++) {
			VkFramebufferCreateInfo framebufferCreateInfo = {};
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.renderPass = renderPass.handle;
			framebufferCreateInfo.attachmentCount = 1;
			framebufferCreateInfo.pAttachments = &swapChain->imageViews[i];
			framebufferCreateInfo.width = swapChain->extent.width;
			framebufferCreateInfo.height = swapChain->extent.height;
			framebufferCreateInfo.layers = 1;
			if (renderingDevice->CreateFramebuffer(&framebufferCreateInfo, nullptr, &renderPass.GetFrameBuffer(i)) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create framebuffer");
			}
		}
		
		// Shaders
		for (auto* shaderPipeline : {testShader}) {
			shaderPipeline->SetRenderPass(&swapChain->viewportState, renderPass.handle, 0);
			shaderPipeline->AddColorBlendAttachmentState();
			shaderPipeline->CreatePipeline(renderingDevice);
		}
	}
	
	void DestroyPipelines() override {
		for (auto& shaderPipeline : {testShader}) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		renderPass.Destroy(renderingDevice);
		for (auto framebuffer : renderPass.frameBuffers) {
			renderingDevice->DestroyFramebuffer(framebuffer, nullptr);
		}
	}
	
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass.handle;
		renderPassInfo.framebuffer = renderPass.GetFrameBuffer(imageIndex);
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = swapChain->extent;
		std::array<VkClearValue, 1> clearValues = {};
		clearValues[0].color = {.0,.0,.0,.0};
		renderPassInfo.clearValueCount = clearValues.size();
		renderPassInfo.pClearValues = clearValues.data();
		
		renderingDevice->CmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		testShader->Execute(renderingDevice, commandBuffer);
		renderingDevice->CmdEndRenderPass(commandBuffer);
	}
	
};
