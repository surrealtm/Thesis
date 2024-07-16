#include "dbgdraw.h"
#include "world.h"
#include "bvh.h"
#include "floodfill.h"

#include "memutils.h"
#include "hash_table.h" // For hash functions

#define DBG_DRAW_BVH_TRIANGLES true

struct Dbg_Internal_Draw_Data {
	Resizable_Array<Debug_Draw_Line> lines;
	Resizable_Array<Debug_Draw_Triangle> triangles;
	Resizable_Array<Debug_Draw_Text> texts;
	Resizable_Array<Debug_Draw_Cuboid> cuboids;
	Resizable_Array<Debug_Draw_Sphere> spheres;
    b8 draw_labels;
    b8 draw_normals;
    s64 bvh_node_counter;
};

struct Dbg_Draw_Color {
	u8 r, g, b, a;
};

static Dbg_Draw_Color dbg_bvh_depth_color_map[] = {
	{   0, 255,   0, 255 },
	{  60, 220,  90, 255 },
	{  40, 200, 100, 255 },
	{  40, 180, 130, 255 },
	{  40, 140, 180, 255 },
	{  40, 100, 220, 255 },
	{  40,  80, 240, 255 },
	{  20,  30, 255, 255 },
};

static f32 dbg_bvh_depth_thickness_map[] = {
	0.25f,
	0.15f,
	0.10f,
	0.08f,
	0.06f,
	0.05f,
	0.035f,
	0.02f,
};

static_assert(ARRAY_COUNT(dbg_bvh_depth_color_map) == ARRAY_COUNT(dbg_bvh_depth_thickness_map), "These two arrays must be of the same size.");

const f32 dbg_anchor_radius             = 0.5f;
const f32 dbg_wireframe_thickness       = 0.01f;
const f32 dbg_normal_thickness          = 0.2f;
const f32 dbg_flood_fill_cell_thickness = 0.03f;
const f32 dbg_axis_gizmo_thickness      = 0.2f;
const f32 dbg_cell_relation_thickness   = 0.1f;

const real dbg_triangle_normal_length   = 1;

const Dbg_Draw_Color dbg_label_color           = { 255, 255, 255, 255 };
const Dbg_Draw_Color dbg_anchor_color          = { 255, 100, 100, 255 };
const Dbg_Draw_Color dbg_delimiter_color       = { 100, 100, 100, 255 };
const Dbg_Draw_Color dbg_root_plane_color      = { 255, 193,   0, 100 };
const Dbg_Draw_Color dbg_delimiter_plane_color = { 255,  60,  50, 100 };
const Dbg_Draw_Color dbg_volume_color          = { 215,  15, 219, 100 };
const Dbg_Draw_Color dbg_flood_fill_cell_color = { 200, 200, 200, 255 };
const Dbg_Draw_Color dbg_step_highlight_color  = { 255, 255, 255, 255 };
const Dbg_Draw_Color dbg_normal_color          = {  50,  50, 255, 255 };

const Dbg_Draw_Color dbg_cell_flooded_color  = { 0, 255, 255, 255 };
const Dbg_Draw_Color dbg_cell_frontier_color = { 255, 0, 0, 255 };
const Dbg_Draw_Color dbg_cell_relation_color = { 100, 100, 100, 255 };

Allocator *dbg_alloc = Default_Allocator;

#if CORE_SINGLE_PRECISION
# define dbg_v3f(v) (v)
# define dbg_qtf(q) (q)
#else
# define dbg_v3f(v) v3f((f32) (v).x, (f32) (v).y, (f32) (v).z)
# define dbg_qtf(q) qtf((f32) (q).x, (f32) (q).y, (f32) (q).z, (f32) (q).w)
#endif

static
Dbg_Draw_Color dbg_draw_color_from_hash(u64 hash) {
    Dbg_Draw_Color color;
    color.r = (u8) ((hash & 0x0000ff) >> 0);
    color.g = (u8) ((hash & 0x00ff00) >> 8);
    color.b = (u8) ((hash & 0xff0000) >> 16);
    color.a = 255;
    return color;
}

