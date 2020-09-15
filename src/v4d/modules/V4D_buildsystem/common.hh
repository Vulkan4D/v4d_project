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
	NONE = 0,
	PLUS_X,
	MINUS_X,
	PLUS_Y,
	MINUS_Y,
	PLUS_Z,
	MINUS_Z,
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
			
			,sizePlusX(size*3)
			,sizePlusY(size*3)
			,sizePlusZ(size*3)
			,sizeMinusX(size*3)
			,sizeMinusY(size*3)
			,sizeMinusZ(size*3)
			
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
	
	glm::i16vec3 GetPosition() const {
		return glm::i16vec3(data.posX, data.posY, data.posZ);
	};
	
	SHAPE const GetShapeType() const {
		return (SHAPE)data.shape;
	};
	
public:
	Block() = default;
	Block(SHAPE t) : data(t) {}
	
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
			case SHAPE::SLOPE: return {};
			case SHAPE::CORNER: return {};
			case SHAPE::PYRAMID: return {};
			case SHAPE::INVCORNER: return {};
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
			case SHAPE::SLOPE: return {};
			case SHAPE::CORNER: return {};
			case SHAPE::PYRAMID: return {};
			case SHAPE::INVCORNER: return {};
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
			case SHAPE::SLOPE: return {};
			case SHAPE::CORNER: return {};
			case SHAPE::PYRAMID: return {};
			case SHAPE::INVCORNER: return {};
			case SHAPE::_EXTRA1: return {};
			case SHAPE::_EXTRA2: return {};
			case SHAPE::_EXTRA3: return {};
		}
		return {};
	};
	
	struct FaceVertex {
		uint8_t vertexIndex;
		v4d::scene::Geometry::VertexBuffer_T* vertexData;
	};
	
	std::tuple<uint/* vertexCount */, uint/* indexCount */> 
	GenerateGeometry(
		v4d::scene::Geometry::VertexBuffer_T* outputVertices, 
		v4d::scene::Geometry::IndexBuffer_T* outputIndices
	) {
		uint vertexCount = 0, indexCount = 0;
		auto points = GetPoints();
		// Resize shape
		for (const auto& face : GetFaces()) {
			for (const auto& p : face.resizePoints) {
				switch (face.resizedir) {
					case RESIZEDIR::PLUS_X:
						points[p] *= data.sizePlusX + 1; // +1 so that the range 0-255 becomes 1-256, then we divide by 8 below to get a range between 0.125 and 32.0 so that we end up with block sizes between 0.25 and 64.0
						break;
					case RESIZEDIR::MINUS_X:
						points[p] *= data.sizeMinusX + 1;
						break;
					case RESIZEDIR::PLUS_Y:
						points[p] *= data.sizePlusY + 1;
						break;
					case RESIZEDIR::MINUS_Y:
						points[p] *= data.sizeMinusY + 1;
						break;
					case RESIZEDIR::PLUS_Z:
						points[p] *= data.sizePlusZ + 1;
						break;
					case RESIZEDIR::MINUS_Z:
						points[p] *= data.sizeMinusZ + 1;
						break;
					default:break;
				}
			}
		}
		// Generate vertices and indices
		std::map<uint8_t/*faceAndPointIndex*/, FaceVertex> faceVertices {};
		int faceIndex = 0;
		for (const auto& face : GetFaces()) {
			assert(faceIndex < MAX_FACES);
			// Normal
			glm::vec3 faceNormal;
			switch (face.resizedir) {
				case RESIZEDIR::PLUS_X:
					faceNormal = {1,0,0};
					break;
				case RESIZEDIR::MINUS_X:
					faceNormal = {-1,0,0};
					break;
				case RESIZEDIR::PLUS_Y:
					faceNormal = {0,1,0};
					break;
				case RESIZEDIR::MINUS_Y:
					faceNormal = {0,-1,0};
					break;
				case RESIZEDIR::PLUS_Z:
					faceNormal = {0,0,1};
					break;
				case RESIZEDIR::MINUS_Z:
					faceNormal = {0,0,-1};
					break;
			}
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
					faceVertex->vertexData->pos = points[pointIndex] / 8.0f; // Grid size of 1/8 meters
					faceVertex->vertexData->normal = faceNormal;
					//TODO faceVertex->vertexData->SetUV()
					//TODO adjust block orientation (affects pos and normal)
					//TODO offset block position (affects pos)
					auto color = COLORS[GetColorIndex(data.useVertexColorGradients? pointIndex : faceIndex)];
					float alpha = 1.0; //TODO get alpha from material
					faceVertex->vertexData->SetColor({color.r, color.g, color.b, alpha});
				}
				outputIndices[indexCount++] = faceVertex->vertexIndex;
			}
			++faceIndex;
		}
		assert(vertexCount <= MAX_VERTICES);
		assert(indexCount <= MAX_INDICES);
		return {vertexCount, indexCount};
	}
	
	
};
