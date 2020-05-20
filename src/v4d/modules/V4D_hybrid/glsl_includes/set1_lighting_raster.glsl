layout(set = 1, input_attachment_index = 1, binding = 0) uniform highp subpassInput in_img_gBuffer_0; // R=snorm8(metallic), G=snorm8(roughness)
layout(set = 1, input_attachment_index = 2, binding = 1) uniform highp subpassInput in_img_gBuffer_1; // RGB=sfloat32(normal.xyz32), A=sfloat32(packed(uv16))
layout(set = 1, input_attachment_index = 3, binding = 2) uniform highp subpassInput in_img_gBuffer_2; // RGB=sfloat32(viewPosition.xyz32), A=sfloat32(realDistanceFromCamera)
layout(set = 1, input_attachment_index = 4, binding = 3) uniform highp subpassInput in_img_gBuffer_3; // RGB=snorm16(albedo.rgb16), A=snorm16(emit?)
layout(set = 1, binding = 4) uniform sampler2D tex_img_depth;
layout(set = 1, binding = 5) uniform sampler2D tex_img_rasterDepth;
