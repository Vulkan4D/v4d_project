#pragma once

#include "VulkanRenderer.hpp"

class VeryBasicRenderer : public VulkanRenderer {
	using VulkanRenderer::VulkanRenderer;
	
	RenderPass renderPass;
	PipelineLayout testLayout;
	RasterShaderPipeline* testShader;
	
public: // Scene configuration methods

	void LoadScene() override {
		// Shader program
		testShader = new RasterShaderPipeline(testLayout, {
			"incubator_rendering/assets/shaders/verybasic.vert",
			"incubator_rendering/assets/shaders/verybasic.frag",
		});
		testShader->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		testShader->SetData(4);
		testShader->LoadShaders();
	}

	void UnloadScene() override {
		// Shaders
		delete testShader;
	}

protected: // Graphics configuration

	void CreatePipelines() override {
		// Pipeline layouts
		testLayout.Create(renderingDevice);
		
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
		renderPass.CreateFrameBuffers(renderingDevice, swapChain);
		
		// Shaders
		testShader->SetRenderPass(&swapChain->viewportState, renderPass.handle, 0);
		testShader->AddColorBlendAttachmentState();
		testShader->CreatePipeline(renderingDevice);
	}
	
	void DestroyPipelines() override {
		testShader->DestroyPipeline(renderingDevice);
		renderPass.DestroyFrameBuffers(renderingDevice);
		renderPass.Destroy(renderingDevice);
		testLayout.Destroy(renderingDevice);
	}
	
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		renderPass.Begin(renderingDevice, commandBuffer, swapChain, {{.0,.0,.0,.0}}, imageIndex);
		testShader->Execute(renderingDevice, commandBuffer);
		renderPass.End(renderingDevice, commandBuffer);
	}
	
};
