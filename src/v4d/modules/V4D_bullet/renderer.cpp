#define _V4D_MODULE
#include <v4d.h>

#include "btBulletDynamicsCommon.h"
#include "../V4D_hybrid/camera_options.hh"

using namespace v4d::scene;
using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

Renderer* r = nullptr;
Scene* scene = nullptr;
V4D_Renderer* primaryRenderModule = nullptr;

btDynamicsWorld* world = nullptr;

class DebugDrawer : public btIDebugDraw {
	int mode = DBG_DrawWireframe | DBG_DrawFeaturesText | DBG_DrawText | DBG_FastWireframe;
	
	void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override {
		glm::vec4 clipSpace1 = scene->camera.projectionMatrix * scene->camera.viewMatrix * glm::dvec4(from.x(), from.y(), from.z(), 1);
		glm::vec4 clipSpace2 = scene->camera.projectionMatrix * scene->camera.viewMatrix * glm::dvec4(to.x(), to.y(), to.z(), 1);
		clipSpace1 /= clipSpace1.w;
		clipSpace2 /= clipSpace2.w;
		if (clipSpace1.z < 0 || clipSpace2.z < 0) return;
		primaryRenderModule->DrawOverlayLine(clipSpace1.x, clipSpace1.y, clipSpace2.x, clipSpace2.y, {color.x(), color.y(), color.z(), 1}, 2);
	}
	void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) override {
		
	}
	void reportErrorWarning(const char* warningString) override {
		
	}
	void draw3dText(const btVector3& location, const char* textString) override {
		glm::vec4 screenSpace = scene->camera.projectionMatrix * scene->camera.viewMatrix * glm::dvec4(location.x(), location.y(), location.z(), 1);
		screenSpace /= screenSpace.w;
		if (screenSpace.z < 0) return;
		primaryRenderModule->DrawOverlayText(textString, screenSpace.x, screenSpace.y, {1,1,1,1}, 20);
	}
	void setDebugMode(int debugMode) override {
		mode = debugMode;
	}
	int getDebugMode() const override {
		return mode;
	}
} debugDrawer;

V4D_MODULE_CLASS(V4D_Renderer) {
	
	V4D_MODULE_FUNC(void, Init, v4d::graphics::Renderer* _r, v4d::scene::Scene* _s) {
		r = _r;
		scene = _s;
		primaryRenderModule = V4D_Renderer::GetPrimaryModule();
	}
	
	V4D_MODULE_FUNC(void*, ModuleGetCustomPtr, int) {
		return &debugDrawer;
	}
	V4D_MODULE_FUNC(void, ModuleSetCustomPtr, int, void* ptr) {
		world = (btDynamicsWorld*)ptr;
	}
	
	V4D_MODULE_FUNC(void, Render2, VkCommandBuffer commandBuffer) {
		if (world && (scene->camera.debugOptions & DEBUG_OPTION_PHYSICS)) {
			world->debugDrawWorld();
		}
	}
	
};
