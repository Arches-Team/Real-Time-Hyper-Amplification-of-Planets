#version 450 core

layout (location = 0) in vec2 texcoords;
layout (location = 0) out vec4 Color;

uniform sampler2D blitTexture;


void main()
{
	ivec2 coords = ivec2(gl_FragCoord.xy - vec2(0.5));
	vec3 c = texelFetch(blitTexture, coords, 0).rgb;
	
	Color = vec4(clamp(c, 0.0, 1.0), 1.0);
}