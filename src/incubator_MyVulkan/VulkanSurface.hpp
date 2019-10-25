#pragma once

#include "VulkanStructs.hpp"
#include "Window.hpp"

using namespace std;

class VulkanSurface {
private:
	VkInstance instance;
	Window *window;
	VkSurfaceKHR handle;

public:
	VulkanSurface(VkInstance instance, Window *window) : instance(instance), window(window), handle(window->CreateSurface(instance)) {

	}

	~VulkanSurface() {
		vkDestroySurfaceKHR(instance, handle, nullptr);
	}

	inline VkSurfaceKHR GetHandle() const {
		return handle;
	}

};
