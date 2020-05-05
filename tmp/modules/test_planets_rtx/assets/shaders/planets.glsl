#include "core.glsl"

#define MAX_PLANETS 1

// layout(set = 3, binding = 0) readonly buffer PlanetBuffer {dmat4 planets[];};
// layout(set = 3, binding = 0) readonly buffer PlanetBuffer {vec4 planets[];};

#############################################################

#common .*comp

#include "incubator_rendering/assets/shaders/_noise.glsl"
#include "incubator_rendering/assets/shaders/_v4dnoise.glsl"

#common .*map.comp

layout(set = 1, binding = 0, rgba32f) uniform image2D bumpMap[1];
// layout(set = 1, binding = 1) uniform writeonly imageCube mantleMap[MAX_PLANETS];
// layout(set = 1, binding = 2) uniform writeonly imageCube tectonicsMap[MAX_PLANETS];
// layout(set = 1, binding = 3) uniform writeonly imageCube heightMap[MAX_PLANETS];
// layout(set = 1, binding = 4) uniform writeonly imageCube volcanoesMap[MAX_PLANETS];
// layout(set = 1, binding = 5) uniform writeonly imageCube liquidsMap[MAX_PLANETS];

layout(std430, push_constant) uniform Planet{
	int planetIndex;
	float planetHeightVariation;
};

vec3 GetCubeDirection(writeonly imageCube image) {
	const vec2 pixelCenter = vec2(gl_GlobalInvocationID.xy) + vec2(0.5);
	const vec2 uv = pixelCenter / imageSize(image).xy;
	vec2 d = uv * 2.0 - 1.0;
	
	vec3 direction = vec3(0);
	switch (int(gl_GlobalInvocationID.z)) {
		case 0 : // Right (POSITIVE_X)
			direction = vec3( 1, -d.y, -d.x);
		break;
		case 1 : // Left (NEGATIVE_X)
			direction = vec3(-1, -d.y, d.x);
		break;
		case 2 : // Front (POSITIVE_Y)
			direction = vec3(d.x, 1, d.y);
		break;
		case 3 : // Back (NEGATIVE_Y)
			direction = vec3(d.x,-1, -d.y);
		break;
		case 4 : // Top (POSITIVE_Z)
			direction = vec3(d.x,-d.y, 1);
		break;
		case 5 : // Bottom (NEGATIVE_Z)
			direction = vec3(-d.x,-d.y, -1);
		break;
	}
	
	return normalize(direction);
}

#common terrain.*comp|terrain.rchit

layout(set = 2, binding = 0) uniform sampler2D bumpMap[1];
// layout(set = 2, binding = 1) uniform samplerCube mantleMap[MAX_PLANETS];
// layout(set = 2, binding = 2) uniform samplerCube tectonicsMap[MAX_PLANETS];
// layout(set = 2, binding = 3) uniform samplerCube heightMap[MAX_PLANETS];
// layout(set = 2, binding = 4) uniform samplerCube volcanoesMap[MAX_PLANETS];
// layout(set = 2, binding = 5) uniform samplerCube liquidsMap[MAX_PLANETS];

// #common terrain.*comp

// #include "core_buffers.glsl"

// layout(std430, push_constant) uniform TerrainChunk{
// 	int planetIndex;
// 	int chunkGeometryIndex;
// 	float solidRadius;
// 	int vertexSubdivisionsPerChunk;
// 	vec2 uvMult;
// 	vec2 uvOffset;
// 	ivec3 chunkPosition;
// 	int face;
// };

#############################################################

#shader bump.altitude.map.comp
void main() {
	vec2 size = vec2(imageSize(bumpMap[0]).xy);
	float mult = 40;
	float noiseOffset = 2331.43;
	vec2 coords1 = abs((vec2(gl_GlobalInvocationID.xy) / size - 0.5) * 2.0); // -1 to +1
	vec2 coords2 = abs((vec2(gl_GlobalInvocationID.yx) / size - 0.5) * 2.0);
	float altitude1 = FastSimplexFractal(vec3(coords1 * mult, noiseOffset), 15);
	float altitude2 = FastSimplexFractal(vec3(coords2 * mult, noiseOffset), 15);
	float altitude = mix(altitude1, altitude2, min(1.0,coords1.x+coords1.y)/2.0);
	imageStore(bumpMap[0], ivec2(gl_GlobalInvocationID.xy), vec4(0,0,0, altitude));
}

