layout(set = 0, binding = 0) uniform CameraUBO {
	dmat4 origin;
	mat4 projection;
	mat4 relativeView;
	dvec4 absolutePosition;
} cameraUBO;
