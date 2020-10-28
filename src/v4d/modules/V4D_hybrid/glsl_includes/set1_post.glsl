layout(set = 1, binding = 0) uniform sampler2D tex_img_lit;
layout(set = 1, binding = 1) uniform sampler2D tex_img_overlay;
layout(set = 1, binding = 2) uniform sampler2D tex_img_history; // previous frame
layout(set = 1, binding = 3) uniform sampler2D tex_img_depth;
layout(set = 1, binding = 4) uniform sampler2D tex_img_rasterDepth;
layout(set = 1, binding = 5, input_attachment_index = 0) uniform highp subpassInput in_img_pp;

	// This is there mostly for debugging them.... may remove if it affects performance
	layout(set = 1, binding = 6) uniform sampler2D img_gBuffer_0; // R=snorm8(metallic), G=snorm8(roughness)
	layout(set = 1, binding = 7) uniform sampler2D img_gBuffer_1; // RGB=sfloat32(normal.xyz), A=sfloat32(packed(uv))
	layout(set = 1, binding = 8) uniform sampler2D img_gBuffer_2; // RGB=sfloat32(viewPosition.xyz), A=sfloat32(distance)
	layout(set = 1, binding = 9) uniform sampler2D img_gBuffer_3; // RGB=snorm16(albedo.rgb), A=snorm16(emit?)
	layout(set = 1, binding = 10) uniform sampler2D img_gBuffer_4; // R=objectIndex24+customType8, G=flags32, B=customData32, A=customData32
