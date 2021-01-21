#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

layout(location = CALL_DATA_LOCATION_TEXTURE) callableDataInEXT ProceduralTextureCall tex;

#shader rcall
void main() {
    tex.albedo.rgb *= mod(floor(tex.materialPayload.rayPayload.position.x) + floor(tex.materialPayload.rayPayload.position.z), 2.0);
}
