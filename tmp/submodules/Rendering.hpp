#pragma once

namespace v4d::modules {
	using namespace v4d::graphics;
	using namespace v4d::graphics::vulkan;
	using namespace v4d::graphics::vulkan::rtx;
	
	class Rendering {
	public: 
		V4D_DEFINE_SUBMODULE( SUBMODULE_TYPE_GRAPHICS | 1 )
		Rendering() {}
		virtual ~Rendering() {}
		
	protected: 
		Renderer* renderer = nullptr;
		Device* renderingDevice = nullptr;
		Queue* graphicsQueue = nullptr;
		Queue* lowPriorityGraphicsQueue = nullptr;
		Queue* lowPriorityComputeQueue = nullptr;
		Queue* transferQueue = nullptr;
		SwapChain* swapChain = nullptr;
		
	public:
		void SetRenderer(Renderer* renderer) {
			this->renderer = renderer;
		}
		void SetRenderingDevice(Device* renderingDevice) {
			this->renderingDevice = renderingDevice;
		}
		void SetGraphicsQueue(Queue* queue) {
			this->graphicsQueue = queue;
		}
		void SetLowPriorityGraphicsQueue(Queue* queue) {
			this->lowPriorityGraphicsQueue = queue;
		}
		void SetLowPriorityComputeQueue(Queue* queue) {
			this->lowPriorityComputeQueue = queue;
		}
		void SetTransferQueue(Queue* queue) {
			this->transferQueue = queue;
		}
		void SetSwapChain(SwapChain* swapChain) {
			this->swapChain = swapChain;
		}
		
	public: // Methods that are typically overridden, in the correct execution order
	
		virtual int OrderIndex() const {return 1000;}
	
		// Executed when calling InitRenderer() on the main Renderer
		virtual void Init() {}
		virtual void InitLayouts(std::map<std::string, DescriptorSet*>&, std::unordered_map<std::string, Image*>&, std::unordered_map<std::string, PipelineLayout*>& pipelineLayouts) {}
		virtual void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders, ShaderBindingTable*, std::unordered_map<std::string, PipelineLayout*>& pipelineLayouts) {}
		
		// Executed when calling their respective methods on the main Renderer
		virtual void ReadShaders() {}
		virtual void LoadScene(Scene&) {}
		virtual void UnloadScene(Scene&) {}
		
		// Executed when calling LoadRenderer()
		virtual void ScorePhysicalDeviceSelection(int& score, PhysicalDevice*) {}
		// after selecting rendering device
		virtual void InitDeviceFeatures() {}
		// after creating locical device and queues
		virtual void Info() {}
		virtual void CreateResources() {} // here we have the swapChain available
		virtual void DestroyResources() {}
		virtual void AllocateBuffers() {}
		virtual void FreeBuffers() {}
		virtual void CreatePipelines(std::unordered_map<std::string, Image*>&) {}
		virtual void DestroyPipelines() {}
		
		// Rendering methods potentially executed on each frame
		virtual void RunDynamicGraphicsTop(VkCommandBuffer, std::unordered_map<std::string, Image*>&) {}
		virtual void RunDynamicGraphicsMiddle(VkCommandBuffer, std::unordered_map<std::string, Image*>&) {}
		virtual void RunDynamicGraphicsBottom(VkCommandBuffer, std::unordered_map<std::string, Image*>&) {}
		virtual void RunDynamicLowPriorityCompute(VkCommandBuffer) {}
		virtual void RunDynamicLowPriorityGraphics(VkCommandBuffer) {}
		
		#ifdef _ENABLE_IMGUI
			virtual void RunImGui() {}
			#ifdef _DEBUG
				virtual void RunImGuiDebug() {}
			#endif
		#endif
		
		// Executed before each frame
		virtual void FrameUpdate(Scene&) {}
		virtual void LowPriorityFrameUpdate() {}
		
	};
}
