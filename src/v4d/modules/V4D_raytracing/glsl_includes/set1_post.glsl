layout(set = 1, binding = 0) uniform sampler2D tex_img_lit;
layout(set = 1, binding = 1) uniform sampler2D tex_img_overlay;
layout(set = 1, binding = 2) uniform sampler2D tex_img_history; // previous frame
layout(set = 1, binding = 3) uniform sampler2D tex_img_depth;
layout(set = 1, binding = 4, input_attachment_index = 0) uniform highp subpassInput in_img_pp;
