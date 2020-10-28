#include "core.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/V2F.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/V2G.glsl"

#common .*frag
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_raster.glsl"

###########################################
#shader vert

#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_raster.glsl"

layout(location = 0) out V2F v2f;

void main() {
	GeometryInstance geometry = GetGeometryInstance(geometryIndex);
	Vertex vert = GetVertex();
	
	v2f.color = vert.color;
	v2f.pos = (geometry.modelViewTransform * vec4(vert.pos, 1));
	v2f.normal = geometry.normalViewTransform * vert.normal;
	v2f.uv = vert.uv;
	
	// Calculate actual distance from camera
	v2f.pos.w = clamp(distance(vec3(camera.worldPosition), (mat4(geometry.modelTransform) * vec4(vert.pos,1)).xyz), float(camera.znear), float(camera.zfar));
	
	gl_Position = mat4(camera.projectionMatrix) * vec4(v2f.pos.xyz, 1);
}

###########################################
#shader aabb.vert

#define PROCEDURAL_GEOMETRY
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_raster.glsl"

layout(location = 0) out V2G v2g;

void main() {
	GeometryInstance geometry = GetGeometryInstance(geometryIndex);
	v2g.pos = geometry.modelViewTransform[3];
	v2g.color = GetColor();
	v2g.aabbMin = aabbMin;
	v2g.aabbMax = aabbMax;
	v2g.custom1 = custom1;
}


#shader aabb.geom
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_raster.glsl"

layout(points) in;
layout(triangle_strip, max_vertices = 25) out;

layout(location = 0) in V2G v2g[];
layout(location = 0) out vec4 out_pos;
layout(location = 1) out flat vec4 out_color;
layout(location = 2) out flat vec3 out_normal;
layout(location = 3) out flat float out_custom1;
//TODO : UVs ?

void main() {
	const vec3 cubeVertices[] = {
		vec3(v2g[0].aabbMin.x, v2g[0].aabbMin.y, v2g[0].aabbMin.z), // 0 left front bottom
		vec3(v2g[0].aabbMax.x, v2g[0].aabbMin.y, v2g[0].aabbMin.z), // 1 right front bottom
		vec3(v2g[0].aabbMin.x, v2g[0].aabbMax.y, v2g[0].aabbMin.z), // 2 left back bottom
		vec3(v2g[0].aabbMax.x, v2g[0].aabbMax.y, v2g[0].aabbMin.z), // 3 right back bottom
		vec3(v2g[0].aabbMin.x, v2g[0].aabbMin.y, v2g[0].aabbMax.z), // 4 left front top
		vec3(v2g[0].aabbMax.x, v2g[0].aabbMin.y, v2g[0].aabbMax.z), // 5 right front top
		vec3(v2g[0].aabbMin.x, v2g[0].aabbMax.y, v2g[0].aabbMax.z), // 6 left back top
		vec3(v2g[0].aabbMax.x, v2g[0].aabbMax.y, v2g[0].aabbMax.z), // 7 right back top
	};

	const int cubeIndices[] = { // frontfaces
		0,2,1,3, // bottom
		3,7,1,5, // right
		5,4,1,0, // front
		0,4,2,6, // left
		6,7,2,3, // back
	  7,7,5,6,4, // top
	};
	
	const vec3 cubeNormals[] = {
		// bottom
		vec3(0,0,-1),
		vec3(0,0,-1),
		vec3(0,0,-1),
		vec3(0,0,-1),
		// right
		vec3(1,0,0),
		vec3(1,0,0),
		vec3(1,0,0),
		vec3(1,0,0),
		// front
		vec3(0,-1,0),
		vec3(0,-1,0),
		vec3(0,-1,0),
		vec3(0,-1,0),
		// left
		vec3(-1,0,0),
		vec3(-1,0,0),
		vec3(-1,0,0),
		vec3(-1,0,0),
		// back
		vec3(0,1,0),
		vec3(0,1,0),
		vec3(0,1,0),
		vec3(0,1,0),
		// top
		vec3(0,0,1),vec3(0,0,1),
		vec3(0,0,1),
		vec3(0,0,1),
		vec3(0,0,1),
	};
	
	GeometryInstance geometry = GetGeometryInstance(geometryIndex);

	for (int i = 0; i < 25; ++i) {
		out_pos = geometry.modelViewTransform * vec4(cubeVertices[cubeIndices[i]], 1);
		out_color = v2g[0].color;
		// out_centerPos = v2g[0].pos.xyz;
		gl_Position = mat4(camera.projectionMatrix) * out_pos;
		out_normal = normalize(geometry.normalViewTransform * cubeNormals[i]);
		out_pos.w = clamp(distance(vec3(camera.worldPosition), (mat4(geometry.modelTransform) * vec4(cubeVertices[cubeIndices[i]],1)).xyz), float(camera.znear), float(camera.zfar));
		out_custom1 = v2g[0].custom1;
		EmitVertex();
	}
	
	EndPrimitive();
}


