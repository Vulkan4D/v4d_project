#pragma once

typedef std::unordered_map<uint32_t /* id */, v4d::scene::NetworkGameObjectPtr> ClientObjects;

namespace OBJECT_TYPE {
	const uint32_t Player = 0;
	const uint32_t CornellBox = 1;
	const uint32_t Ball = 2;
	const uint32_t Light = 3;
}

struct PlayerView {
	std::mutex mu;
	double camSpeed = 10.0, mouseSensitivity = 1.0, tiltSpeed = 2.0;
	double horizontalAngle = 0;
	double verticalAngle = 0;
	glm::dvec3 worldPosition {0};
	glm::dvec3 velocity {0};
	glm::dvec3 viewUp = {0,0,1};
	glm::dvec3 viewForward {0,1,0};
	glm::dvec3 viewRight = glm::cross(viewForward, viewUp);
	glm::dvec3 viewUpTarget = viewUp;
	glm::dvec3 viewForwardTarget = viewForward;
	glm::dvec3 viewRightTarget = viewRight;
	bool useFreeFlyCam = true;
	float flyCamSmoothness = 25.0;
	glm::dmat4 freeFlyCamRotationMatrix {1};
};
