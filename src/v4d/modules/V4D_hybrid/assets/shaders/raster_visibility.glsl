#include "core.glsl"
#define GEOMETRY_BUFFERS_ACCESS readonly
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_raster.glsl"

// struct V2F {
// 	vec4 color;
// 	vec3 normal;
// 	vec2 uv;
// };

// ###########################################
// #shader vert

// layout(location = 0) out V2F v2f;

// void main() {
// 	Vertex vert = GetVertex();
	
// 	v2f.color = vert.color;
// 	v2f.normal = vert.normal;
// 	v2f.uv = vert.uv;
	
// 	gl_Position = mat4(camera.projectionMatrix) * v2f.pos;
// }

// ###########################################
// #shader frag.0

// layout(location = 0) in V2F v2f;

// void main() {
// 	out_img_tmpBuffer = vec2(PackColorAsFloat(v2f.color), uintBitsToFloat(geometryIndex), PackNormalAsFloat(v2f.normal), PackUVasFloat(v2f.uv));
// }

