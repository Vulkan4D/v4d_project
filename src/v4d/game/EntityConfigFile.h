#pragma once

// TinyGltfLoader
#define TINYGLTF_USE_CPP14
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_INCLUDE_JSON
#include "../../json.hpp"
#include "../../tinygltf/tiny_gltf.h"

#include <v4d.h>
#include "utilities/io/ConfigFile.h"
#include "utilities/graphics/Mesh.hpp"
#include "utilities/graphics/RenderableGeometryEntity.h"

#include "Entity.h"

using namespace v4d::graphics::Mesh;

class V4DGAME MeshFile {
	friend class EntityConfigFile;
	
	tinygltf::Model gltfModel;
	
	struct MeshData{
		struct Geometry {
			std::string materialName {""};
			uint32_t indexCount = 0;
			uint32_t vertexCount = 0;
			uint32_t indexStart = 0;
			Index16* index16Buffer = nullptr;
			Index32* index32Buffer = nullptr;
			VertexPosition* vertexPositionBuffer = nullptr;
			uint32_t vertexPositionStart = 0;
			VertexNormal* vertexNormalBuffer = nullptr;
			uint32_t vertexNormalStart = 0;
			VertexColorU8* vertexColorU8Buffer = nullptr;
			uint32_t vertexColorU8Start = 0;
			VertexColorU16* vertexColorU16Buffer = nullptr;
			uint32_t vertexColorU16Start = 0;
			VertexColorF32* vertexColorF32Buffer = nullptr;
			uint32_t vertexColorF32Start = 0;
			VertexUV* vertexUVBuffer = nullptr;
			uint32_t vertexUVStart = 0;
		};
		std::vector<Geometry> geometries {};
		
		uint32_t geometriesCount = 0;
		uint32_t index16Count = 0;
		uint32_t index32Count = 0;
		uint32_t vertexPositionCount = 0;
		uint32_t vertexNormalCount = 0;
		uint32_t vertexColorU8Count = 0;
		uint32_t vertexColorU16Count = 0;
		uint32_t vertexColorF32Count = 0;
		uint32_t vertexUVCount = 0;
	
		std::weak_ptr<v4d::graphics::RenderableGeometryEntity::SharedGeometryData> commonGeometryData;
	};
	
	std::unordered_map<std::string, MeshData> meshes {};
	std::unordered_map<std::string, glm::dvec3> transforms {};
	
	bool Load();
	
public:
	MeshFile(const std::string& filePath);
	
};


class V4DGAME EntityConfigFile : public v4d::io::ConfigFile {
public:
	CONFIGFILE_STRUCT_H(EntityConfigFile)
private:
	void ReadConfig() override;
	void WriteConfig() override {}
	
	// [MESH_FILES]
	// <name> = <FilePath> ...
	static std::shared_ptr<MeshFile> GetMeshFileInstance(const std::string& filePath);
	std::unordered_map<std::string/*name*/, std::shared_ptr<MeshFile>> meshFiles {};
	
	// [RENDERABLE <name>] ...
	struct Renderable {
		
		// MESH_NODE = <meshFiles::name> <Node::name>
		struct Mesh {
			std::shared_ptr<MeshFile> file;
			std::string name;
		};
		std::optional<Mesh> mesh = std::nullopt;
		
		// // SPHERE = <radius>
		// struct Sphere {double radius;};
		// std::optional<Sphere> sphere = std::nullopt;
		
		// // BOX = <halfSize_X> <halfSize_Y> <halfSize_Z>
		// struct Box {glm::dvec3 halfSize;};
		// std::optional<Box> box = std::nullopt;
		
		// TRANSFORM_NODE = <meshFiles::name> <Node::name>
		// TRANSFORM_POSITION = <X> <Y> <Z>
		glm::dmat4 transform {1};
		
		// SHADER = <sbtOffset::name>
		std::string shader {"V4D_raytracing:default"};
		
	};
	std::unordered_map<std::string/*name*/, Renderable> renderables {};
	
public:
	void GenerateRenderables(ClientSideEntity::Ptr entity);
	
};
