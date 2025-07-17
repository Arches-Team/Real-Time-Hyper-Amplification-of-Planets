#version 450 core

layout (location = 0) in vec3 color;
layout (location = 1) in vec3 normal;

layout (location = 0) out vec4 outcolor;

uniform vec3 sunDirection;// = normalize(vec3(4.0, 0.0, 0.0));


void main()
{
	float cosDiff = max(0.0, dot(normal, sunDirection));
	vec3 col = color * (0.01 + cosDiff);

	const float gamma = 1.0/2.2;
	col = clamp(col, 0.0, 1.0);
	col = pow(col, vec3(gamma));
	
	outcolor = vec4(col, 1.0);		
}