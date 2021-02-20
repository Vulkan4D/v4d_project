#include "v4d/modules/V4D_raytracing/glsl_includes/pl_raster.glsl"

struct V2F {
	vec4 color;
	vec3 normal;
};

####################################################################
#shader vert

layout(location = 0) out V2F v2f;

void main() {
	uint index = GetIndex(0);
	v2f.color = GetVertexColor(index);
	v2f.normal = GetModelNormalViewMatrix() * GetVertexNormal(index);
	gl_Position = mat4(camera.projectionMatrix) * vec4((GetModelViewMatrix() * vec4(GetVertexPosition(index), 1)).xyz, 1);
}


##################################################################
#shader transparent.frag

layout(location = 0) in V2F v2f;

void main() {
	float depth = texture(tex_img_depth, gl_FragCoord.xy/vec2(camera.width, camera.height)).r;
	if (depth > gl_FragCoord.z+0.0000001)
		discard;
	out_img_lit = v2f.color;
}


##################################################################
#shader wireframe.frag

layout(location = 0) in V2F v2f;

void main() {
	float depth = texture(tex_img_depth, gl_FragCoord.xy/vec2(camera.width, camera.height)).r;
	if (depth > gl_FragCoord.z+0.0000001)
		discard;
	out_img_lit = wireframeColor * max(0.2, dot(vec3(0,0,1), v2f.normal) / 2.0 + 0.5);
}
