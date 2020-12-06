layout(set = 1, binding = 0, rgba16f) uniform readonly image2D img_thumbnail;
layout(set = 1, binding = 1) buffer writeonly OutputData {
	vec4 buffer_totalLuminance;
};
