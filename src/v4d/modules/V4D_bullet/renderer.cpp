#define _V4D_MODULE
#include <v4d.h>

#include "btBulletDynamicsCommon.h"

v4d::graphics::Renderer* r = nullptr;

class DebugDrawer : public btIDebugDraw {
	void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override {
		
	}
	void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) override {
		
	}
	void reportErrorWarning(const char* warningString) override {
		
	}
	void draw3dText(const btVector3& location, const char* textString) override {
		
	}
	void setDebugMode(int debugMode) override {
		
	}
	int getDebugMode() const override {
		return 0;
	}
public:
	VkCommandBuffer commandBuffer;
} debugDrawer;

btDynamicsWorld* world = nullptr;

extern "C" {
	
	void Init(v4d::graphics::Renderer* _r, v4d::graphics::Scene*) {
		r = _r;
	}
	
	void* ModuleGetCustomPtr(int) {
		return &debugDrawer;
	}
	void ModuleSetCustomPtr(int, void* ptr) {
		world = (btDynamicsWorld*)ptr;
	}
	
	void Render(VkCommandBuffer commandBuffer) {
		if (world) {
			debugDrawer.commandBuffer = commandBuffer;
			world->debugDrawWorld();
		}
	}
	
}
