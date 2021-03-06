#pragma once

#include <unordered_set>

using namespace v4d::graphics;

class Block {
public:
	static constexpr int MAX_POINTS = 8;
	static constexpr int MAX_FACES = 7;
	static constexpr int MAX_LINES = 12;
	static constexpr int MAX_VERTICES_SIMPLE = 24;
	static constexpr int MAX_INDICES_SIMPLE = 36;
	static constexpr int MAX_VERTICES = MAX_VERTICES_SIMPLE + MAX_POINTS*3 + MAX_LINES*4 + 6;
	static constexpr int MAX_INDICES = MAX_INDICES_SIMPLE + MAX_POINTS*3 + MAX_LINES*6;
protected:
	
	// 32 Bytes
	struct BlockData {
		BlockData() = default;
		BlockData(SHAPE t, uint32_t index = 0, float size = 1.0f)
			:index(index)
			,shape((int)t)
			,orientation(0)
			
			,sizeX(size*10.0f - 1)
			,sizeY(size*10.0f - 1)
			,sizeZ(size*10.0f - 1)
			
			,_extra_attributes_1(0)
			
			,posX(0)
			,posY(0)
			,posZ(0)
			
			,_extra_attributes_2(0)
			
			,color0(0)
			,face0(true)
			,color1(0)
			,face1(true)
			,color2(0)
			,face2(true)
			,color3(0)
			,face3(true)
			,color4(0)
			,face4(true)
			,color5(0)
			,face5(true)
			,color6(0)
			,face6(true)
			,color7(0)
			,useVertexColorGradients(false)
			
			,state(0)
			
			,structureMaterial(0)
			
			,face0Material(0)
			,face1Material(0)
			,face2Material(0)
			,face3Material(0)
			,face4Material(0)
			,face5Material(0)
			,face6Material(0)
			
			,line0(0)
			,line1(0)
			,line2(0)
			,line3(0)
			,line4(0)
			,line5(0)
			,line6(0)
			,line7(0)
			,line8(0)
			,line9(0)
			,line10(0)
			,line11(0)
			
			,_extra_attributes_3(0)
		{
			static_assert(sizeof(Block) == 32);
		}
		
		// 4 bytes ident
			uint32_t index : 24; // shall reset to 0 upon game load
			uint32_t shape : 3; // 8 possible shapes, only 5 used
			uint32_t orientation : 5; // 32 possible orientations, only 24 used
		
		// 4 bytes size+extra
			// 3x10 bits size	uvec3(0.1 to 102.4 meters +- 10 cm)
				uint32_t sizeX : 10;
				uint32_t sizeY : 10;
				uint32_t sizeZ : 10;
			// extra per-block attributes for future use
				uint32_t _extra_attributes_1 : 2;
		
		// 8 bytes position+extra
			// 3x20 bits position	ivec3(-104.8576 km to +104.8575 km, +- 10cm) (max hull length of > 200 km)
				int64_t posX : 21;
				int64_t posY : 21;
				int64_t posZ : 21;
			// extra per-block attributes for future use
				int64_t _extra_attributes_2 : 1;
		
		// 8 bytes colors and face visibility (128 color palette, different color for each face or for each point depending on the value of useVertexColorGradients)
			uint8_t color0 : 7;
			uint8_t face0 : 1;
			uint8_t color1 : 7;
			uint8_t face1 : 1;
			uint8_t color2 : 7;
			uint8_t face2 : 1;
			uint8_t color3 : 7;
			uint8_t face3 : 1;
			uint8_t color4 : 7;
			uint8_t face4 : 1;
			uint8_t color5 : 7;
			uint8_t face5 : 1;
			uint8_t color6 : 7;
			uint8_t face6 : 1;
			uint8_t color7 : 7;
			uint8_t useVertexColorGradients : 1;
		
		// 8 bytes for remaining attributes (may change)
			uint64_t state : 8;
			// 8 materials (with 32 possibilities) for 1 structure and 7 faces (MAY CHANGE TO TWO MATERIALS WITH a million POSSIBILITIES OR ADD MORE EXTRA ATTRIBUTES)
			uint64_t structureMaterial : 5;
			uint64_t face0Material : 5;
			uint64_t face1Material : 5;
			uint64_t face2Material : 5;
			uint64_t face3Material : 5;
			uint64_t face4Material : 5;
			uint64_t face5Material : 5;
			uint64_t face6Material : 5;
			// enable/disable individual lines
			uint64_t line0 : 1;
			uint64_t line1 : 1;
			uint64_t line2 : 1;
			uint64_t line3 : 1;
			uint64_t line4 : 1;
			uint64_t line5 : 1;
			uint64_t line6 : 1;
			uint64_t line7 : 1;
			uint64_t line8 : 1;
			uint64_t line9 : 1;
			uint64_t line10 : 1;
			uint64_t line11 : 1;
			// extra per-block attributes for future use
			uint64_t _extra_attributes_3 : 4;
	} data;
	
