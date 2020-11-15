#pragma once

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
	bool canChangeVelocity = true;
	
	void RefreshViewTarget() {
		viewUpTarget = glm::normalize(glm::dvec3(glm::inverse(freeFlyCamRotationMatrix) * glm::dvec4(0,1,0, 0)));
		viewForwardTarget = glm::normalize(glm::dvec3(glm::inverse(freeFlyCamRotationMatrix) * glm::dvec4(0,0,-1, 0)));
		viewRightTarget = glm::cross(viewForwardTarget, viewUpTarget);
	}
	
	void SetInitialPositionAndView(glm::dvec3 worldPosition, glm::dvec3 forward, glm::dvec3 up, bool prioritizeUp = false/* otherwise prioritize forward */) {
		this->worldPosition = worldPosition;
		auto right = glm::cross(forward, up);
		if (prioritizeUp) {
			forward = glm::cross(-right, up);
		} else {
			up = glm::cross(forward, -right);
		}
		freeFlyCamRotationMatrix = glm::lookAt({0,0,0}, forward, up);
		RefreshViewTarget();
		viewUp = viewUpTarget;
		viewForward = viewForwardTarget;
		viewRight = viewRightTarget;
	}
};
