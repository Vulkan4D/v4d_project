#pragma once
#include <v4d.h>

namespace OBJECT_TYPE {
	const uint32_t Build = 0;
}

const int NB_BLOCKS = 5;

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

enum class FACEDIR {
	NONE = 0,
	PLUS_X,
	MINUS_X,
	PLUS_Y,
	MINUS_Y,
	PLUS_Z,
	MINUS_Z,
};

static constexpr int NB_ORIENTATIONS = 24;
static const glm::vec4 ORIENTATIONS[NB_ORIENTATIONS] = /*{Axis, Angle}*/ { // https://www.euclideanspace.com/maths/geometry/rotations/axisAngle/examples/index.htm
	{1,0,0,  0},   {0,1,0,  90},   {0,1,0,  180},   {0,1,0,  -90},
	{0,0,1,  90},   {0.5774,0.5774,0.5774,  120},   {0.7071,0.7071,0,  180},   {-0.5774,-0.5774,0.5774,  120},
	{0,0,-1,  90},   {-0.5774,0.5774,-0.5774,  120},   {-0.7071,0.7071,0,  180},   {0.5774,-0.5774,-0.5774,  120},
	{1,0,0,  90},   {0.5774,0.5774,-0.5774,  120},   {0,0.7071,-0.7071,  180},   {0.5774,-0.5774,0.5774,  120},
	{1,0,0,  180},   {0.7071,0,-0.7071,  180},   {0,0,1,  180},   {0.7071,0,0.7071,  180},
	{-1,0,0,  90},   {-0.5774,0.5774,0.5774,  120},   {0,0.7071,0.7071,  180},   {-0.5774,-0.5774,-0.5774,  120},
};

static const glm::vec3 COLORS[128] = {
	{0.5, 0.5, 0.5}, // grey
	{1.0, 0.0, 0.0}, // red
	{0.0, 1.0, 0.0}, // green
	{0.0, 0.0, 1.0}, // blue
	{0.0, 1.0, 1.0}, // yellow
	{1.0, 0.0, 1.0}, // pink
	{1.0, 1.0, 1.0}, // white
	// ...
};
static constexpr int BLOCK_COLOR_GREY = 0;
static constexpr int BLOCK_COLOR_RED = 1;
static constexpr int BLOCK_COLOR_GREEN = 2;
static constexpr int BLOCK_COLOR_BLUE = 3;
static constexpr int BLOCK_COLOR_YELLOW = 4;

struct BlockFace {
	std::vector<uint8_t> triangles {};
	std::vector<FACEDIR> facedirs {};
	bool canAddBlock {};
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
	std::unordered_map<v4d::scene::NetworkGameObject::Id, std::vector<Block>> buildBlocks {};
	std::unordered_map<v4d::scene::NetworkGameObject::Id, std::shared_ptr<Build>> builds {};

	// Server-Only
	std::mutex serverObjectMapsMutex;
	std::unordered_map<v4d::scene::NetworkGameObject::Id, std::vector<Block>> serverBuildBlocks {};

};