	struct FaceVertex {
		uint8_t vertexIndex;
		Mesh::VertexPosition* vertexPosition;
		Mesh::VertexNormal* vertexNormal;
		Mesh::VertexColor<uint8_t>* vertexColor;
		uint32_t* customData;
	};
	
	uint8_t GetColorIndex(uint8_t i) const {
		switch (i) {
			case 0: return data.color0;
			case 1: return data.color1;
			case 2: return data.color2;
			case 3: return data.color3;
			case 4: return data.color4;
			case 5: return data.color5;
			case 6: return data.color6;
			case 7: return data.color7;
		}
		return 0;
	}
	
	bool GetFaceActive(uint8_t i) const {
		switch (i) {
			case 0: return !! data.face0;
			case 1: return !! data.face1;
			case 2: return !! data.face2;
			case 3: return !! data.face3;
			case 4: return !! data.face4;
			case 5: return !! data.face5;
			case 6: return !! data.face6;
		}
		return false;
	}
	
	glm::ivec3 GetRawPosition() const {
		return glm::ivec3(data.posX, data.posY, data.posZ);
	};
	
	void SetRawPosition(glm::ivec3 pos) {
		data.posX = pos.x;
		data.posY = pos.y;
		data.posZ = pos.z;
	}
	
	SHAPE const GetShapeType() const {
		return (SHAPE)data.shape;
	};
	
	std::vector<glm::vec3> const GetPoints() const {
		switch (GetShapeType()) {
			case SHAPE::CUBE: return {
				// top
				/*0*/{ 1, 1, 1}, // front right
				/*1*/{-1, 1, 1}, // front left
				/*2*/{-1, 1,-1}, // back left
				/*3*/{ 1, 1,-1}, // back right
				// bottom
				/*4*/{ 1,-1, 1}, // front right
				/*5*/{-1,-1, 1}, // front left
				/*6*/{-1,-1,-1}, // back left
				/*7*/{ 1,-1,-1}, // back right
			};
			case SHAPE::SLOPE: return {
				/*0*/{ 1, 1, 1}, // front right top
				/*1*/{-1, 1, 1}, // front left top
				/*2*/{-1,-1,-1}, // back left bottom
				/*3*/{ 1,-1,-1}, // back right bottom
				/*4*/{ 1,-1, 1}, // front right bottom
				/*5*/{-1,-1, 1}, // front left bottom
			};
			case SHAPE::CORNER: return {
				/*0*/{ 1, 1, 1}, // front right top
				/*1*/{-1,-1, 1}, // front left bottom
				/*2*/{ 1,-1,-1}, // back right bottom
				/*3*/{ 1,-1, 1}, // front right bottom
			};
			case SHAPE::PYRAMID: return {
				/*0*/{ 1, 1, 1}, // front right top
				/*1*/{-1,-1,-1}, // back left bottom
				/*2*/{ 1,-1,-1}, // back right bottom
				/*3*/{ 1,-1, 1}, // front right bottom
				/*4*/{-1,-1, 1}, // front left bottom
			};
			case SHAPE::INVCORNER: return {
				// top
				/*0*/{ 1, 1, 1}, // front right
				/*1*/{-1, 1, 1}, // front left
				/*2*/{-1, 1,-1}, // back left
				// bottom
				/*3*/{ 1,-1, 1}, // front right
				/*4*/{-1,-1, 1}, // front left
				/*5*/{-1,-1,-1}, // back left
				/*6*/{ 1,-1,-1}, // back right
			};
			case SHAPE::_EXTRA1: return {};
			case SHAPE::_EXTRA2: return {};
			case SHAPE::_EXTRA3: return {};
		}
		return {};
	};
	
