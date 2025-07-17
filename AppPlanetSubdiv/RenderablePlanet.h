#pragma once

#include "PlanetData.h"
#include "shader.h"
#include "tool.h"
#include "glversion.h"

#include <vector>
#include <fstream>


// ---- options ----
//#define SUBDIVISION_TIMER_QUERIES 1		// if defined then timer queries are launched for each subdivision, note that it stalls the gPU.





#define LOAD_NOISE_TEXTURE	

//#define BASE_RIVERS_LOOKUP_TECTONIC_ELEVATIONS				// if defined then lookup tectonic max elevations for growing the base river network (this is very costly!)

#define RENDER_SCALE						1000.0		// 1 opengl unit is 1000 km			
#define TARGET_EDGE_SIZE_PIXELS				8.0			// screenspace error tolerance on a triangle edge, in pixels
#define MAX_TESSELLATION_LOD				16			// this is < 1 m surface resolution for 50 km edge length at GPU LOD 0

#define BASE_MESH_SUBDIVISION_LEVELS		8			// [deprecated] number of levels required to build the base mesh (8 => 50 km average edge length)

#define PLANET_MAX_TRIANGLES				(20*1024*1024)

#define TYPE_NONE							0	
#define TYPE_SEA							1		
#define TYPE_CONTINENT						2
#define TYPE_RIVER							3
#define TYPE_COAST							4
#define TYPE_RIDGE							5
#define TYPE_OCTAHEDRON_EDGE				16

#define SPRING_FLOWVALUE					0.01f		// value of the flow at spring locations (slightly above zero)


struct alignas(16) EdgeGPU
{
	/// indexes of the two vertices
	int v0, v1;
	/// index of the middle split vertex
	int vm = -1;
	/// LSB to MSB: first byte = split status (0: not split, 1: ghost split, 2: split), second byte = boolean (ghost edge or not), third byte = subdivision level
	unsigned int status = 0;
	/// indexes of the two subedges if split
	int child0 = -1, child1 = -1;	
	/// indexes of the two adjacent faces
	int f0, f1;
	/// Type
	unsigned int type = TYPE_NONE;
	
	int padding[3];
};

struct alignas(16) VertexGPU
{
	/// first list of indexes of incident faces (or -1 if none)
	int faces_0[4] = { -1, -1, -1, -1 };
	/// second list of indexes of incident faces (or -1 if none)
	int faces_1[4] = { -1, -1, -1, -1 };
	/// PRNG seed
	unsigned int seed;
	/// boolean = ghost or not ghost vertex
	unsigned int status = 0;
	/// Type : 0 none, 1 sea, 2 continent, 3 river
	unsigned int type = TYPE_NONE;
	/// number of river branches at that vertex (0 or 1) not counting the main river flow, ie., this is the number of tributaries at that vertex.
	unsigned int branch_count = 0;
	
	/// some references (prim0 is used as a containing triangle index, for water animation ; prim1 is unused ; prim2 is a vertex reference used for lakes]
	int prim0 = -1, prim1 = -1, prim2 = -1;
	int padding1;

	//math::ivec4 padding2;
};

struct alignas(16) TriangleGPU
{
	/// indexes of the three edges (they appear in the right winding order (RH rule)).
	int e0, e1, e2;
	/// normal to the triangle
	float normal_x, normal_y, normal_z;
	/// LSB to MSB : byte 0 = boolean (split 1 or not 0), byte 2 = lod
	unsigned int status = 0;
	/// Type
	unsigned int type = TYPE_NONE;
};

struct alignas(32) VertexAttributesGPU
{
	/// xyz = 3D position, w = ground altitude
	math::dvec4 position;
	/// x = nearest river altitude, y = distance to nearest river, z = tectonic altitude (=max altitude), w = water altitude // - note we need double precision for water altitudes essentially.
	math::dvec4 data;
	/// xyz = water flow direction normalized, w = normalized flow quantity
	math::vec4 flow;	
	/// x = nearest ravin altitude, y = distance to nearest ravin, z = hilly landscape [0, 1], w = ravin flow value in [0, 1] (unused for now)
	math::vec4 misc1;
	/// x = crust age in Ma, y = plateau presence in [0, 1], z = desert/wet biome (1 is desert,0 is wet), w = river profile indirection
	math::vec4 misc2;
	//
	math::vec4 padding_and_debug;
};

struct alignas(32) WaterVertexAttributesGPU
{
	/// xyz = 3D position, w = average water altitude (before wave displacement)
	math::dvec4 position;
	/// xyz = normal to water surface, w = unused
	math::vec4 normal;
	/// xyz = water flow direction normalized, w = unused
	math::vec4 flow;

