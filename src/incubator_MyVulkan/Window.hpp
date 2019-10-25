#pragma once 

// namespace v4d::graphics {
	class Window {
	private:
		int index;
		int width, height;

		GLFWwindow *handle;
		bool resized = false;

		static std::unordered_map<int, Window*> windows;

		static int GetNextIndex() {
			static std::atomic<int> nextIndex = 0;
			return nextIndex++;
		}

		static void ActivateWindowSystem() {
			// Init GLFW
			if (!glfwInit())
				throw std::runtime_error("GLFW Init Failed");
			
			// Hints
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

			// Check for Vulkan support
			if (!glfwVulkanSupported())
				throw std::runtime_error("Vulkan is not supported");

			// Get required vulkan instance extensions from glfw
			uint glfwExtensionCount = 0;
			const char** glfwExtensions;
			glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
			vulkanRequiredExtensions.reserve(glfwExtensionCount);
			for (uint i = 0; i < glfwExtensionCount; i++) {
				vulkanRequiredExtensions.push_back(glfwExtensions[i]);
			}
		}

		static void DeactivateWindowSystem() {
			glfwTerminate();
		}

		static void ResizeCallback(GLFWwindow *handle, int /*newWidth*/, int /*newHeight*/) {
			auto window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
			window->resized = true;
			window->RefreshSize();
		}

	public:
		Window(const std::string& title, int width, int height, GLFWmonitor* monitor = nullptr, GLFWwindow* share = nullptr) : index(GetNextIndex()), width(width), height(height) {
			if (windows.size() == 0) ActivateWindowSystem();
			windows.emplace(index, this);
			handle = glfwCreateWindow(width, height, title.c_str(), monitor, share);
			glfwSetWindowUserPointer(handle, this);
			glfwSetFramebufferSizeCallback(handle, ResizeCallback);
		}

		~Window() {
			glfwDestroyWindow(handle);
			windows.erase(index);
			if (windows.size() == 0) DeactivateWindowSystem();
		}

		// vk::SurfaceKHR CreateSurface(vk::Instance& instance) {
		// 	vk::SurfaceKHR surface;
		// 	if (VkResult err = glfwCreateWindowSurface(instance, handle, nullptr, reinterpret_cast<VkSurfaceKHR*>(&surface)); err != VK_SUCCESS) {
		// 		LOG_ERROR(err)
		// 		throw std::runtime_error("Failed to create Vulkan Surface");
		// 	}
		// 	return surface;
		// }

		VkSurfaceKHR CreateSurface(VkInstance instance) {
			VkSurfaceKHR surface;
			if (VkResult err = glfwCreateWindowSurface(instance, handle, nullptr, &surface); err != VK_SUCCESS) {
				LOG_ERROR(err)
				throw std::runtime_error("Failed to create Vulkan Surface");
			}
			return surface;
		}

		bool IsActive() {
			return !glfwWindowShouldClose(handle);
		}

		GLFWwindow* GetHandle() const {
			return handle;
		}

		int GetWidth() const {
			return width;
		}

		int GetHeight() const {
			return height;
		}

		bool WasResized() const {
			return resized;
		}

		void ResetResize() {
			resized = false;
		}

		void RefreshSize() {
			glfwGetFramebufferSize(handle, &width, &height);
		}

		void WaitEvents() {
			RefreshSize();
			glfwWaitEvents();
		}

	};
// }