	std::vector<BlockFace> const GetFaces() const {
		switch (GetShapeType()) {
			case SHAPE::CUBE: return {
				{ // 0 top
					{0,1,2, 2,3,0},
					{FACEDIR::PLUS_Y},
					true
				},
				{ // 1 bottom
					{7,6,5, 5,4,7},
					{FACEDIR::MINUS_Y},
					true
				},
				{ // 2 right
					{0,3,7, 7,4,0},
					{FACEDIR::PLUS_X},
					true
				},
				{ // 3 left
					{2,1,5, 5,6,2},
					{FACEDIR::MINUS_X},
					true
				},
				{ // 4 front
					{0,4,5, 5,1,0},
					{FACEDIR::PLUS_Z},
					true
				},
				{ // 5 back
					{2,6,7, 7,3,2},
					{FACEDIR::MINUS_Z},
					true
				},
			};
			case SHAPE::SLOPE: return {
				{ // 0 slope
					{0,1,2, 2,3,0},
					{FACEDIR::PLUS_Y, FACEDIR::MINUS_Z},
					false
				},
				{ // 1 right
					{0,3,4},
					{FACEDIR::PLUS_X},
					true
				},
				{ // 2 left
					{1,5,2},
					{FACEDIR::MINUS_X},
					true
				},
				{ // 3 front
					{0,4,1, 1,4,5},
					{FACEDIR::PLUS_Z},
					true
				},
				{ // 4 bottom
					{4,3,5, 5,3,2},
					{FACEDIR::MINUS_Y},
					true
				},
			};
			case SHAPE::CORNER: return {
				{ // 0 slope
					{0,1,2},
					{FACEDIR::MINUS_Z, FACEDIR::MINUS_X, FACEDIR::PLUS_Y},
					false
				},
				{ // 1 right
					{0,2,3},
					{FACEDIR::PLUS_X},
					true
				},
				{ // 2 front
					{0,3,1},
					{FACEDIR::PLUS_Z},
					true
				},
				{ // 3 bottom
					{2,1,3},
					{FACEDIR::MINUS_Y},
					true
				},
			};
			case SHAPE::PYRAMID: return {
				{ // 0 back slope
					{0,1,2},
					{FACEDIR::PLUS_Y, FACEDIR::MINUS_Z},
					false
				},
				{ // 1 left slope
					{0,4,1},
					{FACEDIR::PLUS_Y, FACEDIR::MINUS_X},
					false
				},
				{ // 2 right
					{0,2,3},
					{FACEDIR::PLUS_X},
					true
				},
				{ // 3 front
					{0,3,4},
					{FACEDIR::PLUS_Z},
					true
				},
				{ // 4 bottom
					{1,4,2, 2,4,3},
					{FACEDIR::MINUS_Y},
					true
				},
			};
			case SHAPE::INVCORNER: return {
				{ // 0 top
					{0,1,2},
					{FACEDIR::PLUS_Y},
					true
				},
				{ // 1 bottom
					{6,5,4, 4,3,6},
					{FACEDIR::MINUS_Y},
					true
				},
				{ // 2 right
					{0,6,3},
					{FACEDIR::PLUS_X},
					true
				},
				{ // 3 left
					{2,1,5, 5,1,4},
					{FACEDIR::MINUS_X},
					true
				},
				{ // 4 front
					{0,3,1, 1,3,4},
					{FACEDIR::PLUS_Z},
					true
				},
				{ // 5 back
					{2,5,6},
					{FACEDIR::MINUS_Z},
					true
				},
				{ // 6 slope
					{0,2,6},
					{FACEDIR::PLUS_X, FACEDIR::MINUS_Z, FACEDIR::PLUS_Y},
					false
				},
			};
			case SHAPE::_EXTRA1: return {};
			case SHAPE::_EXTRA2: return {};
			case SHAPE::_EXTRA3: return {};
		}
		return {};
	};
	
	std::vector<BlockFace> const GetFacesContainingPoints(std::vector<uint8_t> pts) const {
		std::vector<BlockFace> faces;
		for (auto& f : GetFaces()) {
			for (auto& pt : pts) {
				for (auto& p : f.triangles) {
					if (p == pt) goto Found;
				}
				goto NotFound;
				Found: continue;
			}
			faces.push_back(f);
			NotFound: continue;
		}
		return faces;
	}
	
	std::tuple<uint8_t, uint8_t> const GetAdjacentFaces(uint8_t faceIndex, uint8_t pointIndex) const {
		auto lines = GetLines();
		std::remove_if(lines.begin(), lines.end(), [faceIndex, pointIndex](BlockLine& line){
			if (line.face1 != faceIndex && line.face2 != faceIndex) return true;
			if (line.point1 != pointIndex && line.point2 != pointIndex) return true;
			return false;
		});
		return {
			lines[0].face1==faceIndex? lines[0].face2 : lines[0].face1,
			lines[1].face1==faceIndex? lines[1].face2 : lines[1].face1,
		};
	}
	
