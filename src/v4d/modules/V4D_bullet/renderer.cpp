#define _V4D_MODULE
#include <v4d.h>

#include "btBulletDynamicsCommon.h"

v4d::graphics::Renderer* r = nullptr;
v4d::graphics::Scene* scene = nullptr;
V4D_Renderer* primaryRenderModule = nullptr;

btDynamicsWorld* world = nullptr;

class DebugDrawer : public btIDebugDraw {
	void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override {
		glm::vec4 screenSpace1 = scene->camera.projectionMatrix * scene->camera.viewMatrix * glm::dvec4(from.x(), from.y(), from.z(), 1);
		glm::vec4 screenSpace2 = scene->camera.projectionMatrix * scene->camera.viewMatrix * glm::dvec4(to.x(), to.y(), to.z(), 1);
		primaryRenderModule->DrawOverlayLine(screenSpace1.x, screenSpace1.y, screenSpace2.x, screenSpace2.y, {color.x(), color.y(), color.z(), 1}, 2);
	}
	void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) override {
		
	}
	void reportErrorWarning(const char* warningString) override {
		
	}
	void draw3dText(const btVector3& location, const char* textString) override {
		glm::vec4 screenSpace = scene->camera.projectionMatrix * scene->camera.viewMatrix * glm::dvec4(location.x(), location.y(), location.z(), 1);
		primaryRenderModule->DrawOverlayText(textString, screenSpace.x, screenSpace.y, {1,1,1,1}, 20);
	}
	void setDebugMode(int debugMode) override {
		
	}
	int getDebugMode() const override {
		return 0;
	}
} debugDrawer;

extern "C" {
	
	void Init(v4d::graphics::Renderer* _r, v4d::graphics::Scene* _s) {
		r = _r;
		scene = _s;
		primaryRenderModule = V4D_Renderer::GetPrimaryModule();
	}
	
	void* ModuleGetCustomPtr(int) {
		return &debugDrawer;
	}
	void ModuleSetCustomPtr(int, void* ptr) {
		world = (btDynamicsWorld*)ptr;
	}
	
	void Render2(VkCommandBuffer commandBuffer) {
		if (world) {
			world->debugDrawWorld();
		}
	}
	
}