static
void debug_draw_line(Dbg_Internal_Draw_Data &_internal, vec3 p0, vec3 p1, f32 thickness, Dbg_Draw_Color color) {
    _internal.lines.add({ dbg_v3f(p0), dbg_v3f(p1), thickness, color.r, color.g, color.b });
}

static
void debug_draw_triangle(Dbg_Internal_Draw_Data &_internal, Triangle *triangle, Dbg_Draw_Color color) {
	_internal.triangles.add({ dbg_v3f(triangle->p0), dbg_v3f(triangle->p1), dbg_v3f(triangle->p2), color.r, color.g, color.b, color.a });
}

static
void debug_draw_triangle_wireframe(Dbg_Internal_Draw_Data &_internal, Triangle *triangle, Dbg_Draw_Color color, f32 thickness) {
    debug_draw_line(_internal, triangle->p0, triangle->p1, thickness, color);
    debug_draw_line(_internal, triangle->p1, triangle->p2, thickness, color);
    debug_draw_line(_internal, triangle->p2, triangle->p0, thickness, color);
}

static
void debug_draw_cuboid_wireframe(Dbg_Internal_Draw_Data &_internal, vec3 center, vec3 half_size, f32 thickness, Dbg_Draw_Color color) {
    debug_draw_line(_internal, center + vec3(-half_size.x, -half_size.y, -half_size.z), center + vec3(+half_size.x, -half_size.y, -half_size.z), thickness, color);
    debug_draw_line(_internal, center + vec3(+half_size.x, -half_size.y, -half_size.z), center + vec3(+half_size.x, -half_size.y, +half_size.z), thickness, color);
    debug_draw_line(_internal, center + vec3(+half_size.x, -half_size.y, +half_size.z), center + vec3(-half_size.x, -half_size.y, +half_size.z), thickness, color);
    debug_draw_line(_internal, center + vec3(-half_size.x, -half_size.y, +half_size.z), center + vec3(-half_size.x, -half_size.y, -half_size.z), thickness, color);

    debug_draw_line(_internal, center + vec3(-half_size.x, +half_size.y, -half_size.z), center + vec3(+half_size.x, +half_size.y, -half_size.z), thickness, color);
    debug_draw_line(_internal, center + vec3(+half_size.x, +half_size.y, -half_size.z), center + vec3(+half_size.x, +half_size.y, +half_size.z), thickness, color);
    debug_draw_line(_internal, center + vec3(+half_size.x, +half_size.y, +half_size.z), center + vec3(-half_size.x, +half_size.y, +half_size.z), thickness, color);
    debug_draw_line(_internal, center + vec3(-half_size.x, +half_size.y, +half_size.z), center + vec3(-half_size.x, +half_size.y, -half_size.z), thickness, color);

	debug_draw_line(_internal, center + vec3(+half_size.x, -half_size.y, +half_size.z), center + vec3(+half_size.x, +half_size.y, +half_size.z), thickness, color);
    debug_draw_line(_internal, center + vec3(+half_size.x, -half_size.y, -half_size.z), center + vec3(+half_size.x, +half_size.y, -half_size.z), thickness, color);
    debug_draw_line(_internal, center + vec3(-half_size.x, -half_size.y, -half_size.z), center + vec3(-half_size.x, +half_size.y, -half_size.z), thickness, color);
    debug_draw_line(_internal, center + vec3(-half_size.x, -half_size.y, +half_size.z), center + vec3(-half_size.x, +half_size.y, +half_size.z), thickness, color);
}

static
void debug_draw_cube_wireframe(Dbg_Internal_Draw_Data &_internal, vec3 center, real half_size, f32 thickness, Dbg_Draw_Color color) {
    debug_draw_cuboid_wireframe(_internal, center, vec3(half_size, half_size, half_size), thickness, color);
}

