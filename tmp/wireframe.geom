/*Required
bool debugMesh = true;
vec4 meshColor = vec4(0);
bool debugNormals = true;
vec4 normalsColor = vec4(0,1,1,1);
float normalsLength = 1.0;
*/

layout(triangles) in;
layout(line_strip, max_vertices = 12) out;

layout(location = 0) in V2F in_v2f[];
layout(location = 0) out V2F out_v2f;

void main() {
	
	// Mesh
	if (debugMesh) {
		gl_Position = gl_in[0].gl_Position;
		out_v2f = in_v2f[0];
		if (meshColor.a > 0) out_v2f.color = meshColor;
		EmitVertex();
		gl_Position = gl_in[1].gl_Position;
		out_v2f = in_v2f[1];
		if (meshColor.a > 0) out_v2f.color = meshColor;
		EmitVertex();
		
		EndPrimitive();
		
		gl_Position = gl_in[1].gl_Position;
		out_v2f = in_v2f[1];
		if (meshColor.a > 0) out_v2f.color = meshColor;
		EmitVertex();
		gl_Position = gl_in[2].gl_Position;
		out_v2f = in_v2f[2];
		if (meshColor.a > 0) out_v2f.color = meshColor;
		EmitVertex();
		
		EndPrimitive();
		
		gl_Position = gl_in[2].gl_Position;
		out_v2f = in_v2f[2];
		if (meshColor.a > 0) out_v2f.color = meshColor;
		EmitVertex();
		gl_Position = gl_in[0].gl_Position;
		out_v2f = in_v2f[0];
		if (meshColor.a > 0) out_v2f.color = meshColor;
		EmitVertex();
		
		EndPrimitive();
	}
	
	// Normals
	if (debugNormals) {
		gl_Position = gl_in[0].gl_Position;
		out_v2f = in_v2f[0];
		if (normalsColor.a > 0) out_v2f.color = normalsColor;
		EmitVertex();
		gl_Position = mat4(camera.projectionMatrix) * (inverse(mat4(camera.projectionMatrix)) * gl_in[0].gl_Position + vec4(in_v2f[1].normal * normalsLength, 0));
		out_v2f = in_v2f[1];
		if (normalsColor.a > 0) out_v2f.color = normalsColor;
		EmitVertex();
		
		EndPrimitive();
		
		gl_Position = gl_in[1].gl_Position;
		out_v2f = in_v2f[1];
		if (normalsColor.a > 0) out_v2f.color = normalsColor;
		EmitVertex();
		gl_Position = mat4(camera.projectionMatrix) * (inverse(mat4(camera.projectionMatrix)) * gl_in[1].gl_Position + vec4(in_v2f[2].normal * normalsLength, 0));
		out_v2f = in_v2f[2];
		if (normalsColor.a > 0) out_v2f.color = normalsColor;
		EmitVertex();
		
		EndPrimitive();
		
		gl_Position = gl_in[2].gl_Position;
		out_v2f = in_v2f[2];
		if (normalsColor.a > 0) out_v2f.color = normalsColor;
		EmitVertex();
		gl_Position = mat4(camera.projectionMatrix) * (inverse(mat4(camera.projectionMatrix)) * gl_in[2].gl_Position + vec4(in_v2f[0].normal * normalsLength, 0));
		out_v2f = in_v2f[0];
		if (normalsColor.a > 0) out_v2f.color = normalsColor;
		EmitVertex();
		
		EndPrimitive();
	}
	
}