#shader sphere.geom
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_raster.glsl"

layout(points) in;
layout(triangle_strip, max_vertices = 14) out;

layout(location = 0) in V2G v2g[];
layout(location = 0) out vec4 out_pos;
layout(location = 1) out flat vec4 out_color;
layout(location = 2) out flat vec3 out_centerPos;
layout(location = 3) out flat float out_radius;

void main() {
	const vec3 cubeVertices[] = {
		vec3(v2g[0].aabbMin.x, v2g[0].aabbMin.y, v2g[0].aabbMin.z), // left front bottom
		vec3(v2g[0].aabbMax.x, v2g[0].aabbMin.y, v2g[0].aabbMin.z), // right front bottom
		vec3(v2g[0].aabbMin.x, v2g[0].aabbMax.y, v2g[0].aabbMin.z), // left back bottom
		vec3(v2g[0].aabbMax.x, v2g[0].aabbMax.y, v2g[0].aabbMin.z), // right back bottom
		vec3(v2g[0].aabbMin.x, v2g[0].aabbMin.y, v2g[0].aabbMax.z), // left front top
		vec3(v2g[0].aabbMax.x, v2g[0].aabbMin.y, v2g[0].aabbMax.z), // right front top
		vec3(v2g[0].aabbMin.x, v2g[0].aabbMax.y, v2g[0].aabbMax.z), // left back top
		vec3(v2g[0].aabbMax.x, v2g[0].aabbMax.y, v2g[0].aabbMax.z), // right back top
	};

	const int cubeIndices[] = { // Backfaces
		0, 1, 2, 3, 7, 1, 5, 4, 7, 6, 2, 4, 0, 1
	};

	for (int i = 0; i < 14; ++i) {
		out_pos = v2g[0].pos + vec4(cubeVertices[cubeIndices[i]], 0);
		out_color = v2g[0].color;
		out_radius = (v2g[0].aabbMax.x - v2g[0].aabbMin.x) / 2.0;
		out_centerPos = v2g[0].pos.xyz;
		gl_Position = mat4(camera.projectionMatrix) * out_pos;
		EmitVertex();
	}
	
	EndPrimitive();
}


###########################################
#shader basic.frag

layout(location = 0) in V2F v2f;

