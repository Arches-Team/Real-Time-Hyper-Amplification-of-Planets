#version 450 core

layout (location = 0) in vec4 vertexPos;

layout (location = 0) out vec3 proxyPos;

uniform dmat4 model;
uniform dmat4 view;
uniform dmat4 projection;



void main()
{  
  dvec4 proxy = model * dvec4(vertexPos.xyz, 1.0LF);
  dvec4 p = projection * view * proxy;
  
  proxyPos = vec3(proxy.xyz);  
  gl_Position = vec4(p);
}