#include "dbgdraw.h"
#include "core.h"

#include "memutils.h"

struct Dbg_Internal_Draw_Data {
	Resizable_Array<Debug_Draw_Line> lines;
	Resizable_Array<Debug_Draw_Triangle> triangles;
	Resizable_Array<Debug_Draw_Text> texts;
	Resizable_Array<Debug_Draw_Cuboid> cuboids;
	Resizable_Array<Debug_Draw_Sphere> spheres;
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

const f32 dbg_anchor_radius = 0.5f;
const f32 dbg_triangle_wireframe_thickness = 0.2f;

const Dbg_Draw_Color dbg_label_color          = { 255, 255, 255, 255 };
const Dbg_Draw_Color dbg_anchor_color         = { 255, 100, 100, 255 };
const Dbg_Draw_Color dbg_boundary_color       = { 100, 100, 100, 255 };
const Dbg_Draw_Color dbg_clipping_plane_color = { 255,  60,  50, 100 };
const Dbg_Draw_Color dbg_volume_color         = { 215,  15, 219, 100 };

Allocator *dbg_alloc = Default_Allocator;

static
void debug_draw_octree(Dbg_Internal_Draw_Data &_internal, Octree *node, Octree_Child_Index child_index, u8 depth) {
	v3f p00 = node->center + v3f(-node->half_size.x, -node->half_size.y, -node->half_size.z),
		p01 = node->center + v3f( node->half_size.x, -node->half_size.y, -node->half_size.z),
		p02 = node->center + v3f( node->half_size.x, -node->half_size.y,  node->half_size.z),
		p03 = node->center + v3f(-node->half_size.x, -node->half_size.y,  node->half_size.z);

	v3f p10 = node->center + v3f(-node->half_size.x,  node->half_size.y, -node->half_size.z),
		p11 = node->center + v3f( node->half_size.x,  node->half_size.y, -node->half_size.z),
		p12 = node->center + v3f( node->half_size.x,  node->half_size.y,  node->half_size.z),
		p13 = node->center + v3f(-node->half_size.x,  node->half_size.y,  node->half_size.z);

	const f32 thickness = dbg_octree_depth_thickness_map[min(depth, ARRAY_SIZE(dbg_octree_depth_thickness_map) - 1)];
	const Dbg_Draw_Color color = dbg_octree_depth_color_map[min(depth, ARRAY_SIZE(dbg_octree_depth_color_map) - 1)];

#define line(p0, p1) _internal.lines.add({ p0, p1, thickness, color.r, color.g, color.b })
	
	b8 px = child_index == OCTREE_CHILD_COUNT ||  (child_index & OCTREE_CHILD_px_flag);
	b8 nx = child_index == OCTREE_CHILD_COUNT || !(child_index & OCTREE_CHILD_px_flag);
	b8 py = child_index == OCTREE_CHILD_COUNT ||  (child_index & OCTREE_CHILD_py_flag);	
	b8 ny = child_index == OCTREE_CHILD_COUNT || !(child_index & OCTREE_CHILD_py_flag);	
	b8 pz = child_index == OCTREE_CHILD_COUNT ||  (child_index & OCTREE_CHILD_pz_flag);
	b8 nz = child_index == OCTREE_CHILD_COUNT || !(child_index & OCTREE_CHILD_pz_flag);
	
	if(pz || py) line(p00, p01);
	if(pz || ny) line(p10, p11);

	if(nx || py) line(p01, p02);
	if(nx || ny) line(p11, p12);

	if(nz || py) line(p02, p03);
	if(nz || ny) line(p12, p13);

	if(px || py) line(p03, p00);
	if(px || ny) line(p13, p10);

	if(px || pz) line(p00, p10);
	if(nx || pz) line(p01, p11);
	if(nx || nz) line(p02, p12);
	if(px || nz) line(p03, p13);

#undef line

	for(s64 i = 0; i < OCTREE_CHILD_COUNT; ++i) {
		if(node->children[i]) debug_draw_octree(_internal, node->children[i], (Octree_Child_Index) i, depth + 1);
	}
}

static
void debug_draw_clipping_plane(Dbg_Internal_Draw_Data &_internal, Clipping_Plane *plane, b8 wireframe) {
	if(wireframe) {
		for(auto *triangle : plane->triangles) {
			_internal.lines.add({ triangle->p0, triangle->p1, dbg_triangle_wireframe_thickness, dbg_clipping_plane_color.r, dbg_clipping_plane_color.g, dbg_clipping_plane_color.b });
			_internal.lines.add({ triangle->p1, triangle->p2, dbg_triangle_wireframe_thickness, dbg_clipping_plane_color.r, dbg_clipping_plane_color.g, dbg_clipping_plane_color.b });
			_internal.lines.add({ triangle->p2, triangle->p0, dbg_triangle_wireframe_thickness, dbg_clipping_plane_color.r, dbg_clipping_plane_color.g, dbg_clipping_plane_color.b });
		}
	} else {
		for(auto *triangle : plane->triangles) {
			_internal.triangles.add({ triangle->p0, triangle->p1, triangle->p2, dbg_clipping_plane_color.r, dbg_clipping_plane_color.g, dbg_clipping_plane_color.b, dbg_clipping_plane_color.a });
		}
	}
}

Debug_Draw_Data debug_draw_world(World *world, Debug_Draw_Options options) {
	Dbg_Internal_Draw_Data _internal;
	_internal.lines.allocator     = dbg_alloc;
	_internal.triangles.allocator = dbg_alloc;
	_internal.texts.allocator     = dbg_alloc;
	_internal.cuboids.allocator   = dbg_alloc;
	_internal.spheres.allocator   = dbg_alloc;

    b8 labels = !!(options & DEBUG_DRAW_Labels);
    
	if(options & DEBUG_DRAW_Octree) {
		debug_draw_octree(_internal, &world->root, OCTREE_CHILD_COUNT, 0);
	}

	if(options & DEBUG_DRAW_Anchors) {
		for(auto *anchor: world->anchors) {
			_internal.spheres.add({ anchor->position, dbg_anchor_radius, dbg_anchor_color.r, dbg_anchor_color.g, dbg_anchor_color.b });

			if(labels) _internal.texts.add({ anchor->position, anchor->dbg_name, dbg_label_color.r, dbg_label_color.g, dbg_label_color.b });
		}			
	}

	if(options & DEBUG_DRAW_Boundaries) {
		for(auto *boundary : world->boundaries) {
			_internal.cuboids.add({ boundary->position, boundary->dbg_half_size, boundary->dbg_rotation, dbg_boundary_color.r, dbg_boundary_color.g, dbg_boundary_color.b });

            if(labels) _internal.texts.add({ boundary->position, boundary->dbg_name, dbg_label_color.r, dbg_label_color.g, dbg_label_color.b });
        }
	}

	if(options & DEBUG_DRAW_Clipping_Plane_Faces) {
		/*
		// Root clipping planes cause more confusion than good right now I think.
		for(auto *root_plane : world->root_clipping_planes) {
			debug_draw_clipping_plane(_internal, root_plane);
		}
		*/

        for(auto *boundary : world->boundaries) {
            for(auto *plane : boundary->clipping_planes) {
                debug_draw_clipping_plane(_internal, plane, false);
			}
        }
    }

	if(options & DEBUG_DRAW_Clipping_Plane_Wireframes) {
		/*
		// Root clipping planes cause more confusion than good right now I think.
		for(auto *root_plane : world->root_clipping_planes) {
			debug_draw_clipping_plane(_internal, root_plane);
		}
		*/

        for(auto *boundary : world->boundaries) {
            for(auto *plane : boundary->clipping_planes) {
                debug_draw_clipping_plane(_internal, plane, true);
			}
        }		
	}

	if(options & DEBUG_DRAW_Volume_Faces) {
		for(auto *anchor : world->anchors) {
			for(auto *triangle : anchor->volume) {
				_internal.triangles.add({ triangle->p0, triangle->p1, triangle->p2, dbg_volume_color.r, dbg_volume_color.g, dbg_volume_color.b, dbg_volume_color.a });
			}
		}
	}

	if(options & DEBUG_DRAW_Volume_Wireframes) {
		for(auto *anchor : world->anchors) {
			for(auto * triangle : anchor->volume) {
				_internal.lines.add({ triangle->p0, triangle->p1, dbg_triangle_wireframe_thickness, dbg_volume_color.r, dbg_volume_color.g, dbg_volume_color.b });
				_internal.lines.add({ triangle->p1, triangle->p2, dbg_triangle_wireframe_thickness, dbg_volume_color.r, dbg_volume_color.g, dbg_volume_color.b });
				_internal.lines.add({ triangle->p2, triangle->p0, dbg_triangle_wireframe_thickness, dbg_volume_color.r, dbg_volume_color.g, dbg_volume_color.b });
			}
		}
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

	dbg_alloc->deallocate(data->triangles);
	data->triangle_count = 0;

	dbg_alloc->deallocate(data->texts);
	data->text_count = 0;

	dbg_alloc->deallocate(data->cuboids);
	data->cuboid_count = 0;

	dbg_alloc->deallocate(data->spheres);
	data->sphere_count = 0;
}