	std::vector<BlockLine> const GetLines() const {
		switch (GetShapeType()) { /* {point1,point2, face1,face2}, */
			case SHAPE::CUBE: return {
				{0,1, 0,4}, // top front
				{2,3, 0,5}, // top back
				{4,5, 1,4}, // bottom front
				{6,7, 1,5}, // bottom back
				{1,2, 0,3}, // top left
				{0,3, 0,2}, // top right
				{5,6, 1,3}, // bottom left
				{4,7, 1,2}, // bottom right
				{1,5, 3,4}, // front left
				{2,6, 3,5}, // back left
				{0,4, 2,4}, // front right
				{3,7, 2,5}, // back right
			};
			case SHAPE::SLOPE: return {
				{0,1, 0,3}, // top
				{1,2, 0,2}, // left slope
				{0,3, 0,1}, // right slope
				{2,3, 0,4}, // back
				{4,5, 3,4}, // front bottom
				{0,4, 1,3}, // front right
				{3,4, 1,4}, // bottom right
				{1,5, 2,3}, // front left
				{2,5, 2,4}, // bottom left
			};
			case SHAPE::CORNER: return {
				{0,1, 0,2},
				{1,2, 0,3},
				{0,2, 0,1},
				{0,3, 1,2},
				{1,3, 2,3},
				{2,3, 1,3},
			};
			case SHAPE::PYRAMID: return {
				{0,1, 0,1},
				{0,4, 1,3},
				{0,2, 0,2},
				{0,3, 2,3},
				{1,2, 0,4},
				{2,3, 2,4},
				{1,4, 1,4},
				{3,4, 3,4},
			};
			case SHAPE::INVCORNER: return {
				{0,1, 0,4}, // top front
				{1,2, 0,3}, // top left
				{0,2, 0,6}, // top slope
				{2,6, 5,6}, // back slope
				{0,6, 2,6}, // right slope
				{2,5, 3,5}, // back left
				{5,6, 1,5}, // back bottom
				{0,3, 2,4}, // front right
				{3,6, 1,2}, // bottom right
				{4,5, 1,3}, // bottom left
				{3,4, 1,4}, // bottom front
				{1,4, 3,4}, // front left
			};
			case SHAPE::_EXTRA1: return {};
			case SHAPE::_EXTRA2: return {};
			case SHAPE::_EXTRA3: return {};
		}
		return {};
	};
	
	std::map<FACEDIR, std::vector<uint8_t>> const GetResizePoints() const {
		switch (GetShapeType()) {
			case SHAPE::CUBE: return {
				{FACEDIR::PLUS_X, {0,3,4,7}},
				{FACEDIR::MINUS_X, {1,2,5,6}},
				{FACEDIR::PLUS_Y, {0,1,2,3}},
				{FACEDIR::MINUS_Y, {4,5,6,7}},
				{FACEDIR::PLUS_Z, {0,1,4,5}},
				{FACEDIR::MINUS_Z, {2,3,6,7}},
			};
			case SHAPE::SLOPE: return {
				{FACEDIR::PLUS_X, {0,3,4}},
				{FACEDIR::MINUS_X, {1,5,2}},
				{FACEDIR::PLUS_Y, {0,1}},
				{FACEDIR::MINUS_Y, {2,3,4,5}},
				{FACEDIR::PLUS_Z, {0,1,4,5}},
				{FACEDIR::MINUS_Z, {2,3}},
			};
			case SHAPE::CORNER: return {
				{FACEDIR::PLUS_X, {0,2,3}},
				{FACEDIR::MINUS_X, {1}},
				{FACEDIR::PLUS_Y, {0}},
				{FACEDIR::MINUS_Y, {1,2,3}},
				{FACEDIR::PLUS_Z, {0,1,3}},
				{FACEDIR::MINUS_Z, {2}},
			};
			case SHAPE::PYRAMID: return {
				{FACEDIR::PLUS_X, {0,2,3}},
				{FACEDIR::MINUS_X, {1,4}},
				{FACEDIR::PLUS_Y, {0}},
				{FACEDIR::MINUS_Y, {1,2,3,4}},
				{FACEDIR::PLUS_Z, {0,3,4}},
				{FACEDIR::MINUS_Z, {1,2}},
			};
			case SHAPE::INVCORNER: return {
				{FACEDIR::PLUS_X, {0,3,6}},
				{FACEDIR::MINUS_X, {1,2,4,5}},
				{FACEDIR::PLUS_Y, {0,1,2}},
				{FACEDIR::MINUS_Y, {3,4,5,6}},
				{FACEDIR::PLUS_Z, {0,1,3,4}},
				{FACEDIR::MINUS_Z, {2,5,6}},
			};
			case SHAPE::_EXTRA1: return {
				{FACEDIR::PLUS_X, {}},
				{FACEDIR::MINUS_X, {}},
				{FACEDIR::PLUS_Y, {}},
				{FACEDIR::MINUS_Y, {}},
				{FACEDIR::PLUS_Z, {}},
				{FACEDIR::MINUS_Z, {}},
			};
			case SHAPE::_EXTRA2: return {
				{FACEDIR::PLUS_X, {}},
				{FACEDIR::MINUS_X, {}},
				{FACEDIR::PLUS_Y, {}},
				{FACEDIR::MINUS_Y, {}},
				{FACEDIR::PLUS_Z, {}},
				{FACEDIR::MINUS_Z, {}},
			};
			case SHAPE::_EXTRA3: return {
				{FACEDIR::PLUS_X, {}},
				{FACEDIR::MINUS_X, {}},
				{FACEDIR::PLUS_Y, {}},
				{FACEDIR::MINUS_Y, {}},
				{FACEDIR::PLUS_Z, {}},
				{FACEDIR::MINUS_Z, {}},
			};
		}
		return {};
	}
	