	/// indexes of the vertices acting as river primitives [update: now only prim.x is used, as a triangle index]
	math::ivec4 prim;
	
	math::vec4 padding2;
};


/// internal use only
struct RiverGrowingNode
{
	double num_edges_to_mouth;
	//double branching_level = 0.0;
	double length_to_mouth = 0.0;
	double target_river_length;//desired river length (max length)	
	double choice_penalty;
	double normalized_tecto_altitude;
	double priority;
	float max_flow_value;//unused ?
	int edge;
	int base_vertex;
	int tip_river_node;
	int tip_vertex;
	bool spring;
	bool full_grown;//unused ?
};

struct RiverNode
{
	int vertex = -1;
	int prevnode = -1;
	int nextnode1 = -1;
	int nextnode2 = -1;
	int nextedge1 = -1;
	int nextedge2 = -1;
	float flow_value = 0.0f;
	int horton_stralher = -1;
	float rosgen_type = -1.0f;	
	int asymetric_branching = -1;	
	double length_to_mouth = 0.0;
	int river_system_id = -1;//the river system this nodes belongs to
	bool disabled = false;//true if this river node belongs to a river system that has been discarded.
};



class RenderablePlanet : protected Q_OPENGL_FUNCS
{

public:

	RenderablePlanet(const PlanetData * planet) : m_planet(planet) 
	{
		initializeOpenGLFunctions();
	}
	~RenderablePlanet() { release(); }

	bool init(int viewportWidth, int viewportHeight, GLuint default_fbo, std::ostream & shader_log);
	void release();

	void render(double timeSeconds, int viewwidth, int viewheight, const math::dvec3 & cameraPosition, const math::dmat4 & view, const math::dmat4 & projection);
	void tessellate(const math::dvec3 & cameraPositionKm, double fov, double nearplaneKm, double farplaneKm, int viewwidth, int viewheight);
	
	void setSunDirection(const math::dvec3 & sundir) { m_sun_direction = sundir; }
	void toggleWireframe() { m_option_wireframe = !m_option_wireframe; }
	void toggleDrainageColor() { m_option_show_drainage = !m_option_show_drainage; }
	void toggleTectonicAgeColor() { m_option_show_tectonic_age = !m_option_show_tectonic_age; }
	void togglePlateauxColor() { m_option_show_plateaux_presence = !m_option_show_plateaux_presence; }
	//void toggleGPURelief() { m_option_no_gpu_relief = !m_option_no_gpu_relief; }
	void toggleSimpleShading() { m_option_debug_shading = !m_option_debug_shading; }
	void toggleBlendProfiles() { m_option_blend_profiles_with_terrain = !m_option_blend_profiles_with_terrain; }
	void toggleGenerateDrainage() { m_option_generate_drainage = !m_option_generate_drainage; }
	void toggleGenerateRiverProfiles() { m_option_generate_valley_profiles = !m_option_generate_valley_profiles; }
	void toggleGenerateLakes() { m_option_generate_lakes = !m_option_generate_lakes; }
	void toggleProfileDistanceColor() { m_option_show_profile_distance = !m_option_show_profile_distance; }
	void toggleFlowDirectionColor() { m_option_show_flow_direction = !m_option_show_flow_direction; }
	void toggleGhostVertexColor() { m_option_show_ghostvertices = !m_option_show_ghostvertices; }
	void toggleRiverPrimitives() { m_option_river_primitives = !m_option_river_primitives; }
	//void togglePureMidPointModel() { m_option_pure_midpoint_model = !m_option_pure_midpoint_model; }

	void reloadShaders() { impl_reload_shaders(); }

private:
	
	bool doTessellationPass(int lod);
	
	void makePoissonDelaunayBaseMesh();
	
	void createBaseRiverNetwork();
	void createAllRiverMouth(std::vector<RiverGrowingNode> & nodes);
	void postprocessBaseRiverNetwork();
	bool isTriangleSeaCoast(int triangle_index) const;
	void getAdjacentEdges(int vertex_index, std::vector<int> & adjacency) const;
	
	double assignWaterElevations(int node, double water_depth_at_mouth);

	void bindBuffers();
	void renderDebug(const math::dvec3 & cameraPosition, const math::dmat4 & view, const math::dmat4 & projection);
	
	bool initGL(std::ostream & shader_log);
	void impl_reload_shaders();
	bool loadTextures();
	bool buildRenderTargets();

	bool initNoiseTexture();
	bool initProfilesTexture();

private:

	const PlanetData * m_planet = nullptr;