#shader bump.normals.map.comp
void main() {
	ivec2 size = ivec2(imageSize(bumpMap[0]).xy);
	ivec2 center = ivec2(gl_GlobalInvocationID.xy);
	ivec2 top = ivec2(gl_GlobalInvocationID.xy) + ivec2(0, -1);
	ivec2 bottom = ivec2(gl_GlobalInvocationID.xy) + ivec2(0, +1);
	ivec2 right = ivec2(gl_GlobalInvocationID.xy) + ivec2(+1, 0);
	ivec2 left = ivec2(gl_GlobalInvocationID.xy) + ivec2(-1, 0);
	if (bottom.y >= size.y) bottom.y = 0;
	if (right.x >= size.x) right.x = 0;
	if (top.y < 0) top.y = size.y-1;
	if (left.x < 0) left.x = size.x-1;
	float altitude = imageLoad(bumpMap[0], ivec2(gl_GlobalInvocationID.xy)).a;
	float altitudeTop = imageLoad(bumpMap[0], top).a / 2.0 + 0.5;
	float altitudeBottom = imageLoad(bumpMap[0], bottom).a / 2.0 + 0.5;
	float altitudeRight = imageLoad(bumpMap[0], right).a / 2.0 + 0.5;
	float altitudeLeft = imageLoad(bumpMap[0], left).a / 2.0 + 0.5;
	vec3 normal = normalize(vec3(4*(altitudeRight-altitudeLeft), 4*(altitudeBottom-altitudeTop), 1));
	imageStore(bumpMap[0], ivec2(gl_GlobalInvocationID.xy), vec4(normal, altitude));
}

// #shader mantle.map.comp
// void main() {
// 	vec3 dir = GetCubeDirection(mantleMap[planetIndex]);
// 	imageStore(mantleMap[planetIndex], ivec3(gl_GlobalInvocationID), vec4(dir,0));
// }

// #shader tectonics.map.comp
// void main() {
// 	imageStore(tectonicsMap[planetIndex], ivec3(gl_GlobalInvocationID), vec4(0));
// }

// #shader height.map.comp
// void main() {
// 	vec3 dir = GetCubeDirection(heightMap[planetIndex]);
// 	float height = FastSimplexFractal(dir*20, 6);
// 	// height = 0;
// 	imageStore(heightMap[planetIndex], ivec3(gl_GlobalInvocationID), vec4(height));
// }

// #shader volcanoes.map.comp
// void main() {
// 	imageStore(volcanoesMap[planetIndex], ivec3(gl_GlobalInvocationID), vec4(0));
// }

// #shader liquids.map.comp
// void main() {
// 	imageStore(liquidsMap[planetIndex], ivec3(gl_GlobalInvocationID), vec4(0));
// }

// // #############################################################
// // #shader raymarching.rint

// // #include "rtx_base.glsl"
// // hitAttributeEXT ProceduralGeometry geom;

// // layout(set = 2, binding = 2) uniform samplerCube heightMap[MAX_PLANETS];

// // double dlength(dvec3 v) {
// // 	return sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
// // }

// // double dlengthSquared(dvec3 v) {
// // 	return (v.x*v.x + v.y*v.y + v.z*v.z);
// // }

// // void main() {
// // 	geom = GetProceduralGeometry(gl_InstanceCustomIndexEXT);
// // 	vec3 spherePosition = geom.objectInstance.position;
// // 	float sphereRadius = geom.aabbMax.x;
// // 	uint planetIndex = geom.material;
	
// // 	const vec3 direction = gl_WorldRayDirectionEXT;
// // 	const float tMin = gl_RayTminEXT;
// // 	const float tMax = gl_RayTmaxEXT;

// // 	const vec3 oc = -spherePosition;
// // 	const float a = dot(direction, direction);
// // 	const float b = dot(oc, direction);
// // 	const float c = dot(oc, oc) - sphereRadius*sphereRadius;
// // 	const float discriminant = b * b - a * c;

