#version 460 core
#extension GL_ARB_shader_viewport_layer_array : enable

precision highp int;
precision highp float;
precision highp sampler2D;

#common gen.*

layout(std430, push_constant) uniform GalaxyGenPushConstant {
	dvec3 cameraPosition;
	int frameIndex;
	int resolution;
} galaxyGen;

#common box.*
#include "_v4d_baseDescriptorSet.glsl"

##################################################################
#shader gen.vert

layout(location = 0) in vec4 posr;
layout(location = 1) in uint seed;
layout(location = 2) in uint numStars;

layout(location = 0) out uint out_seed;
layout(location = 1) out uint out_numStars;

void main() {
	gl_Position = posr; // passthrough
	out_seed = seed + galaxyGen.frameIndex;
	out_numStars = numStars;
}

##################################################################
#shader gen.geom

#include "_noise.glsl"
#include "_v4dnoise.glsl"



float easeOut(float t) {
	return sin(t * 3.14159265459 * 0.5);
}
float easeIn(float t) {
	return 1.0 - cos(t * 3.14159265459 * 0.5);
}
float clamp01(float v) {
	return max(0.0, min(1.0, v));
}

const int octaves = 5;
float FastSimplexFractal(vec3 pos) {
	float amplitude = 0.5333333333;
	float frequency = 1.0;
	float f = FastSimplex(pos * frequency);
	if (octaves > 1) for (int i = 1; i < octaves; ++i) {
		amplitude /= 2.0;
		frequency *= 2.0;
		f += amplitude * FastSimplex(pos * frequency);
	}
	return f;
}


struct GalaxyInfo {
	float spiralCloudsFactor;
	float swirlTwist;
	float swirlDetail;
	float coreSize;
	float cloudsSize;
	float cloudsFrequency;
	float squish;
	float attenuationCloudsFrequency;
	float attenuationCloudsFactor;
	vec3 position;
	vec3 noiseOffset;
	mat4 rotation;
	float irregularities;
};

GalaxyInfo GetGalaxyInfo(vec3 galaxyPosition) {
	GalaxyInfo info;
	float type = QuickNoise(galaxyPosition / 10.0);
	if (type < 0.2) {
		// Elliptical galaxy (20% probability)
		info.spiralCloudsFactor = 0.0;
		info.coreSize = QuickNoise(galaxyPosition);
		info.squish = QuickNoise(galaxyPosition+vec3(-0.33,-0.17,-0.51)) / 2.0;
	} else {
		if (type > 0.3) {
			// Irregular galaxy (70% probability, within spiral)
			info.irregularities = QuickNoise(galaxyPosition+vec3(-0.65,0.69,-0.71));
		} else {
			info.irregularities = 0.0;
		}
		// Spiral galaxy (80% probability, including irregular, only 10% will be regular)
		vec3 n1 = Noise3(galaxyPosition+vec3(0.01,0.43,-0.55)) / 2.0 + 0.5;
		vec3 n2 = Noise3(galaxyPosition+vec3(-0.130,0.590,-0.550)) / 2.0 + 0.5;
		vec3 n3 = Noise3(galaxyPosition+vec3(0.510,-0.310,0.512)) / 2.0 + 0.5;
		info.spiralCloudsFactor = n1.x;
		info.swirlTwist = n1.y;
		info.swirlDetail = n1.z;
		info.coreSize = n2.x;
		info.cloudsSize = n2.y;
		info.cloudsFrequency = n2.z;
		info.squish = n3.x;
		info.attenuationCloudsFrequency = n3.y;
		info.attenuationCloudsFactor = n3.z;
		info.noiseOffset = Noise3(galaxyPosition);
	}
	if (info.spiralCloudsFactor > 0.0 || info.squish > 0.2) {
		vec3 axis = normalize(Noise3(galaxyPosition+vec3(-0.212,0.864,0.892)));
		float angle = QuickNoise(galaxyPosition+vec3(0.176,0.917,1.337));
		float s = sin(angle);
		float c = cos(angle);
		float oc = 1.0 - c;
		info.rotation = mat4(
			oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s, 0.0,
			oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s, 0.0,
			oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c, 0.0,
			0.0, 0.0, 0.0, 1.0
		);
	}
	return info;
}

