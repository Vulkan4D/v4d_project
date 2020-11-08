#pragma once

class TmpBlock {
	v4d::scene::Scene* scene;
	v4d::scene::ObjectInstancePtr sceneObject = nullptr;
	
public:
	Block* block = nullptr;
	float boundingDistance = 1.0;
	glm::vec4 wireframeColor {0.5f, 0.5f, 0.5f, 0.5f};

	TmpBlock(v4d::scene::Scene* scene, glm::dvec3 position = {0, 0, 0}, double angle = (0.0), glm::dvec3 axis = {0, 0, 1}) : scene(scene) {
		sceneObject = scene->AddObjectInstance();
		sceneObject->Configure([this](v4d::scene::ObjectInstance* obj){
			if (block) {
				auto geom = obj->AddGeometry("transparent", Block::MAX_VERTICES, Block::MAX_INDICES);
				
				geom->renderWireframe = true;
				geom->wireframeColor = wireframeColor;
				geom->wireframeThickness = 5.0f;
				
				auto[vertexCount, indexCount] = block->GenerateGeometry(geom->GetVertexPtr(), geom->GetIndexPtr(), 0, 0.3);
				geom->Shrink(vertexCount, indexCount);
				
				for (int i = 0; i < geom->vertexCount; ++i) {
					auto* vert = geom->GetVertexPtr(i);
					geom->boundingDistance = glm::max(geom->boundingDistance, glm::length(vert->pos));
					geom->boundingBoxSize = glm::max(glm::abs(vert->pos), geom->boundingBoxSize);
				}
				boundingDistance = geom->boundingDistance;
				geom->isDirty = true;
			}
		}, position, angle, axis);
	}
	
	~TmpBlock() {
		ClearBlock();
		if (sceneObject) scene->RemoveObjectInstance(sceneObject);
	}
	
	Block& SetBlock(SHAPE shape) {
		ClearBlock();
		block = new Block(shape);
		return *block;
	}
	
	void ResetGeometry() {
		sceneObject->ClearGeometries();
	}
	
	void ClearBlock() {
		if (block) delete block;
	}

	void SetWorldTransform(glm::dmat4 t) {
		if (sceneObject) {
			sceneObject->SetWorldTransform(t);
		}
	}
	
	glm::dmat4 GetWorldTransform() const {
		return sceneObject->GetWorldTransform();
	}
	
};