	const float GetFaceSize(FACEDIR dir) const {
		switch (dir) {
			case FACEDIR::PLUS_X:
			case FACEDIR::MINUS_X:
				return data.sizeX;
			case FACEDIR::PLUS_Y:
			case FACEDIR::MINUS_Y:
				return data.sizeY;
			case FACEDIR::PLUS_Z:
			case FACEDIR::MINUS_Z:
				return data.sizeZ;
		}
		return 0;
	}
	
public:
	Block() = default;
	Block(SHAPE t) : data(t) {}
	
	uint32_t GetIndex() const {
		return data.index;
	}
	
	void SetIndex(uint32_t index) {
		data.index = index;
	}
	
	// Grid size of 10 cm for block positions
	void SetPosition(glm::vec3 pos) {
		data.posX = pos.x * 10.0f;
		data.posY = pos.y * 10.0f;
		data.posZ = pos.z * 10.0f;
	}
	glm::vec3 GetPosition() const {
		return glm::vec3(data.posX, data.posY, data.posZ) / 10.0f;
	};
	
	void SetSimilarTo(const Block& ref) {
		data.shape = ref.data.shape;
		data.posX = ref.data.posX;
		data.posY = ref.data.posY;
		data.posZ = ref.data.posZ;
		data.sizeX = ref.data.sizeX;
		data.sizeY = ref.data.sizeY;
		data.sizeZ = ref.data.sizeZ;
		data.orientation = ref.data.orientation;
	}
	
	void SetSize(glm::vec3 size) {
		data.sizeX = size.x * 10.0f - 1;
		data.sizeY = size.y * 10.0f - 1;
		data.sizeZ = size.z * 10.0f - 1;
	}
	
	void SetOrientation(uint8_t rot) {
		data.orientation = rot;
	}
	
	uint8_t GetOrientation() const {
		return data.orientation;
	}
	
	BlockFace GetFace(uint32_t faceIndex) const {
		return GetFaces()[faceIndex];
	}
	
	void SetColors(bool useVertexColorGradients
		,uint8_t color0 = 0
		,uint8_t color1 = 0
		,uint8_t color2 = 0
		,uint8_t color3 = 0
		,uint8_t color4 = 0
		,uint8_t color5 = 0
		,uint8_t color6 = 0
		,uint8_t color7 = 0
		) {
		data.useVertexColorGradients = useVertexColorGradients?1:0;
		data.color0 = color0;
		data.color1 = color1;
		data.color2 = color2;
		data.color3 = color3;
		data.color4 = color4;
		data.color5 = color5;
		data.color6 = color6;
		data.color7 = color7;
	}
	
	void SetColor(uint8_t color) {
		data.useVertexColorGradients = 0;
		data.color0 = color;
		data.color1 = color;
		data.color2 = color;
		data.color3 = color;
		data.color4 = color;
		data.color5 = color;
		data.color6 = color;
		data.color7 = color;
	}
	
	void SetFaceColor(uint8_t faceIndex, uint8_t color) {
		data.useVertexColorGradients = 0;
		switch (faceIndex) {
			case 0: data.color0 = color;break;
			case 1: data.color1 = color;break;
			case 2: data.color2 = color;break;
			case 3: data.color3 = color;break;
			case 4: data.color4 = color;break;
			case 5: data.color5 = color;break;
			case 6: data.color6 = color;break;
			case 7: data.color7 = color;break;
		}
	}
	
	void SetVertexGradientColor(uint8_t vertexIndex, uint8_t color) {
		data.useVertexColorGradients = 1;
		switch (vertexIndex) {
			case 0: data.color0 = color;break;
			case 1: data.color1 = color;break;
			case 2: data.color2 = color;break;
			case 3: data.color3 = color;break;
			case 4: data.color4 = color;break;
			case 5: data.color5 = color;break;
			case 6: data.color6 = color;break;
			case 7: data.color7 = color;break;
		}
	}
	
	static const glm::vec3 GetFaceDirNormal(FACEDIR dir) {
		switch (dir) {
			case FACEDIR::PLUS_X:
				return {1,0,0};
			case FACEDIR::MINUS_X:
				return {-1,0,0};
			case FACEDIR::PLUS_Y:
				return {0,1,0};
			case FACEDIR::MINUS_Y:
				return {0,-1,0};
			case FACEDIR::PLUS_Z:
				return {0,0,1};
			case FACEDIR::MINUS_Z:
				return {0,0,-1};
		}
		return {0,0,0};
	}
	