float GalaxyStarDensity(in vec3 pos, in GalaxyInfo info) {
	float len = length(pos);
	if (len > 1.0) return 0.0;

	// Rotation
	if (info.spiralCloudsFactor > 0.0 || info.squish > 0.2) pos = (info.rotation * vec4(pos, 1.0)).xyz;

	float squish = info.squish * 2.0;
	float lenSquished = length(pos*vec3(1.0, squish + 1.0, 1.0));
	float radiusGradient = 1.0 - pow(clamp01(len + abs(pos.y)*squish), 5.0);

	float core = clamp01(pow(1.0-lenSquished/info.coreSize, 4.0) + pow(1.0-lenSquished/info.coreSize, 10.0));
	if (core + radiusGradient <= 0.0) return 0.0;
	float finalDensity = core + pow(max(0.0, radiusGradient - 0.2), 8.0);

	if (info.spiralCloudsFactor == 0.0) {
		return finalDensity;
	}

	vec3 noiseOffset = info.noiseOffset * 6544.415;

	// Irregular
	if (info.irregularities > 0.0) {
		vec3 irregular = info.noiseOffset/2.0+.5;
		pos = mix(pos, pos*irregular, info.irregularities);
		info.spiralCloudsFactor = mix(info.spiralCloudsFactor, sin(irregular.y), info.irregularities);
		info.swirlTwist = mix(info.swirlTwist, irregular.x, info.irregularities);
		info.cloudsSize = mix(info.cloudsSize, irregular.y, info.irregularities);
		info.attenuationCloudsFrequency = mix(info.attenuationCloudsFrequency, irregular.z, info.irregularities);
		info.attenuationCloudsFactor = mix(info.attenuationCloudsFactor, irregular.x, info.irregularities);
		core += clamp01(pow(1.0-length(pos+info.noiseOffset)/info.coreSize*irregular.x, (sin(irregular.x)+1.0)*3.0));
		finalDensity += core * pow(max(0.0, radiusGradient), 1.5*irregular.x+1.0);
	}

	// Spiral
	float swirl = len * info.swirlTwist * 10.0;
	float spiralNoise = FastSimplexFractal((vec3(
		pos.x * cos(swirl) - pos.z * sin(swirl),
		pos.y * (squish * 10.0 + 1.0),
		pos.z * cos(swirl) + pos.x * sin(swirl)
	)+noiseOffset)*info.cloudsFrequency*5.0)/2.0+0.5;
	float spirale = clamp01(pow(spiralNoise, (1.1-info.swirlDetail)*10.0) + (info.cloudsSize*1.5) - len*1.5 - (abs(pos.y)*squish*10.0)) * radiusGradient;
	finalDensity += 1.0-pow(1.0-spirale, info.spiralCloudsFactor*2.0);
	if (finalDensity <= 0.0) return 0.0;

	// Attenuation Clouds
	float attenClouds = pow(clamp01(1.0-abs(FastSimplexFractal((vec3(
		pos.x * cos(swirl / 2.5) - pos.z * sin(swirl / 2.5),
		pos.y * (squish * 2.0 + 1.0),
		pos.z * cos(swirl / 2.5) + pos.x * sin(swirl / 2.5)
	)+noiseOffset)*info.attenuationCloudsFrequency*20.0))-core*3.0) * easeIn(radiusGradient), (3.0-info.attenuationCloudsFactor*2.0)) * info.attenuationCloudsFactor;
	if (info.attenuationCloudsFactor > 0.0) finalDensity -= attenClouds * clamp01((FastSimplex((pos+info.noiseOffset)*info.attenuationCloudsFrequency*9.0)/2.0+0.5) * radiusGradient - (abs(pos.y)*squish*3.0));

	return finalDensity;
}

