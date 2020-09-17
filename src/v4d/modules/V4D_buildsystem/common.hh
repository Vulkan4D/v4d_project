#include <v4d.h>

enum class SHAPE : int {
	CUBE = 0,
	SLOPE,
	CORNER,
	PYRAMID,
	INVCORNER,
	_EXTRA1, // inverted pyramid ?
	_EXTRA2, // cone ?
	_EXTRA3, // cylinder ?
};

enum class RESIZEDIR {
	NONE = -1,
	PLUS_X = 0,
	MINUS_X = 1,
	PLUS_Y = 2,
	MINUS_Y = 3,
	PLUS_Z = 4,
	MINUS_Z = 5,
};

const glm::vec3 COLORS[128] = {
	{1.0, 0.0, 0.0}, // red
	{0.0, 1.0, 0.0}, // green
	{0.0, 0.0, 1.0}, // blue
	// ...
};

struct BlockFace {
	std::vector<uint8_t> triangles {};
	RESIZEDIR resizedir = RESIZEDIR::NONE;
	std::vector<uint8_t> resizePoints {};
};

class Block {
public:
	static const int MAX_POINTS = 8;
	static const int MAX_FACES = 7;
	static const int MAX_LINES = 12;
	static const int MAX_VERTICES = 24;
	static const int MAX_INDICES = 36;
protected:
	
	// 32 Bytes
	struct BlockData {
		BlockData() = default;
		BlockData(SHAPE t, uint32_t index = 0, float size = 1.0f)
			:index(index)
			,shape((int)t)
			,orientation(0)
			
			,sizePlusX(size*4 - 1)
			,sizePlusY(size*4 - 1)
			,sizePlusZ(size*4 - 1)
			,sizeMinusX(size*4 - 1)
			,sizeMinusY(size*4 - 1)
			,sizeMinusZ(size*4 - 1)
			
			,posX(0)
			,posY(0)
			,posZ(0)
			
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
			
			,_extra_attributes(0)
		{}
		
		// 4 bytes ident
			uint32_t index : 24; // shall reset to 0 upon game load
			uint32_t shape : 3; // 8 possible shapes, only 5 used
			uint32_t orientation : 5; // 32 possible orientations, only 24 used
		
		// 6 bytes size		2x uvec3 (0 = 0.25  to  255 = 64.0)
			uint8_t sizePlusX;
			uint8_t sizePlusY;
			uint8_t sizePlusZ;
			uint8_t sizeMinusX;
			uint8_t sizeMinusY;
			uint8_t sizeMinusZ;
		
		// 6 bytes position		ivec3(-8km to +8km, +- 25cm)
			int16_t posX;
			int16_t posY;
			int16_t posZ;
		
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
			uint64_t _extra_attributes : 4;
	} data;
	
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
	
	glm::i16vec3 GetRawPosition() const {
		return glm::i16vec3(data.posX, data.posY, data.posZ);
	};
	
	void SetRawPosition(glm::i16vec3 pos) {
		data.posX = pos.x;
		data.posY = pos.y;
		data.posZ = pos.z;
	}
	
	SHAPE const GetShapeType() const {
		return (SHAPE)data.shape;
	};
	
public:
	Block() = default;
	Block(SHAPE t) : data(t) {}
	