	float GetHalfSizeForFaceDir(FACEDIR dir) const {
		return (GetFaceSize(dir)+1) / 20.0f;
	}
	
	std::optional<std::tuple<uint32_t, BlockFace>> GetTheFaceWhichTheNormalIs(const glm::vec3& faceNormal) const {
		std::optional<std::tuple<uint32_t, BlockFace>> face = std::nullopt;
		auto faces = GetFaces();
		for (int i = 0; i < faces.size(); ++i) {
			auto& f = faces[i];
			if (f.canAddBlock && f.facedirs.size() == 1) {
				if (GetFaceDirNormal(f.facedirs[0]) == faceNormal) {
					face = {i, f};
					break;
				}
			}
		}
		return face;
	}
	
	glm::mat4 GetRotationMatrix() const {
		auto& rotation = ORIENTATIONS[data.orientation];
		return glm::rotate(glm::mat4(1), glm::radians(rotation.w), glm::vec3(rotation));
	}
	
	std::vector<glm::vec3> GetPointsPositions(bool realSize = true) const {
		auto points = GetPoints();
		if (realSize) {
			// 10 cm grid for block sizes
			for (auto& p : points) {
				p /= 20.0f;
			}
		}
		// Resize points
		for (const auto&[dir, pts] : GetResizePoints()) {
			for (const auto& p : pts) {
				points[p] += GetFaceDirNormal(dir) * GetFaceSize(dir) / (realSize? 20.0f : 1.0f);
			}
		}
		// adjust block orientation
		for (auto& p : points) {
			p = glm::translate(GetRotationMatrix(), p)[3];
		}
		return points;
	}
	
	std::vector<glm::vec3> GetFinalPointsPositions() const {
		auto points = GetPointsPositions(true);
		auto pos = GetPosition();
		for (auto& p : points) p += pos;
		return points;
	}
	
	FACEDIR GetResizeDirFromFace(uint8_t faceIndex, const glm::vec3& position) {
		auto face = GetFaces()[faceIndex];
		FACEDIR dir = FACEDIR::NONE;
		if (face.facedirs.size() == 1) {
			dir = face.facedirs[0];
		} else if (face.facedirs.size() > 1) {
			const auto& points = GetPointsPositions();
			const auto& dirs = GetResizePoints();
			std::map<FACEDIR, float> closestPoints {};
			for (FACEDIR d : face.facedirs) {
				glm::vec3 avgPosition {};
				for (const auto& p : dirs.at(d)) {
					avgPosition += points[p];
				}
				closestPoints[d] = glm::distance(avgPosition / float(dirs.at(d).size()), position);
			}
			dir = std::min_element(closestPoints.begin(), closestPoints.end(), [](const auto& a, const auto& b){
				return a.second < b.second;
			})->first;
		}
		return dir;
	}
	