void main() {
	// Passthrough Material
	pbrGBuffers.viewSpacePosition = v2f.pos.xyz;
	pbrGBuffers.viewSpaceNormal = v2f.normal;
	pbrGBuffers.uv = PackUVasFloat(v2f.uv);
	pbrGBuffers.albedo = v2f.color.rgb;
	pbrGBuffers.emit = 0;
	pbrGBuffers.metallic = 0.0;
	pbrGBuffers.roughness = 0.0;
	
	pbrGBuffers.distance = v2f.pos.w;
	WritePbrGBuffers();
	WriteCustomBuffer(objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
}


###########################################
#shader standard.frag

layout(location = 0) in V2F v2f;

void main() {
	// Passthrough Material
	pbrGBuffers.viewSpacePosition = v2f.pos.xyz;
	pbrGBuffers.viewSpaceNormal = v2f.normal;
	pbrGBuffers.uv = PackUVasFloat(v2f.uv);
	pbrGBuffers.albedo = v2f.color.rgb;
	pbrGBuffers.emit = 0;
	pbrGBuffers.metallic = 0.0;
	pbrGBuffers.roughness = 0.0;
	
	pbrGBuffers.distance = v2f.pos.w;
	WritePbrGBuffers();
	WriteCustomBuffer(objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
}


###########################################
#shader terrain.frag

layout(location = 0) in V2F v2f;

void main() {
	// Passthrough Material
	pbrGBuffers.viewSpacePosition = v2f.pos.xyz;
	pbrGBuffers.viewSpaceNormal = v2f.normal;
	pbrGBuffers.uv = PackUVasFloat(v2f.uv);
	pbrGBuffers.albedo = v2f.color.rgb;
	pbrGBuffers.emit = 0;
	pbrGBuffers.metallic = 0.0;
	pbrGBuffers.roughness = 0.0;
	
	pbrGBuffers.distance = v2f.pos.w;
	WritePbrGBuffers();
	WriteCustomBuffer(objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
}


###########################################
#shader aabb.frag

layout(location = 0) in vec4 in_pos;
layout(location = 1) in flat vec4 in_color;
layout(location = 2) in flat vec3 in_normal;
layout(location = 3) in flat float in_custom1;

void main() {
	pbrGBuffers.viewSpacePosition = in_pos.xyz;
	pbrGBuffers.viewSpaceNormal = in_normal;
	pbrGBuffers.albedo = in_color.rgb;
	pbrGBuffers.metallic = 0.0;
	pbrGBuffers.roughness = 0.0;
	pbrGBuffers.emit = in_custom1;
	pbrGBuffers.uv = 0;
	pbrGBuffers.distance = in_pos.w;
	
	gl_FragDepth = GetFragDepthFromViewSpacePosition(pbrGBuffers.viewSpacePosition);
	
	WritePbrGBuffers();
	WriteCustomBuffer(objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
}


###########################################
#shader sphere.frag

layout(location = 0) in vec4 in_pos;
layout(location = 1) in flat vec4 in_color;
layout(location = 2) in flat vec3 in_centerPos;
layout(location = 3) in flat float in_radius;

void main() {
	pbrGBuffers.viewSpacePosition = in_pos.xyz;
	pbrGBuffers.viewSpaceNormal = vec3(0);
	pbrGBuffers.albedo = in_color.rgb;
	pbrGBuffers.metallic = 0.0;
	pbrGBuffers.roughness = 0.0;
	pbrGBuffers.emit = 0;
	pbrGBuffers.uv = 0;
	pbrGBuffers.distance = float(camera.znear);
	if (ProceduralSphereIntersection(in_centerPos, in_radius, pbrGBuffers.viewSpacePosition, pbrGBuffers.viewSpaceNormal, pbrGBuffers.distance)) {
		gl_FragDepth = GetFragDepthFromViewSpacePosition(pbrGBuffers.viewSpacePosition);
		WritePbrGBuffers();
		WriteCustomBuffer(objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
		return;
	}
	discard;
}


###########################################
#shader light.frag

layout(location = 0) in vec4 in_pos;
layout(location = 1) in flat vec4 in_color;
layout(location = 2) in flat vec3 in_centerPos;
layout(location = 3) in flat float in_radius;

void main() {
	float dist = length(in_centerPos);
	ProceduralGeometry geometry = GetProceduralGeometry(geometryIndex);
	LightSource light = GetLight(geometry.material);
	vec3 lightColor;
	float emission;
	if (geometry.color.a > 0) {
		lightColor = geometry.color.rgb * geometry.color.a;
		emission = light.intensity/dist/dist;
	} else {
		lightColor = light.color;
		emission = light.intensity/dist;
	}
	
	pbrGBuffers.viewSpacePosition = in_pos.xyz;
	pbrGBuffers.viewSpaceNormal = vec3(0);
	pbrGBuffers.albedo = lightColor;
	pbrGBuffers.metallic = 0;
	pbrGBuffers.roughness = 0;
	pbrGBuffers.emit = emission;
	pbrGBuffers.uv = 0;
	pbrGBuffers.distance = float(camera.znear);
	if (ProceduralSphereIntersection(in_centerPos, in_radius, pbrGBuffers.viewSpacePosition, pbrGBuffers.viewSpaceNormal, pbrGBuffers.distance)) {
		gl_FragDepth = GetFragDepthFromViewSpacePosition(pbrGBuffers.viewSpacePosition);
		pbrGBuffers.viewSpaceNormal = vec3(0);
		WritePbrGBuffers();
		WriteCustomBuffer(objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
		return;
	}
	discard;
}


###########################################
#shader sun.frag

layout(location = 0) in vec4 in_pos;
layout(location = 1) in flat vec4 in_color;
layout(location = 2) in flat vec3 in_centerPos;
layout(location = 3) in flat float in_radius;

void main() {
	float dist = length(in_centerPos);
	ProceduralGeometry geometry = GetProceduralGeometry(geometryIndex);
	LightSource light = GetLight(geometry.material);
	vec3 lightColor;
	float emission;
	if (geometry.color.a > 0) {
		lightColor = geometry.color.rgb * geometry.color.a;
		emission = light.intensity/dist/dist;
	} else {
		lightColor = light.color;
		emission = light.intensity/dist;
	}
	
	pbrGBuffers.viewSpacePosition = in_pos.xyz;
	pbrGBuffers.viewSpaceNormal = vec3(0);
	pbrGBuffers.albedo = lightColor;
	pbrGBuffers.metallic = 0;
	pbrGBuffers.roughness = 0;
	pbrGBuffers.emit = emission;
	pbrGBuffers.uv = 0;
	pbrGBuffers.distance = float(camera.znear);
	if (ProceduralSphereIntersection(in_centerPos, in_radius, pbrGBuffers.viewSpacePosition, pbrGBuffers.viewSpaceNormal, pbrGBuffers.distance)) {
		gl_FragDepth = GetFragDepthFromViewSpacePosition(pbrGBuffers.viewSpacePosition);
		pbrGBuffers.viewSpaceNormal = vec3(0);
		WritePbrGBuffers();
		WriteCustomBuffer(objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
		return;
	}
	discard;
}