// // 	if (discriminant >= 0) {
// // 		const float discriminantSqrt = sqrt(discriminant);
// // 		const float t1 = (-b - discriminantSqrt) / a;
// // 		const float t2 = (-b + discriminantSqrt) / a;
// // 		if ((tMin <= t1 && t1 < tMax)    ||(tMin <= t2 && t2 < tMax)   ) {
// // 			vec3 start = gl_WorldRayDirectionEXT * t1;
// // 			// if (!(tMin <= t1 && t1 < tMax)) start = vec3(0);
// // 			double dist = 0;
// // 			if (!(tMin <= t1 && t1 < tMax)) dist = length(start);
// // 			double stepSize = max(0.01, t1 * 0.001);
// // 			for (int i = 0; i < 200; ++i) {
// // 				dist += stepSize;
// // 				dvec3 hitPoint = dvec3(start) + (dvec3(gl_WorldRayDirectionEXT) * dist);
// // 				dvec3 planetPos = (planets[planetIndex] * dvec4(hitPoint, 1)).xyz;
// // 				// vec3 planetPos = vec4(inverse(geom.objectInstance.modelViewMatrix) * vec4(hitPoint, 1)).xyz;
// // 				if (length(vec3(planetPos)) > sphereRadius) return;
// // 				double sampledTexture = double(texture(heightMap[planetIndex], vec3(planetPos)).r);
// // 				double heightMap = double(sphereRadius) - double(planetHeightVariation) + sampledTexture;
// // 				double alt = dlength(planetPos) - heightMap;
// // 				if (alt < 0.01) {
// // 					break;
// // 				} else {
// // 					stepSize = clamp(alt * 0.5, t1 * 0.01, planetHeightVariation/4);
// // 				}
// // 			}
// // 			reportIntersectionEXT(float(double(t1)+dist), 0);
// // 		}
// // 	}
// // }

// // #############################################################
// // #shader raymarching.rchit

// // #include "rtx_base.glsl"
// // hitAttributeEXT ProceduralGeometry geom;
// // layout(location = 0) rayPayloadInEXT RayPayload ray;
// // layout(location = 2) rayPayloadEXT bool shadowed;
// // #include "rtx_pbr.glsl"

// // #include "incubator_rendering/assets/shaders/_noise.glsl"
// // #include "incubator_rendering/assets/shaders/_v4dnoise.glsl"

// // layout(set = 2, binding = 0) uniform sampler2D bumpMap[1];
// // layout(set = 2, binding = 1) uniform samplerCube mantleMap[MAX_PLANETS];
// // layout(set = 2, binding = 2) uniform samplerCube tectonicsMap[MAX_PLANETS];
// // layout(set = 2, binding = 3) uniform samplerCube heightMap[MAX_PLANETS];
// // layout(set = 2, binding = 4) uniform samplerCube volcanoesMap[MAX_PLANETS];
// // layout(set = 2, binding = 5) uniform samplerCube liquidsMap[MAX_PLANETS];

// // uint planetIndex = geom.material;

// // void main() {
// // 	// vec3 spherePosition = geom.objectInstance.position;
// // 	// float sphereRadius = geom.aabbMax.x;
	
// // 	// Hit World Position
// // 	const vec3 hitPoint = gl_WorldRayDirectionEXT * gl_HitTEXT;
// // 	const dvec3 planetPos = (planets[planetIndex] * dvec4(hitPoint, 1)).xyz;
// // 	// const vec3 planetPos = vec4(inverse(geom.objectInstance.modelViewMatrix) * vec4(hitPoint, 1)).xyz;
	
	
// // 	vec3 pos0 = vec3(planetPos);
// // 	vec3 tangentX = normalize(cross(vec3(0,0,1), normalize(pos0)));
// // 	vec3 tangentY = normalize(cross(normalize(pos0), tangentX));
// // 	vec3 posR = normalize(pos0+tangentX*10);
// // 	vec3 posL = normalize(pos0-tangentX*10);
// // 	vec3 posU = normalize(pos0+tangentY*10);
// // 	vec3 posD = normalize(pos0-tangentY*10);
	
// // 	float height = texture(heightMap[planetIndex], pos0).r;
// // 	posR *= texture(heightMap[planetIndex], posR).r;
// // 	posL *= texture(heightMap[planetIndex], posL).r;
// // 	posU *= texture(heightMap[planetIndex], posU).r;
// // 	posD *= texture(heightMap[planetIndex], posD).r;
	
