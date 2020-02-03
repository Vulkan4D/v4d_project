#pragma once

#include <v4d.h>

namespace CubeToSphere {
	
	enum FACE : int {
		FRONT,
		BACK,
		RIGHT,
		LEFT,
		TOP,
		BOTTOM
	};

	constexpr std::tuple<glm::dvec3, glm::dvec3, glm::dvec3> GetFaceVectors(int face) {
		glm::dvec3 dir {0};
		glm::dvec3 top {0};
		glm::dvec3 right {0};
		switch (face) {
			case FRONT:
				dir = glm::dvec3(0, 0, 1);
				top = glm::dvec3(0, 1, 0);
				right = glm::dvec3(1, 0, 0);
				break;
			case LEFT:
				dir = glm::dvec3(-1, 0, 0);
				top = glm::dvec3(0, -1, 0);
				right = glm::dvec3(0, 0, -1);
				break;
			case BOTTOM:
				dir = glm::dvec3(0, -1, 0);
				top = glm::dvec3(0, 0, -1);
				right = glm::dvec3(-1, 0, 0);
				break;
			case BACK:
				dir = glm::dvec3(0, 0, -1);
				top = glm::dvec3(0, 1, 0);
				right = glm::dvec3(-1, 0, 0);
				break;
			case RIGHT:
				dir = glm::dvec3(1, 0, 0);
				top = glm::dvec3(0, 1, 0);
				right = glm::dvec3(0, 0, -1);
				break;
			case TOP:
				dir = glm::dvec3(0, 1, 0);
				top = glm::dvec3(0, 0, 1);
				right = glm::dvec3(-1, 0, 0);
				break;
		}
		return {dir, top, right};
	}
	
	glm::dvec3 Spherify(glm::dvec3 point, int face) {
		glm::dvec2 pos;
		switch (face) {
		case FRONT:
			pos = {point.x, point.y};
			break;
		case LEFT:
			pos = {point.z, point.y};
			break;
		case BOTTOM:
			pos = {point.x, point.z};
			break;
		case BACK:
			pos = {point.x, point.y};
			break;
		case RIGHT:
			pos = {point.z, point.y};
			break;
		case TOP:
			pos = {point.x, point.z};
			break;
		}
		
		// cube-sphere projection algorithm		http://jcgt.org/published/0007/02/01/paper.pdf
		#if 0
			// Arvo's (100% accurate but asymetrical and sometimes non-continuous with other edges)
			double tan_a_term = glm::tan(pos.x * 0.523598775598);
			double cos_a_term = glm::cos(pos.x * 1.0471975512);
			pos = glm::vec2(1.41421356237 * tan_a_term / glm::sqrt(1.0 - tan_a_term*tan_a_term), pos.y / glm::sqrt(1.0 + (1.0 - pos.y*pos.y)*cos_a_term));
		#else
			// Cobe's (less accurate but symmetrical and continuous)
			glm::dvec2 y = {pos.y, pos.x};
			glm::dvec2 x2 = pos*pos, y2=y*y;
			glm::dvec2 bsum = ( (-0.0941180085824 + 0.0409125981187*y2 - 0.0623272690881*x2)*x2 + (0.0275922480902 + 0.0342217026979*y2)*y2 );
			pos *= 0.723951234952 + 0.276048765048*x2 + (1.0 - x2)*bsum;
		#endif
		
		switch (face) {
		case FRONT:
			point.x = pos.x;
			point.y = pos.y;
			break;
		case LEFT:
			point.z = pos.x;
			point.y = pos.y;
			break;
		case BOTTOM:
			point.x = pos.x;
			point.z = pos.y;
			break;
		case BACK:
			point.x = pos.x;
			point.y = pos.y;
			break;
		case RIGHT:
			point.z = pos.x;
			point.y = pos.y;
			break;
		case TOP:
			point.x = pos.x;
			point.z = pos.y;
			break;
		}
		
		return glm::normalize(point);
	}

}
