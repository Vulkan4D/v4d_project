layout(set = 1, input_attachment_index = 0, binding = 0) uniform highp subpassInput in_img_gBuffer_2; // RGB=sfloat32(viewPosition.xyz32), A=sfloat32(distance)
layout(set = 1, binding = 1) uniform sampler2D tex_img_depth;
layout(set = 1, binding = 2) uniform sampler2D tex_img_rasterDepth;
