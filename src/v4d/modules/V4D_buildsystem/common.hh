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

enum class RESIZEDIR {
	NONE = 0,
	PLUS_X,
	MINUS_X,
	PLUS_Y,
	MINUS_Y,
	PLUS_Z,
	MINUS_Z,
};

glm::vec4 ORIENTATIONS[24] = /*{Axis, Angle}*/ { // https://www.euclideanspace.com/maths/geometry/rotations/axisAngle/examples/index.htm
	{1,0,0,  0},   {0,1,0,  90},   {0,1,0,  180},   {0,1,0,  -90},
	{0,0,1,  90},   {0.5774,0.5774,0.5774,  120},   {0.7071,0.7071,0,  180},   {-0.5774,-0.5774,0.5774,  120},
	{0,0,-1,  90},   {-0.5774,0.5774,-0.5774,  120},   {-0.7071,0.7071,0,  180},   {0.5774,-0.5774,-0.5774,  120},
	{1,0,0,  90},   {0.5774,0.5774,-0.5774,  120},   {0,0.7071,-0.7071,  180},   {0.5774,-0.5774,0.5774,  120},
	{1,0,0,  180},   {0.7071,0,-0.7071,  180},   {0,0,1,  180},   {0.7071,0,0.7071,  180},
	{-1,0,0,  90},   {-0.5774,0.5774,0.5774,  120},   {0,0.7071,0.7071,  180},   {-0.5774,-0.5774,-0.5774,  120},
};

const glm::vec3 COLORS[128] = {
	{0.5, 0.5, 0.5}, // grey
	{1.0, 0.0, 0.0}, // red
	{0.0, 1.0, 0.0}, // green
	{0.0, 0.0, 1.0}, // blue
	{0.0, 1.0, 1.0}, // yellow
	{1.0, 0.0, 1.0}, // pink
	{1.0, 1.0, 1.0}, // white
	// ...
};

struct BlockFace {
	std::vector<uint8_t> triangles {};
	std::vector<RESIZEDIR> resizedirs = {};
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
