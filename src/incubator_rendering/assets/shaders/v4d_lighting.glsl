#version 460 core

precision highp int;
precision highp float;
precision highp sampler2D;

#common .*frag

#include "Camera.glsl"
#include "LightSource.glsl"
#include "gBuffers_in.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

##################################################################
#shader vert

layout (location = 0) out vec2 outUV;

void main() 
{
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0f + -1.0f, 0.0f, 1.0f);
}

##################################################################

#shader opaque.frag
void main() {
	GBuffers gBuffers = LoadGBuffers();
	vec3 color = gBuffers.albedo.rgb;
	float alpha = gBuffers.albedo.a;
	
	
	
	
	
	
	
	
	
	/////////////////////////////
	// Blinn-Phong

	// ambient
	vec3 ambient = vec3(0);// lightSource.color * lightSource.ambientStrength;

	// diffuse
	vec3 norm = normalize(gBuffers.normal);
	vec3 lightDir = normalize(lightSource.viewPosition - gBuffers.position);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * lightSource.color * (lightSource.intensity/20.0);

	// specular
	float specularStrength = 0.5;
	vec3 viewDir = normalize(-gBuffers.position);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	vec3 specular = specularStrength * spec * lightSource.color * (lightSource.intensity/30.0);

// diffuse = vec3(0.5);
// specular = vec3(0);

	// // Attenuation (light.constant >= 1.0, light.linear ~= 0.1, light.quadratic ~= 0.05)
	// float dist = distance(lightSource.viewPosition, gBuffers.position);
	// float attenuation = 1.0 / (1.0/*light.constant*/ + 0.1/*light.linear*/*dist + 0.05/*light.quadratic*/*(dist*dist));
	// ambient *= attenuation;
	// diffuse *= attenuation;
	// specular *= attenuation;
	
	
	// //////
	// // For Spot lights : 
	// float innerCutOff = cos(radians(12/*degrees*/));
	// float outerCutOff = cos(radians(18/*degrees*/));
	// float theta = dot(lightDir, normalize(-spotLight.direction));
	// float epsilon = (innerCutOff - outerCutOff);
	// float intensity = clamp((theta - outerCutOff) / epsilon, 0.0, 1.0);
	// diffuse *= intensity;
	// specular *= intensity;
	
	
	
	// Final color
	color *= (ambient + diffuse + specular);

	/////////////////////////////
	
	
	
	
	
	
	
	
	// color = gBuffers.albedo.rgb * vec3(dot(gBuffers.normal, normalize(lightSource.viewPosition - gBuffers.position)));
	
	
	
	
	
	
	
	
	out_color = vec4(color, alpha);
}

#shader transparent.frag
void main() {
	GBuffers gBuffers = LoadGBuffers();
	vec3 color = gBuffers.albedo.rgb;
	float alpha = gBuffers.albedo.a;
	
	if (alpha < 0.01) discard;
	
	out_color = vec4(color, alpha);
}