// // 	vec3 line1 = posR - posL;
// // 	vec3 line2 = posU - posD;
	
// // 	vec3 normal = geom.objectInstance.normalMatrix * normalize(mix(normalize(pos0), normalize(cross(line1, line2)),0.2));
	
// // 	vec3 c = vec3(height / planetHeightVariation / 2.0 + 0.5);
	
// // 	vec3 color = ApplyPBRShading(hitPoint, c, normal, /*roughness*/0.5, /*metallic*/0.0);
	
	
// // 	// vec4 color = vec4(texture(heightMap[planetIndex], vec3(planetPos)).r / planetHeightVariation / 2.0 + 0.5);
// // 	// vec4 color = texture(mantleMap[planetIndex], normalize(planetPos));
	
// // 	// if (gl_HitTEXT < 10000) color = mix(color, vec4(float(FastSimplexFractal(planetPos/50.0, 2)/2.0+0.5)), smoothstep(10000, 0, gl_HitTEXT));
	
// // 	// if (gl_HitTEXT < 1.0) color = vec4(1,0,0,1);
	
// // 	ray.color = color.rgb;
// // 	ray.distance = gl_HitTEXT;
// // }
					

// #############################################################
// #shader terrain.vertexpos.comp

// vec3 GetVertexPos(uint index) {
// 	return vertices[index*2].xyz;
// }

// vec2 GetVertexUV(uint index) {
// 	return UnpackUVfromFloat(vertices[index*2+1].w);
// }

// void SetVertexPos(uint index, vec3 pos) {
// 	vertices[index*2].xyz = pos;
// }

// void main() {
// 	uint genRow = gl_GlobalInvocationID.x;
// 	uint genCol = gl_GlobalInvocationID.y;
// 	uint currentIndex = (vertexSubdivisionsPerChunk+1) * genRow + genCol + GetVertexOffset(chunkGeometryIndex);
	
// 	dvec3 vertexPos = dvec3(GetVertexPos(currentIndex)) + dvec3(chunkPosition);
// 	dvec3 normalizedPos = normalize(vertexPos);
	
// 	double mainHeightMap = double(texture(heightMap[planetIndex], vec3(vertexPos)).r*12000.0);
// 	vertexPos += normalizedPos * (mainHeightMap);
	
// 	// // double secondaryHeightMap = FastSimplexFractal(normalizedPos*solidRadius/1000000.0, 5)*20000.0;
// 	// // secondaryHeightMap += FastSimplexFractal(normalizedPos*solidRadius/10000.0, 5)*1000.0;
// 	// double secondaryHeightMap = FastSimplexFractal((normalizedPos*solidRadius/100.0), 5)*20.0;
// 	// secondaryHeightMap += FastSimplexFractal(normalizedPos*solidRadius/5.0, 2);
// 	// vertexPos += normalizedPos * (secondaryHeightMap);
	
// 	SetVertexPos(currentIndex, vec3(vertexPos - dvec3(chunkPosition)));
	
// 	// // Skirts
// 	// if (genCol == 0) {
// 	// 	// Left Skirt
		
// 	// } else if (genCol == vertexSubdivisionsPerChunk+1) {
// 	// 	// Right Skirt
		
// 	// } else if (genRow == 0) {
// 	// 	// Top Skirt
		
// 	// } else if (genRow == vertexSubdivisionsPerChunk+1) {
// 	// 	// Bottom Skirt
		
// 	// }
// }


// #############################################################
// #shader terrain.vertexnormal.comp

// vec3 GetVertexPos(uint index) {
// 	return vertices[index*2].xyz;
// }

// void SetVertexNormal(uint index, vec3 normal) {
// 	vertices[index*2+1].xyz = normal;
// }

