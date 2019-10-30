#pragma once

#include "VulkanStructs.hpp"
#include "Window.hpp"

class VulkanSurface {
private:
	xvk::Interface::InstanceInterface* vulkanInstance;
	Window *window;
	VkSurfaceKHR handle;

public:
	VulkanSurface(xvk::Interface::InstanceInterface* vulkanInstance, Window* window) : vulkanInstance(vulkanInstance), window(window), handle(window->CreateSurface(vulkanInstance->handle)) {

	}

	~VulkanSurface() {
		vulkanInstance->DestroySurfaceKHR(handle, nullptr);
	}

	inline VkSurfaceKHR GetHandle() const {
		return handle;
	}

};
