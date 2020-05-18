#include "core.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_thumbnail.glsl"

struct V2F {
    vec2 uv;
};

##################################################################
#shader vert

layout (location = 0) out V2F v2f;

void main() 
{
    v2f.uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(v2f.uv * 2.0f + -1.0f, 0.0f, 1.0f);
}

##################################################################
#shader frag

layout(location = 0) in V2F v2f;

void main() {
	out_img_thumbnail = vec4(texture(tex_img_lit, v2f.uv).rgb, 1.0);
}
