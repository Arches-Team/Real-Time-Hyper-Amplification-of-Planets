#version 450 core

layout (location = 0) in vec3 inVertex;
layout (location = 1) in vec2 inTexcoords;

layout (location = 0) out vec2 texcoords;

void main()
{
    gl_Position = vec4(inVertex.xyz, 1.0);
    texcoords = inTexcoords;
}