vec3 GalaxyStarColor(in vec3 pos, in GalaxyInfo info) {
	vec4 starType = normalize(vec4(
		/*red*/		QuickNoise(pos+info.noiseOffset+vec3(1.337,0.612,1.065)) * 0.5+pow(1.0-length(pos), 5.0)*3.0,
		/*yellow*/	QuickNoise(pos+info.noiseOffset+vec3(0.176,1.337,0.099)) * 1.4,
		/*blue*/	QuickNoise(pos+info.noiseOffset+vec3(1.337,0.420,1.099)) * 0.7+pow(length(pos), 3.0)*3.0,
		/*white*/	QuickNoise(pos+info.noiseOffset+vec3(1.337,1.185,0.474)) * 1.0 
	));
	return		/*red*/		vec3( 1.0 , 0.4 , 0.2 ) * starType.x +
				/*yellow*/	vec3( 1.0 , 1.0 , 0.3 ) * starType.y +
				/*blue*/	vec3( 0.2 , 0.4 , 1.0 ) * starType.z +
				/*white*/	vec3( 1.0 , 1.0 , 1.0 ) * starType.w ;
}




/* 
max_vertices <= min(limits.maxGeometryOutputVertices, floor(limits.maxGeometryTotalOutputComponents / min(limits.maxGeometryOutputComponents, 7 + NUM_OUT_COMPONENTS)))
Typical values (GTX 1050ti, GTX 1080, RTX 2080): 
	limits.maxGeometryOutputComponents 		= 128
	limits.maxGeometryOutputVertices 		= 1024
	limits.maxGeometryTotalOutputComponents = 1024
=======
In practice: 
	max_vertices <= floor(1024 / 7 + NUM_OUT_COMPONENTS)
*/

layout(points) in;
layout(points, max_vertices = 80) out; // takes up 7 components per vertex (1 for gl_PointSize, 4 for gl_Position, 2 for gl_PointCoord)
layout(location = 0) out vec4 out_color; // takes up 4 components

layout(location = 0) in uint in_seed[];
layout(location = 1) in uint in_numStars[];

const float MIN_VIEW_DISTANCE = 0.0;
const float MAX_VIEW_DISTANCE = 2.0;

float linearstep(float a, float b, float x) {
	return (x - a) / (b - a);
}