	std::vector<EdgeGPU> m_base_edges;
	std::vector<VertexGPU> m_base_vertices;
	std::vector<TriangleGPU> m_base_triangles;
	std::vector<VertexAttributesGPU> m_base_vattrib;

	RiverNode * m_river_nodes = nullptr;//storage for all river nodes
	std::list<int> m_rivers;//indexes of the river mouthes into m_river_nodes
	int m_river_nodes_size, m_river_nodes_max_size;
	
	Shader* m_compute_edgesplit = nullptr, * m_compute_edgesplit_simple = nullptr;// *m_compute_edgesplit_norelief = nullptr, * m_compute_edgesplit_puremidpoint = nullptr;
	Shader * m_compute_ghostmarking = nullptr;
	Shader* m_compute_ghostsplit = nullptr;// *m_compute_ghostsplit_puremidpoint = nullptr;
	Shader* m_compute_facesplit_riverbranching = nullptr, * m_compute_facesplit_erosion = nullptr, * m_compute_facesplit_simple = nullptr;// , * m_compute_facesplit_puremidpoint = nullptr;
	Shader* m_compute_profiles = nullptr, * m_compute_postprocess1 = nullptr, * m_compute_postprocess2 = nullptr;// , * m_compute_profiles_puremidpoint = nullptr;
	Shader * m_compute_water_0 = nullptr, *m_compute_water_1 = nullptr, *m_compute_waterghosts = nullptr;
	Shader * m_compute_ibo = nullptr;
	Shader * m_render_test = nullptr;
	Shader * m_geometry_terrain_pass = nullptr, *m_geometry_water_pass = nullptr;
	Shader * m_render_deferred_final = nullptr;

	math::dvec3 m_sun_direction = math::dvec3(1.0, 0.0, 0.0);
	math::dvec3 m_camera_position_km;
	double m_camera_nearplane_km, m_camera_farplane_km, m_camera_fov;
	double m_base_edge_length;
	double MAX_RIVER_LENGTH;
	int m_viewheight, m_viewwidth;

	GLuint m_pos_vbo = 0, m_ibo = 0, m_vao = 0;
	GLuint m_water_vbo = 0, m_water_ibo = 0, m_water_vao = 0, m_water_ibo_counter = 0;
	GLuint m_edge_counter = 0, m_face_counter = 0, m_vertex_counter = 0, m_ibo_counter = 0;
	GLuint m_edge_buffer = 0, m_vertex_buffer = 0, m_face_buffer = 0;

	GLuint m_terrainTextureArray = 0, m_mountainGrassTexture = 0, m_rockTexture = 0, m_desertTexture = 0, m_snowTexture = 0;

	GLuint m_geometry_buffer_0 = 0, m_geometry_buffer_1 = 0, m_geometry_buffer_2 = 0, m_water_geometry_buffer_0 = 0, m_water_geometry_buffer_1 = 0;
	GLuint m_depth_buffer_terrain = 0, m_depth_buffer_water = 0;
	GLuint m_framebuffer_terrain = 0, m_framebuffer_water = 0;

	GLuint m_profiles_texture = 0;
	GLuint m_noiseTexture3D = 0;
	GLuint m_default_fbo = 0;

	GLuint m_timequery = 0;
	double m_modeling_time_worse = 0.0;
	double m_modeling_time_avg = 0.0;
	double m_reset_time = 0.0;
	double m_subdiv_time = 0.0;
	double m_edges_time = 0.0, m_edges_run_time = 0.0;
	double m_ghost_time = 0.0, m_ghost_run_time = 0.0;
	double m_faces_time = 0.0, m_faces_run_time = 0.0;
	double m_profiles_time = 0.0;
	double m_postprocess_time = 0.0;
	int m_modeling_runs = 0;

	unsigned int m_edge_end, m_edge_start, m_vertex_end, m_vertex_start, m_face_end, m_face_start;
	unsigned int m_ibo_count = 0, m_water_ibo_count = 0;

	int debug_counter = 0;

	bool m_viewport_changed = false;

	bool m_option_debug_shading = false;
	bool m_option_generate_valley_profiles = true;
	bool m_option_generate_drainage = true;
	bool m_option_generate_lakes = true;
	bool m_option_show_ghostvertices = false;
	bool m_option_wireframe = false;
	bool m_option_show_drainage = false;
	bool m_option_show_tectonic_age = false;
	bool m_option_show_plateaux_presence = false;
	bool m_option_show_flow_direction = false;
	bool m_option_show_profile_distance = false;
	//bool m_option_no_gpu_relief = false;
	bool m_option_blend_profiles_with_terrain = true;
	bool m_option_river_primitives = true;
	//bool m_option_pure_midpoint_model = false;

	bool m_debug = false;	
};