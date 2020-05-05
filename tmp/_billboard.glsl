/* Example Usage :

//#shader vert
	layout(location = 0) in vec4 posr;
	void main() {
		gl_Position = posr;
	}

//#shader geom
	layout(set = 0, binding = 0) uniform vec3 cameraPosition;
	layout (points) in;
	layout (triangle_strip, max_vertices = 4) out;
	layout(location = 0) out vec3 out_lpos;
	//#include "_billboard.glsl"
	void main(void) {
		GeomEmitBillboardFromPoint(cameraPosition);
	}

//#shader frag
	layout(location = 0) in vec3 in_lpos;
	layout(location = 0) out vec4 o_color;
	//#include "_noise.glsl"
	void main() {
		o_color = vec4(snoise(vec4(in_lpos,0), 10));
	}

*/

void GeomEmitBillboard(vec3 wpos, vec3 up, vec3 right) {
	
	out_lpos = up - right;
	gl_Position = mat4(ubo.proj) * mat4(ubo.view) * vec4(wpos + out_lpos, 1.0);
	EmitVertex();
	
	out_lpos = -up - right;
	gl_Position = mat4(ubo.proj) * mat4(ubo.view) * vec4(wpos + out_lpos, 1.0);
	EmitVertex();
	
	out_lpos = up + right;
	gl_Position = mat4(ubo.proj) * mat4(ubo.view) * vec4(wpos + out_lpos, 1.0);
	EmitVertex();
	
	out_lpos = -up + right;
	gl_Position = mat4(ubo.proj) * mat4(ubo.view) * vec4(wpos + out_lpos, 1.0);
	EmitVertex();
	
	EndPrimitive();
}

void GeomEmitBillboardFromPoint(vec3 cameraPosition) {
	// Get center position and radius
	vec3 wpos = gl_in[0].gl_Position.xyz;
	float radius = gl_in[0].gl_Position.w;
	
	// Get stretch directions relative to camera view
	vec3 toCamera = normalize(cameraPosition - wpos);
	vec3 up = vec3(0, 0, 1);
	vec3 right = normalize(cross(up, toCamera));
	up = -cross(right, toCamera);
	
	// Emit one billboard, always facing the camera, with specified size based on vertex W as radius
	GeomEmitBillboard(wpos, up * radius, right * radius);
}