static
void debug_draw_triangulated_plane(Dbg_Internal_Draw_Data &_internal, Triangulated_Plane *plane, Dbg_Draw_Color color) {
    for(Triangle &triangle : plane->triangles) {
        debug_draw_triangle(_internal, &triangle, color);

        if(_internal.draw_normals) {
            vec3 center = (triangle.p0 + triangle.p1 + triangle.p2) / 3.;
            debug_draw_line(_internal, center, center + plane->n * dbg_triangle_normal_length, dbg_normal_thickness, dbg_normal_color);
        }
    }
}

static
void debug_draw_triangulated_plane_wireframe(Dbg_Internal_Draw_Data &_internal, Triangulated_Plane *plane, Dbg_Draw_Color color) {
    for(Triangle &triangle : plane->triangles) {
        debug_draw_triangle_wireframe(_internal, &triangle, color, dbg_wireframe_thickness);

        if(_internal.draw_normals) {
            vec3 center = (triangle.p0 + triangle.p1 + triangle.p2) / 3.;
            debug_draw_line(_internal, center, center + plane->n * dbg_triangle_normal_length, dbg_normal_thickness, dbg_normal_color);
        }
    }
}

static
void debug_draw_bvh(Dbg_Internal_Draw_Data &_internal, BVH *bvh, BVH_Node *node, s64 depth, b8 left) {
    Dbg_Draw_Color color = dbg_draw_color_from_hash(fnv1a_64(&_internal.bvh_node_counter, sizeof(s64)));
    ++_internal.bvh_node_counter;

    vec3 center = (node->min + node->max) / 2.;
    vec3 half_size = (node->max - node->min) / 2.;
    debug_draw_cuboid_wireframe(_internal, center, half_size, dbg_bvh_depth_thickness_map[depth], color);

#if DBG_DRAW_BVH_TRIANGLES
    if(node->leaf) {
        s64 one_plus_last = node->first_entry_index + node->entry_count;
        for(s64 i = node->first_entry_index; i < one_plus_last; ++i) {
            auto &entry = bvh->entries[i];
            debug_draw_triangle_wireframe(_internal, &entry.triangle, color, .01f);
        }
    }
#endif

    if(!node->leaf) {
        if(node->children[0]) debug_draw_bvh(_internal, bvh, node->children[0], min(depth + 1, ARRAY_COUNT(dbg_bvh_depth_color_map) - 1), true);
        if(node->children[1]) debug_draw_bvh(_internal, bvh, node->children[1], min(depth + 1, ARRAY_COUNT(dbg_bvh_depth_color_map) - 1), false);
    }
}

static
void debug_draw_bvh(Dbg_Internal_Draw_Data &_internal, BVH *bvh) {
    _internal.bvh_node_counter = 0;
    debug_draw_bvh(_internal, bvh, &bvh->root, 0, true);
}

static
void debug_draw_flood_fill_cell_center(Dbg_Internal_Draw_Data &_internal, Flood_Fill *ff, vec3 center, Dbg_Draw_Color color) {
    real half_size = .1 * ff->cell_world_space_size;
    f32 thickness = (f32) (half_size / 4.f);
    debug_draw_line(_internal, center - vec3(half_size, 0., 0.), center + vec3(half_size, 0., 0.), thickness, color);
    debug_draw_line(_internal, center - vec3(0., half_size, 0.), center + vec3(0., half_size, 0.), thickness, color);
    debug_draw_line(_internal, center - vec3(0., 0., half_size), center + vec3(0., 0., half_size), thickness, color);
}

