#include <v4d.h>

enum class SHAPE {
	CUBE,
	SLOPE,
	PYRAMID,
	CORNER,
	INVERTED_CORNER,
};

class Block {
protected:
	static const int MAX_VERTICES = 8;
	static const int MAX_FACES = 7;
	static const int MAX_LINES = 12;
	
	// 32 Bytes
	struct BlockData {
		
		// 4 bytes ident
			uint32_t type : 3; // 8 possible shapes, only 5 used
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
		
		// 8 bytes colors (128 color palette, different color for each face or each vertex)
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
	
};
