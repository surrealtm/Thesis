#include "dbgdraw.h"
#include "core.h"

#include "memutils.h"

struct Dbg_Internal_Draw_Data {
	Resizable_Array<Debug_Draw_Line> lines;
	Resizable_Array<Debug_Draw_Triangle> triangles;
	Resizable_Array<Debug_Draw_Text> texts;
	Resizable_Array<Debug_Draw_Cuboid> cuboids;
	Resizable_Array<Debug_Draw_Sphere> spheres;
    b8 draw_labels;
    b8 draw_normals;
};

struct Dbg_Draw_Color {
	u8 r, g, b, a;
};

static Dbg_Draw_Color dbg_octree_depth_color_map[] = {
	{   0, 255,   0 },
	{  60, 220,  90 },
	{  40, 200, 100 },
	{  40, 180, 130 },
	{  40, 140, 180 },
	{  40, 100, 220 },
	{  40,  80, 240 },
	{  20,  30, 255 },
};

static f32 dbg_octree_depth_thickness_map[] = {
	0.25f,
	0.15f,
	0.10f,
	0.08f,
	0.06f,
	0.05f,
	0.035f,
	0.02f,
};

const f32 dbg_anchor_radius                = 0.5f;
const f32 dbg_triangle_wireframe_thickness = 0.03f;
const f32 dbg_triangle_normal_thickness    = 0.2f;
const f32 dbg_axis_gizmo_thickness         = 0.2f;

const real dbg_triangle_normal_length      = 1;

const Dbg_Draw_Color dbg_label_color          = { 255, 255, 255, 255 };
const Dbg_Draw_Color dbg_anchor_color         = { 255, 100, 100, 255 };
const Dbg_Draw_Color dbg_boundary_color       = { 100, 100, 100, 255 };
const Dbg_Draw_Color dbg_root_plane_color     = { 255, 193,   0, 100 };
const Dbg_Draw_Color dbg_clipping_plane_color = { 255,  60,  50, 100 };
const Dbg_Draw_Color dbg_volume_color         = { 215,  15, 219, 100 };
const Dbg_Draw_Color dbg_step_highlight_color = { 255, 255, 255, 255 };
const Dbg_Draw_Color dbg_normal_color         = {  50,  50, 255, 255 };

Allocator *dbg_alloc = Default_Allocator;

#if CORE_SINGLE_PRECISION
# define dbg_v3f(v) (v)
# define dbg_qtf(q) (q)
#else
# define dbg_v3f(v) v3f((f32) (v).x, (f32) (v).y, (f32) (v).z)
# define dbg_qtf(q) qtf((f32) (q).x, (f32) (q).y, (f32) (q).z, (f32) (q).w)
#endif

static
void debug_draw_line(Dbg_Internal_Draw_Data &_internal, vec3 p0, vec3 p1, f32 thickness, Dbg_Draw_Color color) {
    _internal.lines.add({ dbg_v3f(p0), dbg_v3f(p1), thickness, color.r, color.g, color.b });
}

static
void debug_draw_triangle(Dbg_Internal_Draw_Data &_internal, Triangle *triangle, Dbg_Draw_Color color) {
	_internal.triangles.add({ dbg_v3f(triangle->p0), dbg_v3f(triangle->p1), dbg_v3f(triangle->p2), color.r, color.g, color.b, color.a });

    if(_internal.draw_normals) {
        vec3 center = (triangle->p0 + triangle->p1 + triangle->p2) / static_cast<real>(3.);
        vec3 normal = triangle->n * dbg_triangle_normal_length;
        _internal.lines.add({ dbg_v3f(center), dbg_v3f(center + normal), dbg_triangle_normal_thickness, dbg_normal_color.r, dbg_normal_color.g, dbg_normal_color.b });
    }
}

static
void debug_draw_triangle_wireframe(Dbg_Internal_Draw_Data &_internal, Triangle *triangle, Dbg_Draw_Color color, f32 thickness) {
	_internal.lines.add({ dbg_v3f(triangle->p0), dbg_v3f(triangle->p1), thickness, color.r, color.g, color.b });
    _internal.lines.add({ dbg_v3f(triangle->p1), dbg_v3f(triangle->p2), thickness, color.r, color.g, color.b });
    _internal.lines.add({ dbg_v3f(triangle->p2), dbg_v3f(triangle->p0), thickness, color.r, color.g, color.b });

    if(_internal.draw_normals) {
        vec3 center = (triangle->p0 + triangle->p1 + triangle->p2) / static_cast<real>(3.);
        vec3 normal = triangle->n * dbg_triangle_normal_length;
        _internal.lines.add({ dbg_v3f(center), dbg_v3f(center + normal), dbg_triangle_normal_thickness, dbg_normal_color.r, dbg_normal_color.g, dbg_normal_color.b });
    }
}

