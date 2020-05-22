layout(set = 1, binding = 0, rg8) uniform image2D img_gBuffer_0; // R=snorm8(metallic), G=snorm8(roughness)
layout(set = 1, binding = 1, rgba32f) uniform image2D img_gBuffer_1; // RGB=sfloat32(normal.xyz), A=sfloat32(packed(uv))
layout(set = 1, binding = 2, rgba32f) uniform image2D img_gBuffer_2; // RGB=sfloat32(viewPosition.xyz), A=sfloat32(distance)
layout(set = 1, binding = 3, rgba16f) uniform image2D img_gBuffer_3; // RGB=snorm16(albedo.rgb), A=snorm16(emit?)
layout(set = 1, binding = 4, r32f) uniform image2D img_depth;
layout(set = 1, binding = 5, rgba16f) uniform image2D img_lit;
layout(set = 1, binding = 6) uniform sampler2D tex_img_rasterDepth;
