#include "samples/samples_base_main.inl"

// Called right after loading the vulkan library, before creating the vulkan instance
void ApplicationInit() {
	
}

void KeyInputCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	// Quit application upon pressing the Escape key
	if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
		glfwSetWindowShouldClose(window, 1);
	}
}

void Draw(xvk::Instance* instance, xvk::Device* device, VkCommandBuffer cmdBuffer, VkImage image, VkImageSubresourceRange imageSubresourceRange) {

	// Clear
	VkClearColorValue clearColor {0.0f, 0.3f, 1.0f, 0.0f};
	device->CmdClearColorImage(
		cmdBuffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		&clearColor,
		1, // rangeCount
		&imageSubresourceRange
	);
	
	//...
	
	
}