	std::tuple<uint/* vertexCount */, uint/* indexCount */> 
	GenerateSimpleGeometry(
		Mesh::Index32* outputIndices,
		Mesh::VertexPosition* outputVerticesPosition,
		Mesh::VertexNormal* outputVerticesNormal,
		Mesh::VertexColor<uint8_t>* outputVerticesColor,
		uint32_t* outputCustomData,
		uint vertexIndexOffset = 0,
		float alpha = 1.0f
	) {
		uint vertexCount = 0, indexCount = 0;
		auto points = GetPointsPositions();
		
		// Generate vertices and indices
		std::map<uint8_t/*faceAndPointIndex*/, FaceVertex> faceVertices {};
		int faceIndex = 0;
		for (const auto& face : GetFaces()) {
			assert(faceIndex < MAX_FACES);
			assert(face.triangles.size() >= 3 && (face.triangles.size() % 3) == 0);
			// Normal
			glm::vec3 faceNormal = glm::normalize(glm::cross(points[face.triangles[1]] - points[face.triangles[0]], points[face.triangles[1]] - points[face.triangles[2]]));
			// Vertices
			for (const auto& pointIndex : face.triangles) {
				assert(pointIndex < MAX_POINTS);
				uint8_t faceVerticesIndex = (faceIndex << 4) | pointIndex; // each face should have their own vertices, but vertices within a face can be reused
				FaceVertex* faceVertex;
				try {
					faceVertex = &faceVertices.at(faceVerticesIndex);
				} catch (...) {
					faceVertex = &faceVertices[faceVerticesIndex];
					faceVertex->vertexIndex = vertexCount++;
					faceVertex->vertexPosition = &outputVerticesPosition[faceVertex->vertexIndex];
					faceVertex->vertexNormal = &outputVerticesNormal[faceVertex->vertexIndex];
					faceVertex->vertexColor = &outputVerticesColor[faceVertex->vertexIndex];
					faceVertex->customData = &outputCustomData[faceVertex->vertexIndex];
					PackedBlockCustomData customData;{
						customData.blockIndex = data.index;
						customData.faceIndex = faceIndex;
					}
					switch (faceIndex) {
						case 0: customData.materialId = data.face0Material;break;
						case 1: customData.materialId = data.face1Material;break;
						case 2: customData.materialId = data.face2Material;break;
						case 3: customData.materialId = data.face3Material;break;
						case 4: customData.materialId = data.face4Material;break;
						case 5: customData.materialId = data.face5Material;break;
						case 6: customData.materialId = data.face6Material;break;
						// case 7: customData.materialId = data.structureMaterial;break; //TODO
					}
					auto color = COLORS[GetColorIndex(data.useVertexColorGradients? pointIndex : faceIndex)];
					*faceVertex->vertexPosition = points[pointIndex] + GetPosition();
					*faceVertex->vertexNormal = faceNormal;
					*faceVertex->vertexColor = glm::u8vec4(glm::vec4{color.r, color.g, color.b, alpha}*255.0f);
					*faceVertex->customData = customData.packed;
				}
				outputIndices[indexCount++] = vertexIndexOffset + faceVertex->vertexIndex;
			}
			++faceIndex;
		}
		assert(vertexCount <= MAX_VERTICES_SIMPLE);
		assert(indexCount <= MAX_INDICES_SIMPLE);
		return {vertexCount, indexCount};
	}
	
