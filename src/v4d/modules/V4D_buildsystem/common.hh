#include <v4d.h>

enum class SHAPE : int {
	CUBE = 0,
	SLOPE,
	CORNER,
	PYRAMID,
	INVCORNER,
	_EXTRA1, // inverted pyramid ?
	_EXTRA2,
	_EXTRA3,
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

struct BlockFace {
	std::vector<uint8_t> triangles {};
	RESIZEDIR resizedir = RESIZEDIR::NONE;
	std::vector<uint8_t> resizePoints {};
};

class Block {
protected:
	static const int MAX_POINTS = 8;
	static const int MAX_FACES = 7;
	static const int MAX_LINES = 12;
	
	// 32 Bytes
	struct BlockData {
		BlockData() = default;
		BlockData(SHAPE t) : shape((int)t) {}
		
		// 4 bytes ident
			uint32_t shape : 3; // 8 possible shapes, only 5 used
			uint32_t orientation : 5; // 32 possible orientations, only 24 used
			uint32_t index : 24; // shall reset to 0 upon game load
		
		// 6 bytes size		2x uvec3(0.25 to 64.0)
			uint8_t sizePlusX;
			uint8_t sizePlusY;
			uint8_t sizePlusZ;
			uint8_t sizeMinuxX;
			uint8_t sizeMinuxY;
			uint8_t sizeMinuxZ;
		
		// 6 bytes position		ivec3(-8km to +8km, +- 25cm)
			int16_t posX;
			int16_t posY;
			int16_t posZ;
		
		// 8 bytes colors (128 color palette, different color for each face or each point)
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
			// 8 materials (with 32 possibilities) for 1 structure and 7 faces
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
			// extra attributes for future use (may also be a 4-bit uint with 16 possibilities)
			uint64_t _extra_attribute_0 : 1;
			uint64_t _extra_attribute_1 : 1;
			uint64_t _extra_attribute_2 : 1;
			uint64_t _extra_attribute_3 : 1;
			
	} data;
	
	glm::i16vec3 GetPosition() const {
		return glm::i16vec3(data.posX, data.posY, data.posZ);
	};
	
	SHAPE const GetShape() const {
		return (SHAPE)data.shape;
	};
	
	Block() = default;
	Block(SHAPE t) : data(t) {}
	
	std::vector<glm::vec3> const GetPoints() const {
		switch (GetShape()) {
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
		switch (GetShape()) {
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
	
	
	
	
};
