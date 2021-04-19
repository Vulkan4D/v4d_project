#include "EntityConfigFile.h"

CONFIGFILE_STRUCT_CPP(EntityConfigFile)

std::shared_ptr<MeshFile> EntityConfigFile::GetMeshFileInstance(const std::string& filePath)
	STATIC_CLASS_INSTANCES_CPP(filePath, MeshFile, filePath)

MeshFile::MeshFile(const std::string& filePath) {
	LOG("Loading glTF model " << filePath)
	using namespace tinygltf;
	TinyGLTF loader;
	std::string err;
	std::string warn;
	if (!loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filePath)) {
		throw std::runtime_error(err);
	}
	if (warn != "") LOG(warn)
	if (!Load()) {
		throw std::runtime_error("Failed to load glTF model");
	}
}

bool MeshFile::Load() {
	
	for (auto node : gltfModel.nodes) {
		glm::dvec3& translation = transforms[node.name] = {0,0,0};
		if (node.translation.size() == 3) {
			translation.x = node.translation[0];
			translation.y = node.translation[1];
			translation.z = node.translation[2];
		}
		
		if (node.mesh != -1) {
			// LOG("Loading mesh node '" << node.name << "'")
			
			auto& meshData = meshes[node.name];
			auto& geometryPrimitives = meshData.geometries;
			
			geometryPrimitives.reserve(gltfModel.meshes[node.mesh].primitives.size());
			
			for (auto& p : gltfModel.meshes[node.mesh].primitives) {
				ASSERT_OR_RETURN_FALSE(p.mode == TINYGLTF_MODE_TRIANGLES);
				auto* primitiveData = &geometryPrimitives.emplace_back();
				
				{// Indices
					auto& primitiveIndices = gltfModel.accessors[p.indices];
					auto& indexBufferView = gltfModel.bufferViews[primitiveIndices.bufferView];
					ASSERT_OR_RETURN_FALSE(indexBufferView.byteStride == 0); // Only supports tightly packed buffers
					ASSERT_OR_RETURN_FALSE(indexBufferView.byteLength > 0);
					primitiveData->indexCount = primitiveIndices.count;
					switch (primitiveIndices.componentType) {
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT : {
							ASSERT_OR_RETURN_FALSE(indexBufferView.byteLength == primitiveIndices.count * sizeof(Index16));
							primitiveData->index16Buffer = reinterpret_cast<Index16*>(&gltfModel.buffers[indexBufferView.buffer].data.data()[indexBufferView.byteOffset]);
							primitiveData->indexStart = meshData.index16Count;
							meshData.index16Count += primitiveIndices.count;
						break;}
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
							ASSERT_OR_RETURN_FALSE(indexBufferView.byteLength == primitiveIndices.count * sizeof(Index32));
							primitiveData->index32Buffer = reinterpret_cast<Index32*>(&gltfModel.buffers[indexBufferView.buffer].data.data()[indexBufferView.byteOffset]);
							primitiveData->indexStart = meshData.index32Count;
							meshData.index32Count += primitiveIndices.count;
						break;}
						default: throw std::runtime_error("Index buffer only supports 16 or 32 bits unsigned integer components");
					}
					ASSERT_OR_RETURN_FALSE(primitiveData->indexCount > 0);
					ASSERT_OR_RETURN_FALSE(primitiveData->index16Buffer || primitiveData->index32Buffer);
				}
				
				{// Vertex data
					for (auto&[name,accessorIndex] : p.attributes) {
						if (name == "POSITION") {
							auto& vertices = gltfModel.accessors[accessorIndex];
							ASSERT_OR_RETURN_FALSE(primitiveData->vertexCount == 0 || primitiveData->vertexCount == vertices.count);
							primitiveData->vertexCount = vertices.count;
							auto& vertexBufferView = gltfModel.bufferViews[vertices.bufferView];
							ASSERT_OR_RETURN_FALSE(vertices.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
							ASSERT_OR_RETURN_FALSE(vertexBufferView.byteStride == 0); // Only supports tightly packed buffers
							ASSERT_OR_RETURN_FALSE(vertexBufferView.byteLength > 0);
							ASSERT_OR_RETURN_FALSE(vertexBufferView.byteLength == vertices.count * sizeof(VertexPosition));
							primitiveData->vertexPositionBuffer = reinterpret_cast<VertexPosition*>(&gltfModel.buffers[vertexBufferView.buffer].data.data()[vertexBufferView.byteOffset]);
							primitiveData->vertexPositionStart = meshData.vertexPositionCount;
							meshData.vertexPositionCount += vertices.count;
						} else if (name == "NORMAL") {
							auto& vertices = gltfModel.accessors[accessorIndex];
							ASSERT_OR_RETURN_FALSE(primitiveData->vertexCount == 0 || primitiveData->vertexCount == vertices.count);
							primitiveData->vertexCount = vertices.count;
							auto& vertexBufferView = gltfModel.bufferViews[vertices.bufferView];
							ASSERT_OR_RETURN_FALSE(vertices.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
							ASSERT_OR_RETURN_FALSE(vertexBufferView.byteStride == 0); // Only supports tightly packed buffers
							ASSERT_OR_RETURN_FALSE(vertexBufferView.byteLength > 0);
							ASSERT_OR_RETURN_FALSE(vertexBufferView.byteLength == vertices.count * sizeof(VertexNormal));
							primitiveData->vertexNormalBuffer = reinterpret_cast<VertexNormal*>(&gltfModel.buffers[vertexBufferView.buffer].data.data()[vertexBufferView.byteOffset]);
							primitiveData->vertexNormalStart = meshData.vertexNormalCount;
							meshData.vertexNormalCount += vertices.count;
						} else if (name == "TEXCOORD_0") {
							auto& vertices = gltfModel.accessors[accessorIndex];
							ASSERT_OR_RETURN_FALSE(primitiveData->vertexCount == 0 || primitiveData->vertexCount == vertices.count);
							primitiveData->vertexCount = vertices.count;
							auto& vertexBufferView = gltfModel.bufferViews[vertices.bufferView];
							ASSERT_OR_RETURN_FALSE(vertices.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
							ASSERT_OR_RETURN_FALSE(vertexBufferView.byteStride == 0); // Only supports tightly packed buffers
							ASSERT_OR_RETURN_FALSE(vertexBufferView.byteLength > 0);
							ASSERT_OR_RETURN_FALSE(vertexBufferView.byteLength == vertices.count * sizeof(VertexUV));
							primitiveData->vertexUVBuffer = reinterpret_cast<VertexUV*>(&gltfModel.buffers[vertexBufferView.buffer].data.data()[vertexBufferView.byteOffset]);
							primitiveData->vertexUVStart = meshData.vertexUVCount;
							meshData.vertexUVCount += vertices.count;
						} else if (name == "COLOR_0") {
							auto& vertices = gltfModel.accessors[accessorIndex];
							ASSERT_OR_RETURN_FALSE(primitiveData->vertexCount == 0 || primitiveData->vertexCount == vertices.count);
							primitiveData->vertexCount = vertices.count;
							auto& vertexBufferView = gltfModel.bufferViews[vertices.bufferView];
							ASSERT_OR_RETURN_FALSE(vertexBufferView.byteStride == 0); // Only supports tightly packed buffers
							ASSERT_OR_RETURN_FALSE(vertexBufferView.byteLength > 0);
							switch (vertices.componentType) {
								case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE : {
									ASSERT_OR_RETURN_FALSE(vertexBufferView.byteLength == vertices.count * sizeof(VertexColorU8));
									primitiveData->vertexColorU8Buffer = reinterpret_cast<VertexColorU8*>(&gltfModel.buffers[vertexBufferView.buffer].data.data()[vertexBufferView.byteOffset]);
									primitiveData->vertexColorU8Start = meshData.vertexColorU8Count;
									meshData.vertexColorU8Count += vertices.count;
								break;}
								case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT : {
									ASSERT_OR_RETURN_FALSE(vertexBufferView.byteLength == vertices.count * sizeof(VertexColorU16));
									primitiveData->vertexColorU16Buffer = reinterpret_cast<VertexColorU16*>(&gltfModel.buffers[vertexBufferView.buffer].data.data()[vertexBufferView.byteOffset]);
									primitiveData->vertexColorU16Start = meshData.vertexColorU16Count;
									meshData.vertexColorU16Count += vertices.count;
								break;}
								case TINYGLTF_COMPONENT_TYPE_FLOAT: {
									ASSERT_OR_RETURN_FALSE(vertexBufferView.byteLength == vertices.count * sizeof(VertexColorF32));
									primitiveData->vertexColorF32Buffer = reinterpret_cast<VertexColorF32*>(&gltfModel.buffers[vertexBufferView.buffer].data.data()[vertexBufferView.byteOffset]);
									primitiveData->vertexColorF32Start = meshData.vertexColorF32Count;
									meshData.vertexColorF32Count += vertices.count;
								break;}
								default: throw std::runtime_error("Vertex Color attributes only supports 8 or 16 bit unsigned integer or 32-bit float components");
							}
						}
					}
					ASSERT_OR_RETURN_FALSE(primitiveData->vertexCount > 0);
				}
				
				if (p.material != -1) {// Material
					primitiveData->materialName = gltfModel.materials[p.material].name;
				}
				
				meshData.geometriesCount++;
			}
		}
	}
	return true;
}

void EntityConfigFile::ReadConfig() {
	
	// Locking and Reset happens here
	//...
	
	
	ReadFromINI([this](std::stringstream section, std::vector<ConfLineStream>& configs){
		if (section.eof()) return;
		
		std::string type;
		section >> type;
		
		// LOG(type)
		
		if (type == "") {
			//void
		}
		else if (type == "MESH_FILES") {
			for (auto& conf : configs) {
				std::string name, filePath;
				conf.name >> name;
				conf.value >> filePath;
				meshFiles[name] = GetMeshFileInstance(filePath);
			}
		}
		else if (type == "RENDERABLE") {
			if (section.eof()) return;
			
			std::string name;
			section >> name;
			auto& renderable = renderables[name];
			
			for (auto& conf : configs) {
				std::string key;
				conf.name >> key;
			
				if (key == "MESH_NODE") {
					std::string meshFile, nodeName;
					conf.value >> meshFile >> nodeName;
					if (meshFiles.count(meshFile) == 0 || !meshFiles[meshFile] || meshFiles[meshFile]->meshes.count(nodeName) == 0) {
						LOG_ERROR("MESH_NODE: Node " << nodeName << " does not exist in meshFile " << meshFile)
						return;
					}
					auto& mesh = renderable.mesh.emplace();
					mesh.file = meshFiles[meshFile];
					mesh.name = nodeName;
				}
				else if (key == "TRANSFORM_NODE") {
					std::string meshFile, nodeName;
					conf.value >> meshFile >> nodeName;
					if (meshFiles.count(meshFile) == 0 || !meshFiles[meshFile] || meshFiles[meshFile]->transforms.count(nodeName) == 0) {
						LOG_ERROR("TRANSFORM_NODE: Node " << nodeName << " does not exist in meshFile " << meshFile)
						return;
					}
					renderable.transform = glm::translate(glm::dmat4(1), meshFiles[meshFile]->transforms[nodeName]);
				}
				else if (key == "TRANSFORM_POSITION") {
					double x,y,z;
					conf.value >> x >> y >> z;
					renderable.transform = glm::translate(glm::dmat4(1), {x,y,z});
				}
				else if (key == "SHADER") {
					conf.value >> renderable.shader;
				}
			}
			
		}
	});
}

void EntityConfigFile::GenerateRenderables(ClientSideEntity::Ptr entity) {
	for (auto&[name, renderable] : renderables) {
		
		if (renderable.mesh.has_value()) {
			auto renderableEntity = v4d::graphics::RenderableGeometryEntity::Create(entity->moduleID, entity->GetID());
			entity->renderableGeometryEntityInstances[v4d::TextID(name)] = renderableEntity;
		
			renderableEntity->generator = [meshFile=renderable.mesh.value().file, nodeName=renderable.mesh.value().name, shader=renderable.shader, transform=renderable.transform](v4d::graphics::RenderableGeometryEntity* entity, v4d::graphics::vulkan::Device* device){
				auto& meshData = meshFile->meshes[nodeName];
				auto sharedGeometryData = meshData.commonGeometryData.lock();
				if (sharedGeometryData) {
					entity->sharedGeometryData = sharedGeometryData;
					entity->Allocate(device, shader, 0);
				} else {
					entity->sharedGeometryData = std::make_shared<v4d::graphics::RenderableGeometryEntity::SharedGeometryData>();
					meshData.commonGeometryData = entity->sharedGeometryData;
				
					if (meshData.geometriesCount == 0) return;
					entity->Allocate(device, shader, meshData.geometriesCount);
					
					if (meshData.index16Count)
						entity->Add_meshIndices16()->AllocateBuffersCount(device, meshData.index16Count);
					if (meshData.index32Count)
						entity->Add_meshIndices32()->AllocateBuffersCount(device, meshData.index32Count);
					if (meshData.vertexPositionCount)
						entity->Add_meshVertexPosition()->AllocateBuffersCount(device, meshData.vertexPositionCount);
					if (meshData.vertexNormalCount)
						entity->Add_meshVertexNormal()->AllocateBuffersCount(device, meshData.vertexNormalCount);
					if (meshData.vertexColorU8Count)
						entity->Add_meshVertexColorU8()->AllocateBuffersCount(device, meshData.vertexColorU8Count);
					if (meshData.vertexColorU16Count)
						entity->Add_meshVertexColorU16()->AllocateBuffersCount(device, meshData.vertexColorU16Count);
					if (meshData.vertexColorF32Count)
						entity->Add_meshVertexColorF32()->AllocateBuffersCount(device, meshData.vertexColorF32Count);
					if (meshData.vertexUVCount)
						entity->Add_meshVertexUV()->AllocateBuffersCount(device, meshData.vertexUVCount);
					
					entity->sharedGeometryData->geometries.clear();
					entity->sharedGeometryData->geometries.reserve(meshData.geometries.size());
					
					for (auto& geometry : meshData.geometries) {
						auto& geom = entity->sharedGeometryData->geometries.emplace_back();
						geom.transform = transform;
						geom.materialName = geometry.materialName;
						geom.indexCount = geometry.indexCount;
						geom.vertexCount = geometry.vertexCount;
						
						if (geometry.index16Buffer && entity->meshIndices16) {
							memcpy(&entity->meshIndices16.Lock()->data[geometry.indexStart], geometry.index16Buffer, geometry.indexCount * sizeof(Index16));
							geom.firstIndex = geometry.indexStart;
						}
						if (geometry.index32Buffer && entity->meshIndices32) {
							memcpy(&entity->meshIndices32.Lock()->data[geometry.indexStart], geometry.index32Buffer, geometry.indexCount * sizeof(Index32));
							geom.firstIndex = geometry.indexStart;
						}
						if (geometry.vertexPositionBuffer && entity->meshVertexPosition) {
							memcpy(&entity->meshVertexPosition.Lock()->data[geometry.vertexPositionStart], geometry.vertexPositionBuffer, geometry.vertexCount * sizeof(VertexPosition));
							geom.firstVertexPosition = geometry.vertexPositionStart;
						}
						if (geometry.vertexNormalBuffer && entity->meshVertexNormal) {
							memcpy(&entity->meshVertexNormal.Lock()->data[geometry.vertexNormalStart], geometry.vertexNormalBuffer, geometry.vertexCount * sizeof(VertexNormal));
							geom.firstVertexNormal = geometry.vertexNormalStart;
						}
						if (geometry.vertexColorU8Buffer && entity->meshVertexColorU8) {
							memcpy(&entity->meshVertexColorU8.Lock()->data[geometry.vertexColorU8Start], geometry.vertexColorU8Buffer, geometry.vertexCount * sizeof(VertexColorU8));
							geom.firstVertexColorU8 = geometry.vertexColorU8Start;
						}
						if (geometry.vertexColorU16Buffer && entity->meshVertexColorU16) {
							memcpy(&entity->meshVertexColorU16.Lock()->data[geometry.vertexColorU16Start], geometry.vertexColorU16Buffer, geometry.vertexCount * sizeof(VertexColorU16));
							geom.firstVertexColorU16 = geometry.vertexColorU16Start;
						}
						if (geometry.vertexColorF32Buffer && entity->meshVertexColorF32) {
							memcpy(&entity->meshVertexColorF32.Lock()->data[geometry.vertexColorF32Start], geometry.vertexColorF32Buffer, geometry.vertexCount * sizeof(VertexColorF32));
							geom.firstVertexColorF32 = geometry.vertexColorF32Start;
						}
						if (geometry.vertexUVBuffer && entity->meshVertexUV) {
							memcpy(&entity->meshVertexUV.Lock()->data[geometry.vertexUVStart], geometry.vertexUVBuffer, geometry.vertexCount * sizeof(VertexUV));
							geom.firstVertexUV = geometry.vertexUVStart;
						}
					}
				}
				
				// // Special nodes
				// for (auto& node : meshData.gltfModel.nodes) {
				// 	if (node.name == "Camera" && node.translation.size() == 3) {
				// 		entity->cameraOffset = glm::translate(glm::dmat4(1), glm::dvec3(node.translation[0], node.translation[1], node.translation[2]));
				// 	}
				// }
			};
		}
		
	}
}