void main(void) {
	uint seed = in_seed[0];
	uint fseed = seed + 15;
	vec3 relPos = vec3(dvec3(gl_in[0].gl_Position.xyz) - galaxyGen.cameraPosition.xyz); // position relative to camera
	float radius = gl_in[0].gl_Position.w;
	
	float dist = length(relPos);
	float sizeInScreen = radius / dist * float(galaxyGen.resolution);
	int nbStarsToDraw = int(max(1, min(sizeInScreen*sizeInScreen, in_numStars[0])));
	float brightnessBasedOnDistance = pow(linearstep(MAX_VIEW_DISTANCE, MIN_VIEW_DISTANCE, dist), 0.5);
	
	if (brightnessBasedOnDistance < 0.001) return;
	
	GalaxyInfo info = GetGalaxyInfo(gl_in[0].gl_Position.xyz);
	
	for (int i = 0; i < nbStarsToDraw; i++) {
		if (i > 80) break;
		gl_PointSize = max(3, brightnessBasedOnDistance * 4);
		
		vec3 starPos = RandomInUnitSphere(seed);
		
		float starDensity = GalaxyStarDensity(starPos, info);
		
		if (starDensity == 0.0) continue;
		
		gl_PointSize *= starDensity/2.0+1.0;
		
		vec3 pos = relPos + starPos*radius;
		
		vec3 color = GalaxyStarColor(starPos, info);
		
		out_color = vec4(
			color,
			/*(1-pow(1-RandomFloat(fseed), 2)) * */ brightnessBasedOnDistance * starDensity
		);
		
		// Compute which side of the cube map we should render to
		if (pos.x > 0 && abs(pos.z) <= pos.x && abs(pos.y) <= pos.x) {
			// Right
			gl_Layer = 0;
			gl_Position = vec4(
				-pos.z / pos.x,
				-pos.y / pos.x,
				0, 1
			);
		} else 
		if (-pos.x > 0 && abs(pos.z) <= -pos.x && abs(pos.y) <= -pos.x) {
			// Left
			gl_Layer = 1;
			gl_Position = vec4(
				-pos.z / pos.x,
				 pos.y / pos.x,
				0, 1
			);
		} else
		if (pos.y > 0 && abs(pos.z) <= pos.y && abs(pos.x) <= pos.y) {
			// Front
			gl_Layer = 2;
			gl_Position = vec4(
				pos.x / pos.y,
				pos.z / pos.y,
				0, 1
			);
		} else 
		if (-pos.y > 0 && abs(pos.z) <= -pos.y && abs(pos.x) <= -pos.y) {
			// Back
			gl_Layer = 3;
			gl_Position = vec4(
				-pos.x / pos.y,
				 pos.z / pos.y,
				0, 1
			);
		} else 
		if (pos.z > 0 && abs(pos.x) <= pos.z && abs(pos.y) <= pos.z) {
			// Top
			gl_Layer = 4;
			gl_Position = vec4(
				 pos.x / pos.z,
				-pos.y / pos.z,
				0, 1
			);
		} else 
		if (-pos.z > 0 && abs(pos.x) <= -pos.z && abs(pos.y) <= -pos.z) {
			// Bottom
			gl_Layer = 5;
			gl_Position = vec4(
				pos.x / pos.z,
				pos.y / pos.z,
				0, 1
			);
		}
		
		// Magical formula to adjust point size for sperical cubemap... Took 3 days of intensive math to figure it out...
		gl_PointSize *= (tan(length(gl_Position.xy)/sqrt(2))+0.5)/2;
		
		EmitVertex();
	}
}

##################################################################
#shader gen.frag

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void main() {
	float center = 1 - pow(length(gl_PointCoord * 2 - 1), 1.0 / max(0.7, in_color.a));
	out_color = vec4(in_color.rgb, in_color.a) * center;
}

##################################################################
#shader fade.vert

layout(location = 0) out int index;

void main() {
	index = gl_VertexIndex;
}

##################################################################
#shader fade.geom

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in int index[];

void main(void) {
	gl_Position = vec4(-1, 1, 0, 1);
	gl_Layer = index[0];
	EmitVertex();
	gl_Position = vec4(-1, -1, 0, 1);
	gl_Layer = index[0];
	EmitVertex();
	gl_Position = vec4(1, 1, 0, 1);
	gl_Layer = index[0];
	EmitVertex();
	gl_Position = vec4(1, -1, 0, 1);
	gl_Layer = index[0];
	EmitVertex();
	
	EndPrimitive();
}

##################################################################
#shader fade.frag

layout(location = 0) out vec4 out_color;

void main() {
	// out_color = vec4(0.004,0.004,0.004, 1);
}

##################################################################
#shader box.vert

layout(push_constant) uniform GalaxyBoxPushConstant {
	dmat4 inverseProjectionView;
} galaxyBox;

layout(location = 0) out vec3 out_dir;

void main() {
	// Full-screen Quad from 4 empty vertices
	dvec2 pos = vec2((gl_VertexIndex & 2)>>1, 1-(gl_VertexIndex & 1)) * 2.0 - 1.0;
	gl_Position = vec4(vec2(pos), 0, 1);
	// output direction of vertex into world
	out_dir = vec3(normalize(galaxyBox.inverseProjectionView * dvec4(pos, 1, 1)).xyz);
}

##################################################################
#shader box.frag

layout(location = 0) in vec3 in_dir;
layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform samplerCube galaxyBox;

void main() {
	out_color = texture(galaxyBox, in_dir);
}
