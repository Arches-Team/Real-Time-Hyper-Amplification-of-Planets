#version 450 core

layout (location = 0) in vec3 worldpos;
layout (location = 1) in vec3 normal;
layout (location = 2) in float altitude;
layout (location = 3) in float distance2Cam;

layout (location = 0) out vec4 gbuffer0;
layout (location = 1) out vec4 gbuffer1;

void main()
{  
	gbuffer0 = vec4(worldpos, altitude);
	gbuffer1 = vec4(normal, distance2Cam);
}