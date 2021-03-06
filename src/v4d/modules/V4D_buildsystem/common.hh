#pragma once
#include <v4d.h>

namespace OBJECT_TYPE {
	const uint32_t Build = 0;
}

#pragma region Block shapes

	const int NB_BLOCKS = 5; // could be up to 8 in the future

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

#pragma endregion

#pragma region Faces

	enum class FACEDIR {
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
		std::vector<FACEDIR> facedirs {};
		bool canAddBlock {};
	};

#pragma endregion

#pragma region Orientations

	static constexpr int NB_ORIENTATIONS = 24;
	
	static const glm::vec4 ORIENTATIONS[NB_ORIENTATIONS] = /*{Axis, Angle}*/ { // https://www.euclideanspace.com/maths/geometry/rotations/axisAngle/examples/index.htm
		{1,0,0,  0},   {0,1,0,  90},   {0,1,0,  180},   {0,1,0,  -90},
		{0,0,1,  90},   {0.5774,0.5774,0.5774,  120},   {0.7071,0.7071,0,  180},   {-0.5774,-0.5774,0.5774,  120},
		{0,0,-1,  90},   {-0.5774,0.5774,-0.5774,  120},   {-0.7071,0.7071,0,  180},   {0.5774,-0.5774,-0.5774,  120},
		{1,0,0,  90},   {0.5774,0.5774,-0.5774,  120},   {0,0.7071,-0.7071,  180},   {0.5774,-0.5774,0.5774,  120},
		{1,0,0,  180},   {0.7071,0,-0.7071,  180},   {0,0,1,  180},   {0.7071,0,0.7071,  180},
		{-1,0,0,  90},   {-0.5774,0.5774,0.5774,  120},   {0,0.7071,0.7071,  180},   {-0.5774,-0.5774,-0.5774,  120},
	};

#pragma endregion

#pragma region Colors

	static constexpr int NB_COLORS = 9; //TODO define 128 colors
	
	static const glm::vec3 COLORS[NB_COLORS] = {
		{0.7, 0.7, 0.7}, // grey
		{1.0, 0.0, 0.0}, // red
		{0.0, 1.0, 0.0}, // green
		{0.0, 0.0, 1.0}, // blue
		{1.0, 1.0, 0.0}, // yellow
		{1.0, 0.0, 1.0}, // pink
		{0.0, 1.0, 1.0}, // turquoise
		{1.0, 1.0, 1.0}, // white
		{0.2, 0.2, 0.2}, // black
		// ...
	};
	
	static constexpr int BLOCK_COLOR_GREY = 0;
	static constexpr int BLOCK_COLOR_RED = 1;
	static constexpr int BLOCK_COLOR_GREEN = 2;
	static constexpr int BLOCK_COLOR_BLUE = 3;
	static constexpr int BLOCK_COLOR_YELLOW = 4;
	static constexpr int BLOCK_COLOR_PINK = 5;
	static constexpr int BLOCK_COLOR_TURQUOISE = 6;
	static constexpr int BLOCK_COLOR_WHITE = 7;
	static constexpr int BLOCK_COLOR_BLACK = 8;
	
#pragma endregion

struct BlockLine {
	uint8_t point1;
	uint8_t point2;
	uint8_t face1;
	uint8_t face2;
};

struct PackedBlockCustomData {
	union {
		struct {
			uint32_t blockIndex : 24;
			uint32_t faceIndex : 3;
			uint32_t materialId : 5;
		};
		uint32_t packed;
	};
};

#include "utils/Block.hpp"
#include "utils/Build.hpp"
#include "utils/TmpBlock.hpp"
#include "utils/BuildInterface.hpp"

struct CachedData {
	
	// Client-Only
	std::mutex objectMapsMutex;
	std::unordered_map<uint64_t, std::vector<Block>> buildBlocks {};
	std::unordered_map<uint64_t, std::shared_ptr<Build>> builds {};

	// Server-Only
	std::mutex serverObjectMapsMutex;
	std::unordered_map<uint64_t, std::vector<Block>> serverBuildBlocks {};

};