	// Grid size of 1/4 meters for block positions
	void SetPosition(glm::vec3 pos) {
		data.posX = pos.x * 4.0f;
		data.posY = pos.y * 4.0f;
		data.posZ = pos.z * 4.0f;
	}
	glm::vec3 GetPosition() const {
		return glm::vec3(data.posX, data.posY, data.posZ) / 4.0f;
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
				{ // top
					{0,1,2, 2,3,0},
					RESIZEDIR::PLUS_Y,
					{0,1,2,3}
				},
				{ // bottom
					{7,6,5, 5,4,7},
					RESIZEDIR::MINUS_Y,
					{4,5,6,7}
				},
				{ // right
					{0,3,7, 7,4,0},
					RESIZEDIR::PLUS_X,
					{0,3,4,7}
				},
				{ // left
					{2,1,5, 5,6,2},
					RESIZEDIR::MINUS_X,
					{1,2,5,6}
				},
				{ // front
					{0,4,5, 5,1,0},
					RESIZEDIR::PLUS_Z,
					{0,1,4,5}
				},
				{ // back
					{2,6,7, 7,3,2},
					RESIZEDIR::MINUS_Z,
					{2,3,6,7}
				},
			};
			case SHAPE::SLOPE: return {
				{ // slope
					{0,1,2, 2,3,0},
					RESIZEDIR::NONE,
					{}
				},
				{ // right
					{0,3,4},
					RESIZEDIR::PLUS_X,
					{0,3,4}
				},
				{ // left
					{1,5,2},
					RESIZEDIR::MINUS_X,
					{1,5,2}
				},
				{ // front
					{0,4,1, 1,4,5},
					RESIZEDIR::PLUS_Z,
					{0,1,4,5}
				},
				{ // bottom
					{4,3,5, 5,3,2},
					RESIZEDIR::MINUS_Y,
					{2,3,4,5}
				},
			};
			case SHAPE::CORNER: return {
				{ // slope
					{0,1,2},
					RESIZEDIR::NONE,
					{}
				},
				{ // right
					{0,2,3},
					RESIZEDIR::PLUS_X,
					{0,2,3}
				},
				{ // front
					{0,3,1},
					RESIZEDIR::PLUS_Z,
					{0,1,3}
				},
				{ // bottom
					{2,1,3},
					RESIZEDIR::MINUS_Y,
					{2,1,3}
				},
			};
			case SHAPE::PYRAMID: return {
				{ // back slope
					{0,1,2},
					RESIZEDIR::NONE,
					{}
				},
				{ // left slope
					{0,4,1},
					RESIZEDIR::NONE,
					{}
				},
				{ // right
					{0,2,3},
					RESIZEDIR::PLUS_X,
					{0,2,3}
				},
				{ // front
					{0,3,4},
					RESIZEDIR::PLUS_Z,
					{0,3,4}
				},
				{ // bottom
					{1,4,2, 2,4,3},
					RESIZEDIR::MINUS_Y,
					{1,2,3,4}
				},
			};
			case SHAPE::INVCORNER: return {
				{ // top
					{0,1,2},
					RESIZEDIR::PLUS_Y,
					{0,1,2}
				},
				{ // bottom
					{6,5,4, 4,3,6},
					RESIZEDIR::MINUS_Y,
					{3,4,5,6}
				},
				{ // right
					{0,6,3},
					RESIZEDIR::PLUS_X,
					{0,6,3}
				},
				{ // left
					{2,1,5, 5,1,4},
					RESIZEDIR::MINUS_X,
					{1,2,4,5}
				},
				{ // front
					{0,3,1, 1,3,4},
					RESIZEDIR::PLUS_Z,
					{0,1,3,4}
				},
				{ // back
					{2,5,6},
					RESIZEDIR::MINUS_Z,
					{2,5,6}
				},
				{ // slope
					{0,2,6},
					RESIZEDIR::NONE,
					{}
				},
			};
			case SHAPE::_EXTRA1: return {};
			case SHAPE::_EXTRA2: return {};
			case SHAPE::_EXTRA3: return {};
		}
		return {};
	};
	
	std::vector<glm::uvec2> const GetLines() const {
		switch (GetShapeType()) {
			case SHAPE::CUBE: return {
				{0,1}, // top front
				{2,3}, // top back
				{4,5}, // bottom front
				{6,7}, // bottom back
				{1,2}, // top left
				{0,3}, // top right
				{5,6}, // bottom left
				{4,7}, // bottom right
				{1,5}, // front left
				{2,6}, // back left
				{0,4}, // front right
				{3,7}, // back right
			};
			case SHAPE::SLOPE: return {
				{0,1}, // top
				{1,2}, // left slope
				{0,3}, // right slope
				{2,3}, // back
				{4,5}, // front bottom
				{0,4}, // front right
				{3,4}, // bottom right
				{1,5}, // front left
				{2,5}, // bottom left
			};
			case SHAPE::CORNER: return {
				{0,1},
				{1,2},
				{0,2},
				{0,3},
				{1,3},
				{2,3},
			};
			case SHAPE::PYRAMID: return {
				{0,1},
				{0,4},
				{0,2},
				{0,3},
				{1,2},
				{2,3},
				{1,4},
				{3,4},
			};
			case SHAPE::INVCORNER: return {
				{0,1}, // top front
				{1,2}, // top left
				{0,2}, // top slope
				{2,6}, // back slope
				{0,6}, // right slope
				{2,5}, // back left
				{5,6}, // back bottom
				{0,3}, // front right
				{3,6}, // bottom right
				{4,5}, // bottom left
				{3,4}, // bottom front
				{1,4}, // front left
			};
			case SHAPE::_EXTRA1: return {};
			case SHAPE::_EXTRA2: return {};
			case SHAPE::_EXTRA3: return {};
		}
		return {};
	};
	
	std::map<RESIZEDIR, std::vector<uint8_t>> const GetResizePoints() const {
		switch (GetShapeType()) {
			case SHAPE::CUBE: return {
				{RESIZEDIR::PLUS_X, {0,3,4,7}},
				{RESIZEDIR::MINUS_X, {1,2,5,6}},
				{RESIZEDIR::PLUS_Y, {0,1,2,3}},
				{RESIZEDIR::MINUS_Y, {4,5,6,7}},
				{RESIZEDIR::PLUS_Z, {0,1,4,5}},
				{RESIZEDIR::MINUS_Z, {2,3,6,7}},
			};
			case SHAPE::SLOPE: return {
				{RESIZEDIR::PLUS_X, {0,3,4}},
				{RESIZEDIR::MINUS_X, {1,5,2}},
				{RESIZEDIR::PLUS_Y, {0,1}},
				{RESIZEDIR::MINUS_Y, {2,3,4,5}},
				{RESIZEDIR::PLUS_Z, {0,1,4,5}},
				{RESIZEDIR::MINUS_Z, {2,3}},
			};
			case SHAPE::CORNER: return {
				{RESIZEDIR::PLUS_X, {0,2,3}},
				{RESIZEDIR::MINUS_X, {1}},
				{RESIZEDIR::PLUS_Y, {0}},
				{RESIZEDIR::MINUS_Y, {1,2,3}},
				{RESIZEDIR::PLUS_Z, {0,1,3}},
				{RESIZEDIR::MINUS_Z, {2}},
			};
			case SHAPE::PYRAMID: return {
				{RESIZEDIR::PLUS_X, {0,2,3}},
				{RESIZEDIR::MINUS_X, {1,4}},
				{RESIZEDIR::PLUS_Y, {0}},
				{RESIZEDIR::MINUS_Y, {1,2,3,4}},
				{RESIZEDIR::PLUS_Z, {0,3,4}},
				{RESIZEDIR::MINUS_Z, {1,2}},
			};
			case SHAPE::INVCORNER: return {
				{RESIZEDIR::PLUS_X, {0,3,6}},
				{RESIZEDIR::MINUS_X, {1,2,4,5}},
				{RESIZEDIR::PLUS_Y, {0,1,2}},
				{RESIZEDIR::MINUS_Y, {3,4,5,6}},
				{RESIZEDIR::PLUS_Z, {0,1,3,4}},
				{RESIZEDIR::MINUS_Z, {2,5,6}},
			};
			case SHAPE::_EXTRA1: return {
				{RESIZEDIR::PLUS_X, {}},
				{RESIZEDIR::MINUS_X, {}},
				{RESIZEDIR::PLUS_Y, {}},
				{RESIZEDIR::MINUS_Y, {}},
				{RESIZEDIR::PLUS_Z, {}},
				{RESIZEDIR::MINUS_Z, {}},
			};
			case SHAPE::_EXTRA2: return {
				{RESIZEDIR::PLUS_X, {}},
				{RESIZEDIR::MINUS_X, {}},
				{RESIZEDIR::PLUS_Y, {}},
				{RESIZEDIR::MINUS_Y, {}},
				{RESIZEDIR::PLUS_Z, {}},
				{RESIZEDIR::MINUS_Z, {}},
			};
			case SHAPE::_EXTRA3: return {
				{RESIZEDIR::PLUS_X, {}},
				{RESIZEDIR::MINUS_X, {}},
				{RESIZEDIR::PLUS_Y, {}},
				{RESIZEDIR::MINUS_Y, {}},
				{RESIZEDIR::PLUS_Z, {}},
				{RESIZEDIR::MINUS_Z, {}},
			};
		}
		return {};
	}
	
	struct FaceVertex {
		uint8_t vertexIndex;
		v4d::scene::Geometry::VertexBuffer_T* vertexData;
	};
	
	glm::vec3 GetFaceResizeDirNormal(RESIZEDIR dir) {
		switch (dir) {
			case RESIZEDIR::PLUS_X:
				return {1,0,0};
			case RESIZEDIR::MINUS_X:
				return {-1,0,0};
			case RESIZEDIR::PLUS_Y:
				return {0,1,0};
			case RESIZEDIR::MINUS_Y:
				return {0,-1,0};
			case RESIZEDIR::PLUS_Z:
				return {0,0,1};
			case RESIZEDIR::MINUS_Z:
				return {0,0,-1};
		}
		return {0,0,0};
	}
	
	float GetFaceSize(RESIZEDIR dir) {
		switch (dir) {
			case RESIZEDIR::PLUS_X:
				return data.sizePlusX;
			case RESIZEDIR::MINUS_X:
				return data.sizeMinusX;
			case RESIZEDIR::PLUS_Y:
				return data.sizePlusY;
			case RESIZEDIR::MINUS_Y:
				return data.sizeMinusY;
			case RESIZEDIR::PLUS_Z:
				return data.sizePlusZ;
			case RESIZEDIR::MINUS_Z:
				return data.sizeMinusZ;
		}
		return 0;
	}
	
	std::tuple<uint/* vertexCount */, uint/* indexCount */> 
	GenerateGeometry(
		v4d::scene::Geometry::VertexBuffer_T* outputVertices, 
		v4d::scene::Geometry::IndexBuffer_T* outputIndices,
		uint vertexIndexOffset = 0
	) {
		uint vertexCount = 0, indexCount = 0;
		auto points = GetPoints();
		
		// 1/8 meters grid for block sizes
		for (auto& p : points) {
			p /= 8.0f;
		}
		
		// Resize points
		for (const auto&[dir, pts] : GetResizePoints()) {
			for (const auto& p : pts) {
				points[p] += GetFaceResizeDirNormal(dir) * GetFaceSize(dir) / 8.0f;
			}
		}
		
		// Generate vertices and indices
		std::map<uint8_t/*faceAndPointIndex*/, FaceVertex> faceVertices {};
		int faceIndex = 0;
		for (const auto& face : GetFaces()) {
			assert(faceIndex < MAX_FACES);
			assert(face.triangles.size() >= 3 && (face.triangles.size() % 3) == 0);
			// Normal
			//		In the case where resizedir is NONE, we calculate the normal based on the actual position of the points because it is on a slope, we only need the first triangle of the face for calculating the normal
			//		Otherwise, we can directly take the resizedir
			glm::vec3 faceNormal = face.resizedir == RESIZEDIR::NONE ? glm::normalize(glm::cross(points[face.triangles[1]] - points[face.triangles[0]], points[face.triangles[1]] - points[face.triangles[2]])) : GetFaceResizeDirNormal(face.resizedir);
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
					faceVertex->vertexData = &outputVertices[faceVertex->vertexIndex];
					//TODO faceVertex->vertexData->SetUV()
					faceVertex->vertexData->pos = points[pointIndex];
					faceVertex->vertexData->normal = faceNormal;
					//TODO adjust block orientation (affects pos and normal)
					faceVertex->vertexData->pos += GetPosition();
					auto color = COLORS[GetColorIndex(data.useVertexColorGradients? pointIndex : faceIndex)];
					float alpha = 1.0; //TODO get alpha from material
					faceVertex->vertexData->SetColor({color.r, color.g, color.b, alpha});
				}
				outputIndices[indexCount++] = faceVertex->vertexIndex + vertexIndexOffset;
			}
			++faceIndex;
		}
		assert(vertexCount <= MAX_VERTICES);
		assert(indexCount <= MAX_INDICES);
		return {vertexCount, indexCount};
	}
	
	
};
