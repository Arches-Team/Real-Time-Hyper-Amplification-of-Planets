#version 450 core

layout (location = 0) in dvec4 vertexPos;
layout (location = 1) in vec4 vertexNormal;
layout (location = 2) in vec4 vertexFlow;

layout (location = 0) out vec3 worldpos;
layout (location = 1) out vec3 normal;
layout (location = 2) out float altitude;
layout (location = 3) out float distance2Cam;

uniform dmat4 projview;
uniform dvec3 cameraPosition;
uniform double renderScale;
uniform double farPlane;
const float Fcoef = 2.0 / log2(1.0*float(farPlane) + 1.0);
uniform double planetRadiusKm;
uniform double seaLevelKm; 


void main()
{  
	dvec3 pkm = normalize(vertexPos.xyz) * (planetRadiusKm + vertexPos.w);
	dvec4 p = dvec4(pkm / renderScale, 1.0LF);
	
	distance2Cam = float(distance(p.xyz, cameraPosition));
	worldpos = vec3(p.xyz);
	altitude = float((vertexPos.w - seaLevelKm) / renderScale);
	normal = vertexNormal.xyz;
	
	// --- Log depth ---
	dvec4 proj = projview * p;	
	proj.z = double(log2(max(1e-6, 1.0 + 1.0*float(proj.w))) * Fcoef - 1.0);  // log z - (see : http://outerra.blogspot.fr/2009/08/logarithmic-z-buffer.html)
	proj.z *= proj.w;
	
	// --- out ---
	gl_Position = vec4(proj);		
}