// void GetFaceVectors(int face, out vec3 dir, out vec3 top, out vec3 right) {
// 	switch (face) {
// 		case 0:
// 			dir = vec3(0, 0, 1);
// 			top = vec3(0, 1, 0);
// 			right = vec3(1, 0, 0);
// 			break;
// 		case 1:
// 			dir = vec3(0, 0, -1);
// 			top = vec3(0, 1, 0);
// 			right = vec3(-1, 0, 0);
// 			break;
// 		case 2:
// 			dir = vec3(1, 0, 0);
// 			top = vec3(0, 1, 0);
// 			right = vec3(0, 0, -1);
// 			break;
// 		case 3:
// 			dir = vec3(-1, 0, 0);
// 			top = vec3(0, -1, 0);
// 			right = vec3(0, 0, -1);
// 			break;
// 		case 4:
// 			dir = vec3(0, 1, 0);
// 			top = vec3(0, 0, 1);
// 			right = vec3(-1, 0, 0);
// 			break;
// 		case 5:
// 			dir = vec3(0, -1, 0);
// 			top = vec3(0, 0, -1);
// 			right = vec3(-1, 0, 0);
// 			break;
// 	}
// }

// void main() {
// 	uint vertexOffset = GetVertexOffset(chunkGeometryIndex);
	
// 	uint genRow = gl_GlobalInvocationID.x;
// 	uint genCol = gl_GlobalInvocationID.y;
// 	uint currentIndex = (vertexSubdivisionsPerChunk+1) * genRow + genCol + vertexOffset;
// 	vec3 vertexPos = GetVertexPos(currentIndex);
	
// 	vec3 tangentX;
// 	vec3 tangentY;
	
// 	if (genRow < vertexSubdivisionsPerChunk && genCol < vertexSubdivisionsPerChunk) {
// 		// For full face (generate top left)
// 		uint topLeftIndex = currentIndex;
// 		uint topRightIndex = topLeftIndex+1;
// 		uint bottomLeftIndex = (vertexSubdivisionsPerChunk+1) * (genRow+1) + genCol + vertexOffset;
		
// 		tangentX = normalize(GetVertexPos(topRightIndex) - vertexPos);
// 		tangentY = normalize(vertexPos - GetVertexPos(bottomLeftIndex));
		
// 	} else if (genCol == vertexSubdivisionsPerChunk && genRow == vertexSubdivisionsPerChunk) {
// 		// For right-most bottom-most vertex (generate bottom-most right-most)
		
// 		// vec3 bottomLeftPos {0};
// 		// {
// 		// 	dvec3 topOffset = mix(topLeft - center, bottomLeft - center, double(genRow+1)/vertexSubdivisionsPerChunk);
// 		// 	dvec3 rightOffset = mix(topLeft - center, topRight - center, double(genCol)/vertexSubdivisionsPerChunk);
// 		// 	dvec3 pos = Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
// 		// 	bottomLeftPos = {pos * planet->GetHeightMap(pos, triangleSize) - centerPos};
// 		// }

// 		// vec3 topRightPos {0};
// 		// {
// 		// 	dvec3 topOffset = mix(topLeft - center, bottomLeft - center, double(genRow)/vertexSubdivisionsPerChunk);
// 		// 	dvec3 rightOffset = mix(topLeft - center, topRight - center, double(genCol+1)/vertexSubdivisionsPerChunk);
// 		// 	dvec3 pos = Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
// 		// 	topRightPos = {pos * planet->GetHeightMap(pos, triangleSize) - centerPos};
// 		// }

// 		// tangentX = normalize(topRightPos - vertexPos);
// 		// tangentY = normalize(vertexPos - bottomLeftPos);
		
// 		tangentX = normalize(vertexPos - GetVertexPos(currentIndex-1));
// 		tangentY = normalize(GetVertexPos(currentIndex-vertexSubdivisionsPerChunk-1) - vertexPos);
		
// 	} else if (genCol == vertexSubdivisionsPerChunk) {
// 		// For others in right col (generate top right)
// 		// uint bottomRightIndex = currentIndex+vertexSubdivisionsPerChunk+1;
		
// 		// vec3 topRightPos {0};
// 		// {
// 		// 	dvec3 topOffset = mix(topLeft - center, bottomLeft - center, double(genRow)/vertexSubdivisionsPerChunk);
// 		// 	dvec3 rightOffset = mix(topLeft - center, topRight - center, double(genCol+1)/vertexSubdivisionsPerChunk);
// 		// 	dvec3 pos = Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
// 		// 	topRightPos = {pos * planet->GetHeightMap(pos, triangleSize) - centerPos};
// 		// }

// 		// tangentX = normalize(topRightPos - vertexPos);
// 		// tangentY = normalize(vertexPos - GetVertexPos(bottomRightIndex));
		
