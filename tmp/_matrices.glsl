
mat4 perspective(float fov, float aspect, float near, float far) {
	float tanHalfFox = tan(fov / 2);
	mat4 res = mat4(0);
	res[0][0] = 1 / (aspect * tanHalfFox);
	res[1][1] = 1 / tanHalfFox;
	res[2][2] = far / (near - far);
	res[2][3] = -1;
	res[3][2] = -(far * near) / (far - near);
	return res;
}

mat4 lookAt(vec3 eye, vec3 center, vec3 up) {
	vec3 f = vec3(normalize(center - eye));
	vec3 s = vec3(normalize(cross(f, up)));
	vec3 u = vec3(cross(s, f));
	mat4 res = mat4(1);
	res[0][0] = s.x;
	res[1][0] = s.y;
	res[2][0] = s.z;
	res[0][1] = u.x;
	res[1][1] = u.y;
	res[2][1] = u.z;
	res[0][2] =-f.x;
	res[1][2] =-f.y;
	res[2][2] =-f.z;
	res[3][0] =-dot(s, eye);
	res[3][1] =-dot(u, eye);
	res[3][2] = dot(f, eye);
	return res;
}
