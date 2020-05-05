#pragma once

namespace v4d::modules {
	class Input {
	public: 
		V4D_DEFINE_SUBMODULE( SUBMODULE_TYPE_INPUT | 1 )
	
	protected:
		Window* window = nullptr;
		Renderer* renderer = nullptr;
		
	public:
		virtual ~Input(){}
		
		virtual std::string CallbackName() const {return "InputSubModule";}
		
		virtual void SetWindow(Window* window) {
			this->window = window;
		}
		virtual void SetRenderer(Renderer* renderer) {
			this->renderer = renderer;
		}
		virtual void AddCallbacks() {
			window->AddKeyCallback(this->CallbackName(), [this](int key, int scancode, int action, int mods){
				this->KeyCallback(key, scancode, action, mods);
			});
			window->AddMouseButtonCallback(this->CallbackName(), [this](int button, int action, int mods){
				this->MouseButtonCallback(button, action, mods);
			});
			window->AddScrollCallback(this->CallbackName(), [this](double x, double y){
				this->ScrollCallback(x, y);
			});
			window->AddCharCallback(this->CallbackName(), [this](unsigned int c){
				this->CharCallback(c);
			});
		}
		virtual void RemoveCallbacks() {
			window->RemoveKeyCallback(this->CallbackName());
			window->RemoveMouseButtonCallback(this->CallbackName());
		}
		
		virtual void Init() {}
		virtual void KeyCallback(int key, int scancode, int action, int mods) {}
		virtual void MouseButtonCallback(int button, int action, int mods) {}
		virtual void ScrollCallback(double x, double y) {}
		virtual void CharCallback(unsigned int c) {}
		virtual void Update(double deltaTime) {}
		
	};
}