// 		tangentX = normalize(vertexPos - GetVertexPos(currentIndex-1));
// 		tangentY = normalize(vertexPos - GetVertexPos(currentIndex+vertexSubdivisionsPerChunk+1));
		
// 	} else if (genRow == vertexSubdivisionsPerChunk) {
// 		// For others in bottom row (generate bottom left)
		
// 		// vec3 bottomLeftPos {0};
// 		// {
// 		// 	dvec3 topOffset = mix(topLeft - center, bottomLeft - center, double(genRow+1)/vertexSubdivisionsPerChunk);
// 		// 	dvec3 rightOffset = mix(topLeft - center, topRight - center, double(genCol)/vertexSubdivisionsPerChunk);
// 		// 	dvec3 pos = Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
// 		// 	bottomLeftPos = {pos * planet->GetHeightMap(pos, triangleSize) - centerPos};
// 		// }

// 		// tangentX = normalize(GetVertexPos(currentIndex+1) - vertexPos);
// 		// tangentY = normalize((vertexPos - bottomLeftPos));
		
// 		tangentX = normalize(GetVertexPos(currentIndex+1) - vertexPos);
// 		tangentY = normalize(GetVertexPos(currentIndex-vertexSubdivisionsPerChunk-1) - vertexPos);
		
// 	}

// 	vec3 faceDir, topDir, rightDir;
// 	GetFaceVectors(face, faceDir, topDir, rightDir);
// 	float topSign = topDir.x + topDir.y + topDir.z;
// 	float rightSign = rightDir.x + rightDir.y + rightDir.z;
	
// 	tangentX *= rightSign;
// 	tangentY *= topSign;
	
// 	vec3 normal = normalize(cross(tangentX, tangentY));
	
// 	SetVertexNormal(currentIndex, normal);
	
// 	// slope = (float) max(0.0, dot(dvec3(normal), normalize(centerPos + dvec3(vertexPos))));
// }

#############################################################
#shader terrain.rchit

#include "rtx_base.glsl"
hitAttributeEXT vec3 hitAttribs;
layout(location = 0) rayPayloadInEXT RayPayload ray;
layout(location = 2) rayPayloadEXT bool shadowed;
#include "rtx_pbr.glsl"
#include "rtx_fragment.glsl"

// #include "incubator_rendering/assets/shaders/_noise.glsl"
// #include "incubator_rendering/assets/shaders/_v4dnoise.glsl"

vec4 GetBumpMap(vec2 uv, vec2 uvChunk) {
	return vec4(0,0,1,0)
	//  + texture(bumpMap[0], uv)
	//  + texture(bumpMap[0], uv*256)
	 + texture(bumpMap[0], uvChunk)/2.0
	;
}

void main() {
	Fragment fragment = GetHitFragment(true);


	vec3 tangentX = normalize(cross(fragment.geometryInstance.normalViewTransform * vec3(0,1,0)/* fixed arbitrary vector in object space */, fragment.viewSpaceNormal));
	vec3 tangentY = normalize(cross(fragment.viewSpaceNormal, tangentX));
	mat3 TBN = mat3(tangentX, tangentY, fragment.viewSpaceNormal); // viewSpace TBN
	vec2 uvOffset = fragment.geometryInstance.custom3f.xy;
	vec2 uvMult = vec2(fragment.geometryInstance.custom3f.z);
	vec2 uv = (fragment.uv*uvMult+uvOffset);
	vec4 bump = GetBumpMap(uv, fragment.uv);
	vec3 normal = normalize(TBN * bump.xyz);
	vec3 color = ApplyPBRShading(fragment.hitPoint, fragment.color.rgb, normal, /*bump*/fragment.viewSpaceNormal*(bump.w+1.0)*0.001, /*roughness*/0.6, /*metallic*/0.1);
	vec3 ambient = fragment.color.rgb * 0.001;
	
	ray.color = color + ambient;
	ray.distance = gl_HitTEXT;
	
	
	// vec3 color = ApplyPBRShading(fragment.hitPoint, fragment.color.rgb, fragment.viewSpaceNormal, vec3(0), /*roughness*/0.6, /*metallic*/0.1);
	// ray.color = color;
	// ray.distance = gl_HitTEXT;
	
}