	std::tuple<uint/* vertexCount */, uint/* indexCount */> 
	GenerateGeometry(
		Mesh::Index32* outputIndices,
		Mesh::VertexPosition* outputVerticesPosition,
		Mesh::VertexNormal* outputVerticesNormal,
		Mesh::VertexColor<uint8_t>* outputVerticesColor,
		uint32_t* outputCustomData,
		uint vertexIndexOffset = 0
	) {
		uint vertexCount = 0, indexCount = 0;
		auto points = GetPointsPositions(true);
		
		bool addBevels = true;
		
		std::map<uint8_t/*faceAndPointIndex*/, FaceVertex> faceVertices {};
		std::unordered_map<uint8_t/*pointIndex*/, std::unordered_set<glm::vec3>> cornerVertices {};
		std::unordered_map<uint8_t/*pointIndex*/, std::vector<std::tuple<uint8_t/*faceIndex*/, glm::vec3/*vertexPosition*/>>> extraCornerVertices {};
		
		// Generate vertices and indices
		int faceIndex = 0;
		for (const auto& face : GetFaces()) {
			assert(faceIndex < MAX_FACES);
			assert(face.triangles.size() >= 3 && (face.triangles.size() % 3) == 0);
			// Normal
			glm::vec3 faceNormal = glm::normalize(glm::cross(points[face.triangles[1]] - points[face.triangles[0]], points[face.triangles[1]] - points[face.triangles[2]]));
			// Vertices
			for (const auto& pointIndex : face.triangles) {
				assert(pointIndex < MAX_POINTS);
				uint8_t faceVerticesIndex = (faceIndex << 4) | pointIndex; // each face should have their own vertices, but vertices within a face can be reused
				FaceVertex* faceVertex;
				try {
					faceVertex = &faceVertices.at(faceVerticesIndex);
				} catch (...) {
					faceVertex = &faceVertices[faceVerticesIndex];
					faceVertex->vertexIndex = vertexCount++;
					faceVertex->vertexPosition = &outputVerticesPosition[faceVertex->vertexIndex];
					faceVertex->vertexNormal = &outputVerticesNormal[faceVertex->vertexIndex];
					faceVertex->vertexColor = &outputVerticesColor[faceVertex->vertexIndex];
					faceVertex->customData = &outputCustomData[faceVertex->vertexIndex];
					PackedBlockCustomData customData;{
						customData.blockIndex = data.index;
						customData.faceIndex = faceIndex;
					}
					switch (faceIndex) {
						case 0: customData.materialId = data.face0Material;break;
						case 1: customData.materialId = data.face1Material;break;
						case 2: customData.materialId = data.face2Material;break;
						case 3: customData.materialId = data.face3Material;break;
						case 4: customData.materialId = data.face4Material;break;
						case 5: customData.materialId = data.face5Material;break;
						case 6: customData.materialId = data.face6Material;break;
					}
					
					auto pt = points[pointIndex];
					
					if (addBevels) {
						float bevelSize = 0.02f;
						pt -= glm::sign(glm::round(points[pointIndex] * glm::abs(glm::abs(faceNormal) - 1.0f) * 100.0f)) * bevelSize;
						if ((GetShapeType() == SHAPE::INVCORNER) && (faceIndex == 6)) pt += faceNormal * bevelSize / 2.0f;
						else if (face.facedirs.size() > 1) pt += faceNormal * bevelSize / 4.0f;
					}
					
					auto colorIndex = GetColorIndex(data.useVertexColorGradients? pointIndex : faceIndex);
					auto color = COLORS[colorIndex];
					
					*faceVertex->vertexPosition = pt + GetPosition();
					*faceVertex->vertexNormal = faceNormal;
					*faceVertex->vertexColor = glm::u8vec4(glm::vec4{color.r, color.g, color.b, colorIndex? 1:0} * 255.0f);
					*faceVertex->customData = customData.packed;
					
					if (cornerVertices[pointIndex].size() < 3) {
						cornerVertices[pointIndex].emplace(glm::round(glm::vec3(*faceVertex->vertexPosition) * 100.0f) / 100.0f);
					} else {
						extraCornerVertices[pointIndex].emplace_back(faceIndex, *faceVertex->vertexPosition);
					}
				}
				outputIndices[indexCount++] = vertexIndexOffset + faceVertex->vertexIndex;
			}
			++faceIndex;
		}
		
		if (addBevels) {
			auto addStructureTriangle = [&outputVerticesPosition, &outputVerticesNormal, &outputVerticesColor, &outputCustomData, &outputIndices, &vertexCount, &indexCount, vertexIndexOffset, this](const auto& vertices){
				assert(vertices.size() == 3 || vertices.size() == 4);
				size_t index = vertexCount;
				for (auto& vertexPos : vertices) outputVerticesPosition[vertexCount++] = vertexPos;
				glm::vec3 faceNormal = glm::normalize(glm::cross(glm::vec3(outputVerticesPosition[index+1]) - glm::vec3(outputVerticesPosition[index+2]), glm::vec3(outputVerticesPosition[index+1]) - glm::vec3(outputVerticesPosition[index+0])));
				if (glm::dot(faceNormal, glm::normalize(glm::vec3(outputVerticesPosition[index]) - GetPosition())) < 0) faceNormal *= -1;
				PackedBlockCustomData customData;{
					customData.blockIndex = data.index;
					customData.faceIndex = 7; // 7 is for structure
					customData.materialId = data.structureMaterial;
				}
				for (int i = 0; i < vertices.size(); ++i) {
					outputCustomData[index+i] = customData.packed;
					outputVerticesNormal[index+i] = faceNormal;
					outputVerticesColor[index+i] = glm::u8vec4(glm::vec4(COLORS[BLOCK_COLOR_GREY], 1) * 255.0f);
				}
				outputIndices[indexCount++] = vertexIndexOffset + index + 0;
				outputIndices[indexCount++] = vertexIndexOffset + index + 1;
				outputIndices[indexCount++] = vertexIndexOffset + index + 2;
				if (vertices.size() == 4) {
					outputIndices[indexCount++] = vertexIndexOffset + index + 2;
					outputIndices[indexCount++] = vertexIndexOffset + index + 3;
					outputIndices[indexCount++] = vertexIndexOffset + index + 1;
				}
			};
			// Corners
			for (auto&[corner, vertices] : cornerVertices) {
				if (vertices.size() == 3) {
					addStructureTriangle(vertices);
					if (extraCornerVertices.count(corner)) {
						for (auto&[faceIndex, vert0] : extraCornerVertices[corner]) {
							auto&[face1, face2] = GetAdjacentFaces(faceIndex, corner);
							auto vert1 = *faceVertices[(face1 << 4) | corner].vertexPosition;
							auto vert2 = *faceVertices[(face2 << 4) | corner].vertexPosition;
							addStructureTriangle(std::vector<glm::vec3>{vert0, vert1, vert2});
						}
					}
				}
			}
			// Edges
			for (auto& line : GetLines()) {
				auto vert0 = *faceVertices[(line.face1 << 4) | line.point1].vertexPosition;
				auto vert1 = *faceVertices[(line.face1 << 4) | line.point2].vertexPosition;
				auto vert2 = *faceVertices[(line.face2 << 4) | line.point1].vertexPosition;
				auto vert3 = *faceVertices[(line.face2 << 4) | line.point2].vertexPosition;
				addStructureTriangle(std::vector<glm::vec3>{vert0, vert1, vert2, vert3});
			}
		}
		
		assert(vertexCount <= MAX_VERTICES);
		assert(indexCount <= MAX_INDICES);
		return {vertexCount, indexCount};
	}
	
	
};