static
void debug_draw_octree(Dbg_Internal_Draw_Data &_internal, Octree *node, Octree_Child_Index child_index, u8 depth) {
	vec3 p00 = node->center + vec3(-node->half_size.x, -node->half_size.y, -node->half_size.z),
		p01 = node->center + vec3( node->half_size.x, -node->half_size.y, -node->half_size.z),
		p02 = node->center + vec3( node->half_size.x, -node->half_size.y,  node->half_size.z),
		p03 = node->center + vec3(-node->half_size.x, -node->half_size.y,  node->half_size.z);

	vec3 p10 = node->center + vec3(-node->half_size.x,  node->half_size.y, -node->half_size.z),
		p11 = node->center + vec3( node->half_size.x,  node->half_size.y, -node->half_size.z),
		p12 = node->center + vec3( node->half_size.x,  node->half_size.y,  node->half_size.z),
		p13 = node->center + vec3(-node->half_size.x,  node->half_size.y,  node->half_size.z);

	const f32 thickness = dbg_octree_depth_thickness_map[min(depth, ARRAY_COUNT(dbg_octree_depth_thickness_map) - 1)];
	const Dbg_Draw_Color color = dbg_octree_depth_color_map[min(depth, ARRAY_COUNT(dbg_octree_depth_color_map) - 1)];
	
	b8 px = child_index == OCTREE_CHILD_COUNT ||  (child_index & OCTREE_CHILD_px_flag);
	b8 nx = child_index == OCTREE_CHILD_COUNT || !(child_index & OCTREE_CHILD_px_flag);
	b8 py = child_index == OCTREE_CHILD_COUNT ||  (child_index & OCTREE_CHILD_py_flag);	
	b8 ny = child_index == OCTREE_CHILD_COUNT || !(child_index & OCTREE_CHILD_py_flag);	
	b8 pz = child_index == OCTREE_CHILD_COUNT ||  (child_index & OCTREE_CHILD_pz_flag);
	b8 nz = child_index == OCTREE_CHILD_COUNT || !(child_index & OCTREE_CHILD_pz_flag);
	
	if(pz || py) debug_draw_line(_internal, p00, p01, thickness, color);
	if(pz || ny) debug_draw_line(_internal, p10, p11, thickness, color);
	if(nx || py) debug_draw_line(_internal, p01, p02, thickness, color);
	if(nx || ny) debug_draw_line(_internal, p11, p12, thickness, color);
	if(nz || py) debug_draw_line(_internal, p02, p03, thickness, color);
	if(nz || ny) debug_draw_line(_internal, p12, p13, thickness, color);
	if(px || py) debug_draw_line(_internal, p03, p00, thickness, color);
	if(px || ny) debug_draw_line(_internal, p13, p10, thickness, color);
	if(px || pz) debug_draw_line(_internal, p00, p10, thickness, color);
	if(nx || pz) debug_draw_line(_internal, p01, p11, thickness, color);
	if(nx || nz) debug_draw_line(_internal, p02, p12, thickness, color);
	if(px || nz) debug_draw_line(_internal, p03, p13, thickness, color);

	for(s64 i = 0; i < OCTREE_CHILD_COUNT; ++i) {
		if(node->children[i]) debug_draw_octree(_internal, node->children[i], (Octree_Child_Index) i, depth + 1);
	}
}

