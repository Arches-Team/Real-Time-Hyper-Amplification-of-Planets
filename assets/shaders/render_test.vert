#version 450 core

layout (location = 0) in dvec4 vertexPos;
layout (location = 1) in dvec4 vertexData;
layout (location = 2) in vec4 vertexFlow;
layout (location = 3) in vec4 vertexMisc1;
layout (location = 4) in vec4 vertexMisc2;

layout (location = 0) out vec3 color;
layout (location = 1) out vec3 normal;

// --- structures de données ---
#define TYPE_NONE							0	
#define TYPE_SEA							1		
#define TYPE_CONTINENT						2
#define TYPE_RIVER							3
#define TYPE_COAST							4
#define TYPE_RIDGE							5
#define TYPE_DRAINAGE						6

struct Vertex
{
	/// first list of indexes of incident faces (or -1 if none)
	int faces_0[4];
	/// second list of indexes of incident faces (or -1 if none)
	int faces_1[4];
	/// PRNG seed
	uint seed;
	/// boolean = ghost or not ghost vertex
	uint status;
	//
	uint type;
	///
	uint branch_count; 	
	
	/// indexes of the vertices acting as river primitives
	int prim0;
	int prim1;
	int prim2;
	
	int padding1;
	//int pad[4];
};
struct Face
{
	/// indexes of the three edges.
	int e0, e1, e2;
	/// normal to the triangle
	float normal_x, normal_y, normal_z;
	/// LSB to MSB : byte 0 = boolean (split 1 or not 0), byte 2 = lod
	uint status;
	//
	uint type;
};


layout(binding = 0, std430) readonly buffer vertices_buffer 
{
  Vertex vertices[];
};
layout(binding = 1, std430) readonly buffer faces_buffer 
{
  Face faces[];
};



uniform dmat4 view;
uniform dmat4 projection;
uniform double renderScale;
uniform double farPlane;
const float Fcoef = 2.0 / log2(1.0*float(farPlane) + 1.0);

uniform uint showDrainage;
uniform uint showTectonicAge;
uniform uint showPlateaux;
uniform uint showProfileDistance;
uniform uint showFlowDirection;
uniform uint showGhostVertices;
uniform double seaLevelKm;

void main()
{  
	dvec4 p = dvec4(vertexPos.xyz / renderScale, 1.0LF);
	const Vertex V = vertices[gl_VertexID];
	
	// --- normal ---
	normal = vertexMisc1.xyz;
		
	// --- base color ---
	color = vec3(1.0) * clamp(float(vertexPos.w) - 10.0, 1.0, 10.0)/10.0;
	
	// --- false colors ---
	if (showFlowDirection == 1u)
		color = 0.5* (1.0 + vertexFlow.xyz);// * sqrt(vertexFlow.w);
	else if (showProfileDistance == 1u)
	{
		color.g = vertexMisc2.w;//false colors showing blend coefficient between river profile and raw terrain.
		color.g = color.g > 1.0 ? color.r : color.g;
	}	
	/*else if (showGhostVertices == 1u)
	{
		color.g = vertexMisc2.z;
	}*/
		
	if (vertexPos.w < seaLevelKm)
		color = vec3(0.0, 0.0, 0.5);	
	else
	{		
		if (showTectonicAge == 1u)
		{
			const float agelerp = smoothstep(0.0, 200.0, vertexMisc2.x);
			color *= vec3(1.0, 0.5 + 0.5*agelerp, agelerp);
		}
		else if (showPlateaux == 1u)
		{
			const float plateau = vertexMisc2.y > 0.5 ? 1.0:0.0;
			color *= vec3(1.0 - plateau, 1.0, 1.0 - plateau);
		}
		
		if (V.type == TYPE_RIVER)// && maxlod > (0u + 8u))
		{
			//color = vec3(vertexMisc1.w, vertexMisc1.w, 1.0);//show importance of the river too.	
			int river_id = int(vertexMisc1.w);
			//river_id = (16359979 + 43759862 * river_id) % 65536;
			float g = float(river_id / 256) / 255.0;
			float b = float(river_id % 256) / 255.0;
			color.r = 0.0;
			color.g = g;
			color.b = b;
		} else if (showDrainage == 1u && V.type == TYPE_DRAINAGE)
			color *= vec3(0.7, 0.0, 0.0);		
	}
	
	// --- Log depth ---
	dvec4 proj = projection * view * p;	
	proj.z = double(log2(max(1e-6, 1.0 + 1.0*float(proj.w))) * Fcoef - 1.0);  // log z - (see : http://outerra.blogspot.fr/2009/08/logarithmic-z-buffer.html)
	proj.z *= proj.w;
	
	// --- out ---
	gl_Position = vec4(proj);
}