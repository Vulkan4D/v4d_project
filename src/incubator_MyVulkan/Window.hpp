#pragma once 

// namespace v4d::graphics {
	class Window {
	private:
		int index;
		int width, height;

		GLFWwindow *handle;
		
		static std::unordered_map<int, Window*> windows;
		
		std::map<std::string, std::function<void(int,int)>> resizeCallbacks {};
		std::map<std::string, std::function<void(int,int,int,int)>> keyCallbacks {};

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
		}

		static void DeactivateWindowSystem() {
			glfwTerminate();
		}

		static void ResizeCallback(GLFWwindow* handle, int newWidth, int newHeight) {
			auto window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
			window->RefreshSize();
			for (auto[name, callback] : window->resizeCallbacks) {
				callback(newWidth, newHeight);
			}
		}
		
		static void KeyCallback(GLFWwindow* handle, int key, int scancode, int action, int mods) {
			auto window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
			for (auto[name, callback] : window->keyCallbacks) {
				callback(key, scancode, action, mods);
			}
		}

	public:
		Window(const std::string& title, int width, int height, GLFWmonitor* monitor = nullptr, GLFWwindow* share = nullptr) : index(GetNextIndex()), width(width), height(height) {
			if (windows.size() == 0) ActivateWindowSystem();
			windows.emplace(index, this);
			handle = glfwCreateWindow(width, height, title.c_str(), monitor, share);
			glfwSetWindowUserPointer(handle, this);
			glfwSetFramebufferSizeCallback(handle, ResizeCallback);
			glfwSetKeyCallback(handle, KeyCallback);
		}

		~Window() {
			glfwDestroyWindow(handle);
			windows.erase(index);
			if (windows.size() == 0) DeactivateWindowSystem();
		}

		VkSurfaceKHR CreateVulkanSurface(VkInstance instance) {
			VkSurfaceKHR surface;
			if (VkResult err = glfwCreateWindowSurface(instance, handle, nullptr, &surface); err != VK_SUCCESS) {
				LOG_ERROR(err)
				throw std::runtime_error("Failed to create Vulkan Surface");
			}
			return surface;
		}
		
		void GetRequiredVulkanInstanceExtensions(std::vector<const char*>& requiredInstanceExtensions) const {
			uint glfwExtensionCount = 0;
			const char** glfwExtensions;
			glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
			requiredInstanceExtensions.reserve(glfwExtensionCount);
			for (uint i = 0; i < glfwExtensionCount; i++) {
				requiredInstanceExtensions.push_back(glfwExtensions[i]);
			}
		}
		
		void AddResizeCallback(std::string name, std::function<void(int,int)>&& callback) {
			resizeCallbacks[name] = std::forward<std::function<void(int,int)>>(callback);
		}
		
		void RemoveResizeCallback(std::string name) {
			resizeCallbacks.erase(name);
		}

		void AddKeyCallback(std::string name, std::function<void(int,int,int,int)>&& callback) {
			keyCallbacks[name] = std::forward<std::function<void(int,int,int,int)>>(callback);
		}
		
		void RemoveKeyCallback(std::string name) {
			keyCallbacks.erase(name);
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

		void RefreshSize() {
			glfwGetFramebufferSize(handle, &width, &height);
		}

		void WaitEvents() {
			RefreshSize();
			glfwWaitEvents();
		}

	};
// }
