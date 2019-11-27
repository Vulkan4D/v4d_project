
/* Generate a cube from 14 empty vertices
Example usage from vertex shader:
	vec3 vertexPos = GetVertexPosCube() / 2;
	gl_Position = mat4(ubo.proj) * mat4(ubo.view) * mat4(model) * vec4(vertexPos, 1);
*/
vec3 GetVertexPosCube() {
	int r = int(gl_VertexIndex > 6);
	int i = r==1 ? 13-gl_VertexIndex : gl_VertexIndex;
	int x = int(i<3 || i==4);
	int y = r ^ int(i>0 && i<4);
	int z = r ^ int(i<2 || i>5);
	return vec3(x, y, z) * 2.0 - 1.0; // returns vertex position (-1,-1,-1) to (+1,+1,+1)
}