Debug_Draw_Data debug_draw_world(World *world, Debug_Draw_Options options) {
	Dbg_Internal_Draw_Data _internal;
	_internal.lines.allocator     = dbg_alloc;
	_internal.triangles.allocator = dbg_alloc;
	_internal.texts.allocator     = dbg_alloc;
	_internal.cuboids.allocator   = dbg_alloc;
	_internal.spheres.allocator   = dbg_alloc;

    _internal.draw_labels  = !!(options & DEBUG_DRAW_Labels);
    _internal.draw_normals = !!(options & DEBUG_DRAW_Normals);
    
	if(options & DEBUG_DRAW_Octree) {
		debug_draw_octree(_internal, &world->root, OCTREE_CHILD_COUNT, 0);
	}

	if(options & DEBUG_DRAW_Anchors) {
		for(auto &anchor: world->anchors) {
			_internal.spheres.add({ dbg_v3f(anchor.position), dbg_anchor_radius, dbg_anchor_color.r, dbg_anchor_color.g, dbg_anchor_color.b });

			if(_internal.draw_labels) _internal.texts.add({ dbg_v3f(anchor.position), anchor.dbg_name, dbg_label_color.r, dbg_label_color.g, dbg_label_color.b });
		}			
	}

	if(options & DEBUG_DRAW_Boundaries) {
		for(auto &boundary : world->boundaries) {
			_internal.cuboids.add({ dbg_v3f(boundary.position), dbg_v3f(boundary.dbg_half_size), dbg_qtf(boundary.dbg_rotation), dbg_boundary_color.r, dbg_boundary_color.g, dbg_boundary_color.b });

            if(_internal.draw_labels) _internal.texts.add({ dbg_v3f(boundary.position), boundary.dbg_name, dbg_label_color.r, dbg_label_color.g, dbg_label_color.b });
        }
	}

	if(options & DEBUG_DRAW_Clipping_Faces) {
		if(options & DEBUG_DRAW_Root_Planes) {
			for(s64 i = 0; i < world->root_clipping_triangles.count; ++i) {
				b8 active = world->dbg_step_initialized && !world->dbg_step_completed && world->dbg_step_root_triangle == i;
				Dbg_Draw_Color color = active ? dbg_step_highlight_color : dbg_root_plane_color;
				debug_draw_triangle(_internal, &world->root_clipping_triangles[i], color);
			}
		}
		
        for(s64 i = 0; i < world->boundaries.count; ++i) {
            Boundary *boundary = &world->boundaries[i];
            for(s64 j = 0; j < boundary->clipping_triangles.count; ++j) {
                b8 active = world->dbg_step_initialized && !world->dbg_step_completed && world->dbg_step_boundary == i && world->dbg_step_clipping_triangle == j;
				Dbg_Draw_Color color = active ? dbg_step_highlight_color : dbg_clipping_plane_color;
                debug_draw_triangle(_internal, &boundary->clipping_triangles[j], color);
			}
        }
    }

	if(options & DEBUG_DRAW_Clipping_Wireframes) {
		if(options & DEBUG_DRAW_Root_Planes) {
			for(s64 i = 0; i < world->root_clipping_triangles.count; ++i) {
				b8 active = world->dbg_step_initialized && !world->dbg_step_completed && world->dbg_step_root_triangle == i;
				Dbg_Draw_Color color = active ? dbg_step_highlight_color : dbg_root_plane_color;
				f32 thickness = active ? dbg_triangle_wireframe_thickness * 2 : dbg_triangle_wireframe_thickness;
				debug_draw_triangle_wireframe(_internal, &world->root_clipping_triangles[i], color, thickness);
			}
		}
		
        for(s64 i = 0; i < world->boundaries.count; ++i) {
            Boundary *boundary = &world->boundaries[i];
            for(s64 j = 0; j < boundary->clipping_triangles.count; ++j) {
                b8 active = world->dbg_step_initialized && !world->dbg_step_completed && world->dbg_step_boundary == i && world->dbg_step_clipping_triangle == j;
				Dbg_Draw_Color color = active ? dbg_step_highlight_color : dbg_clipping_plane_color;
				f32 thickness = active ? dbg_triangle_wireframe_thickness * 2 : dbg_triangle_wireframe_thickness;
				debug_draw_triangle_wireframe(_internal, &boundary->clipping_triangles[j], color, thickness);
			}
        }		
	}

	if(options & DEBUG_DRAW_Volume_Faces) {
		for(s64 i = 0; i < world->anchors.count; ++i) {
            Anchor *anchor = &world->anchors[i];
			for(s64 j = 0; j < anchor->volume_triangles.count; ++j) {
				Dbg_Draw_Color color = dbg_volume_color;
				debug_draw_triangle(_internal, &anchor->volume_triangles[j], color);
			}
		}
	}

	if(options & DEBUG_DRAW_Volume_Wireframes) {
		for(s64 i = 0; i < world->anchors.count; ++i) {
            Anchor *anchor = &world->anchors[i];
			for(s64 j = 0; j < anchor->volume_triangles.count; ++j) {
				Dbg_Draw_Color color = dbg_volume_color;
                f32 thickness = dbg_triangle_wireframe_thickness;
                debug_draw_triangle_wireframe(_internal, &anchor->volume_triangles[j], color, thickness);
			}
		}
	}

	if(options & DEBUG_DRAW_Axis_Gizmo) {
		_internal.lines.add({ v3f(0, 0, 0), v3f(3, 0, 0), dbg_axis_gizmo_thickness, 255, 0, 0 });
		_internal.lines.add({ v3f(0, 0, 0), v3f(0, 3, 0), dbg_axis_gizmo_thickness, 0, 255, 0 });
		_internal.lines.add({ v3f(0, 0, 0), v3f(0, 0, 3), dbg_axis_gizmo_thickness, 0, 0, 255 });
	}

	Debug_Draw_Data data = { 0 };
	data.lines          = _internal.lines.data;
	data.line_count     = _internal.lines.count;
	data.triangles      = _internal.triangles.data;
	data.triangle_count = _internal.triangles.count;
	data.texts          = _internal.texts.data;
	data.text_count     = _internal.texts.count;
	data.cuboids        = _internal.cuboids.data;
	data.cuboid_count   = _internal.cuboids.count;
	data.spheres        = _internal.spheres.data;
	data.sphere_count   = _internal.spheres.count;
	return data;
}

void free_debug_draw_data(Debug_Draw_Data *data) {
	dbg_alloc->deallocate(data->lines);
	data->line_count = 0;
    data->lines = null;
    
	dbg_alloc->deallocate(data->triangles);
	data->triangle_count = 0;
    data->triangles = null;
    
	dbg_alloc->deallocate(data->texts);
	data->text_count = 0;
    data->texts = null;
    
	dbg_alloc->deallocate(data->cuboids);
	data->cuboid_count = 0;
    data->cuboids = null;
    
	dbg_alloc->deallocate(data->spheres);
	data->sphere_count = 0;
    data->spheres = null;
}
