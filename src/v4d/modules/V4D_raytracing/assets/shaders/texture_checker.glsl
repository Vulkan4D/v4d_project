#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

layout(location = CALL_DATA_LOCATION_TEXTURE) callableDataInEXT ProceduralTextureCall tex;

#shader rcall
void main() {
    tex.color.rgb *= mod(floor(tex.position.x) + floor(tex.position.z), 2.0);
}