static
void debug_draw_flood_fill(Dbg_Internal_Draw_Data &_internal, Flood_Fill *ff) {
	for(s32 x = 0; x < ff->hx; ++x) {
        for(s32 y = 0; y < ff->hy; ++y) {
            for(s32 z = 0; z < ff->hz; ++z) {
                vec3 center = get_cell_world_space_center(ff, v3i(x, y, z));

                //
                // Draw the outline. Only draw the "required" lines to avoid a lot of overhead by duplicate lines.
                //
                {
					real half_size       = ff->cell_world_space_size / 2.f;
					f32 thickness        = dbg_flood_fill_cell_thickness;
					Dbg_Draw_Color color = dbg_flood_fill_cell_color;
                    
                    b8 endx = x + 1 == ff->hx;
                    b8 endy = y + 1 == ff->hy;
                    b8 endz = z + 1 == ff->hz;

                    b8 startx = x == 0;
                    b8 startz = z == 0;

                    debug_draw_line(_internal, center + vec3(-half_size, -half_size, -half_size), center + vec3(+half_size, -half_size, -half_size), thickness, color);
                    debug_draw_line(_internal, center + vec3(-half_size, -half_size, +half_size), center + vec3(-half_size, -half_size, -half_size), thickness, color);

                    if(endx) debug_draw_line(_internal, center + vec3(+half_size, -half_size, -half_size), center + vec3(+half_size, -half_size, +half_size), thickness, color);
                    if(endz) debug_draw_line(_internal, center + vec3(+half_size, -half_size, +half_size), center + vec3(-half_size, -half_size, +half_size), thickness, color);

					if(endy) {
						debug_draw_line(_internal, center + vec3(-half_size, +half_size, -half_size), center + vec3(+half_size, +half_size, -half_size), thickness, color);
                        debug_draw_line(_internal, center + vec3(-half_size, +half_size, +half_size), center + vec3(-half_size, +half_size, -half_size), thickness, color);

						if(endx) debug_draw_line(_internal, center + vec3(+half_size, +half_size, -half_size), center + vec3(+half_size, +half_size, +half_size), thickness, color);
						if(endz) debug_draw_line(_internal, center + vec3(+half_size, +half_size, +half_size), center + vec3(-half_size, +half_size, +half_size), thickness, color);
					}
                    
                    debug_draw_line(_internal, center + vec3(-half_size, -half_size, -half_size), center + vec3(-half_size, +half_size, -half_size), thickness, color);

                    if(endz || endx) debug_draw_line(_internal, center + vec3(+half_size, -half_size, +half_size), center + vec3(+half_size, +half_size, +half_size), thickness, color);

                    if(endx && startz) debug_draw_line(_internal, center + vec3(+half_size, -half_size, -half_size), center + vec3(+half_size, +half_size, -half_size), thickness, color);
                    if(endz && startx) debug_draw_line(_internal, center + vec3(-half_size, -half_size, +half_size), center + vec3(-half_size, +half_size, +half_size), thickness, color);
                }

                //
                // Draw the center indicating the state of the cell.
                //
                {
                    Cell *cell = get_cell(ff, v3i(x, y, z));
                    switch(cell->state) {
                    case CELL_Untouched: break;

                    case CELL_Currently_In_Frontier:
                        debug_draw_flood_fill_cell_center(_internal, ff, center, dbg_cell_frontier_color);
                        break;
                        
                    case CELL_Has_Been_Flooded:
                        debug_draw_flood_fill_cell_center(_internal, ff, center, dbg_cell_flooded_color);
                        //_internal.lines.add({ center, dbg_v3f(get_cell_world_space_center(ff, cell->added_from_cell)), dbg_cell_relation_thickness, dbg_cell_relation_color.r, dbg_cell_relation_color.r, dbg_cell_relation_color.b }); // Draw a connection between the cell that added this cell to the frontier.
                        break;
                    }
                }
            }
        }
    }

    debug_draw_flood_fill_cell_center(_internal, ff, ff->world_space_center, Dbg_Draw_Color { 255, 0, 0, 255 });
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

	if(options & DEBUG_DRAW_BVH) {
        debug_draw_bvh(_internal, world->bvh);
	}

	if(options & DEBUG_DRAW_Anchors) {
		for(auto &anchor: world->anchors) {
			_internal.spheres.add({ dbg_v3f(anchor.position), dbg_anchor_radius, dbg_anchor_color.r, dbg_anchor_color.g, dbg_anchor_color.b });
			if(_internal.draw_labels) _internal.texts.add({ dbg_v3f(anchor.position), anchor.dbg_name, dbg_label_color.r, dbg_label_color.g, dbg_label_color.b });
		}			
	}

	if(options & DEBUG_DRAW_Delimiters) {
		for(auto &delimiter : world->delimiters) {
			_internal.cuboids.add({ dbg_v3f(delimiter.position), dbg_v3f(delimiter.dbg_half_size), dbg_qtf(delimiter.dbg_rotation), dbg_delimiter_color.r, dbg_delimiter_color.g, dbg_delimiter_color.b });
            if(_internal.draw_labels) _internal.texts.add({ dbg_v3f(delimiter.position), delimiter.dbg_name, dbg_label_color.r, dbg_label_color.g, dbg_label_color.b });
        }
	}

	if(options & DEBUG_DRAW_Clipping_Faces) {
		if(options & DEBUG_DRAW_Root_Planes) {
			for(Triangulated_Plane &plane : world->root_clipping_planes) {
                debug_draw_triangulated_plane(_internal, &plane, dbg_root_plane_color);
			}
		}
		
        for(s64 i = 0; i < world->delimiters.count; ++i) {
            Delimiter *delimiter = &world->delimiters[i];
            for(s64 j = 0; j < delimiter->plane_count; ++j) {
                Triangulated_Plane *plane = &delimiter->planes[j];
                debug_draw_triangulated_plane(_internal, plane, dbg_delimiter_plane_color);
            }
        }		
    }

	if(options & DEBUG_DRAW_Clipping_Wireframes) {
		if(options & DEBUG_DRAW_Root_Planes) {
			for(Triangulated_Plane &plane : world->root_clipping_planes) {
                debug_draw_triangulated_plane_wireframe(_internal, &plane, dbg_root_plane_color);
			}
		}
		
        for(s64 i = 0; i < world->delimiters.count; ++i) {
            Delimiter *delimiter = &world->delimiters[i];
            for(s64 j = 0; j < delimiter->plane_count; ++j) {
                Triangulated_Plane *plane = &delimiter->planes[j];
                debug_draw_triangulated_plane_wireframe(_internal, plane, dbg_delimiter_plane_color);
            }
        }		
	}

	if(options & DEBUG_DRAW_Volume_Faces) {
		for(s64 i = 0; i < world->anchors.count; ++i) {
            Anchor *anchor = &world->anchors[i];
			for(Triangle &triangle : anchor->volume) {
				Dbg_Draw_Color color = dbg_volume_color;
				debug_draw_triangle(_internal, &triangle, color);
			}
		}
	}

	if(options & DEBUG_DRAW_Volume_Wireframes) {
		for(s64 i = 0; i < world->anchors.count; ++i) {
            Anchor *anchor = &world->anchors[i];
			for(Triangle &triangle : anchor->volume) {
				Dbg_Draw_Color color = dbg_volume_color;
                f32 thickness = dbg_wireframe_thickness;
                debug_draw_triangle_wireframe(_internal, &triangle, color, thickness);
			}
		}
	}

    if(options & DEBUG_DRAW_Flood_Fill) {
        debug_draw_flood_fill(_internal, world->current_flood_fill);
    }
    
	if(options & DEBUG_DRAW_Axis_Gizmo) {
        debug_draw_line(_internal, vec3(0, 0, 0), vec3(3, 0, 0), dbg_axis_gizmo_thickness, Dbg_Draw_Color { 255, 0, 0 });
        debug_draw_line(_internal, vec3(0, 0, 0), vec3(0, 3, 0), dbg_axis_gizmo_thickness, Dbg_Draw_Color { 0, 255, 0 });
        debug_draw_line(_internal, vec3(0, 0, 0), vec3(0, 0, 3), dbg_axis_gizmo_thickness, Dbg_Draw_Color { 0, 0, 255 });
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
