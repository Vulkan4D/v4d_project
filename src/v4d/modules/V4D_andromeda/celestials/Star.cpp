#include "Star.h"
#include "../../V4D_raytracing/camera_options.hh"

void Star::RenderUpdate(glm::dvec3 position, glm::dvec3 cameraPosition, double sizeInScreen/* > 0.001 */) const {
	auto& sphere = renderableEntities["sphere"];
	if (!sphere) {
		sphere = v4d::graphics::RenderableGeometryEntity::Create(THIS_MODULE, GetID());
		sphere->generator = [this](v4d::graphics::RenderableGeometryEntity* entity, v4d::graphics::vulkan::Device* device){
			double radius = GetRadius();
			double intensity = 1e22;
			entity->Allocate(device, "V4D_raytracing:aabb_sphere.light")->material.visibility.emission = intensity;
			entity->rayTracingMask = RAY_TRACED_ENTITY_LIGHT;
			entity->Add_proceduralVertexAABB()->AllocateBuffers(device, {glm::vec3(-radius), glm::vec3(radius)});
			entity->Add_meshVertexColorU8()->AllocateBuffers(device, {255,255,255,255});
			entity->Add_lightSource(glm::vec3{0,0,0}, glm::vec3{1}, radius, intensity);
		};
	}
	sphere->SetWorldTransform(glm::translate(glm::dmat4(1), position));
}
