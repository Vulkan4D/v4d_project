struct RayCast {
	// from img_gBuffer_1
	glm::vec3 normal;
	union {
		float _uv;
		uint32_t customData0;
	};
	// from img_gBuffer_2
	glm::vec3 position;
	float distance;
	// from img_gBuffer_4
	uint32_t hit : 1;
	uint32_t objectBufferOffset : 23;
	uint32_t customType : 8;
	uint32_t : 0;
	uint32_t flags;
	uint32_t customData1;
	uint32_t customData2;
	
	RayCast() : hit(0) { static_assert(sizeof(RayCast) == sizeof(glm::vec4) * 3); }
	
	glm::vec2 GetUV() {
		return v4d::graphics::UnpackUVfromFloat(_uv);
	}
	
};
