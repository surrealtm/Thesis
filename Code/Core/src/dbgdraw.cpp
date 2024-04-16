#include "dbgdraw.h"
#include "core.h"

#include "memutils.h"

struct Dbg_Internal_Draw_Data {
	Resizable_Array<Debug_Draw_Line> lines;
	Resizable_Array<Debug_Draw_Text> texts;
	Resizable_Array<Debug_Draw_Cuboid> cuboids;
};

struct Dbg_Draw_Color {
	u8 r, g, b;
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

Debug_Draw_Data debug_draw_world(World *world, Debug_Draw_Options options) {
	Dbg_Internal_Draw_Data _internal;
	_internal.lines.allocator = dbg_alloc;
	_internal.texts.allocator = dbg_alloc;
	_internal.cuboids.allocator = dbg_alloc;

	if(options & DEBUG_DRAW_Octree) {
		debug_draw_octree(_internal, &world->root, OCTREE_CHILD_COUNT, 0);
	}

	if(options & DEBUG_DRAW_Anchors) {
		// @Incomplete
	}

	if(options & DEBUG_DRAW_Boundaries) {
		// @Incomplete
	}

	Debug_Draw_Data data = { 0 };
	data.lines        = _internal.lines.data;
	data.line_count   = _internal.lines.count;
	data.texts        = _internal.texts.data;
	data.text_count   = _internal.texts.count;
	data.cuboids      = _internal.cuboids.data;
	data.cuboid_count = _internal.cuboids.count;
	return data;
}

void free_debug_draw_data(Debug_Draw_Data *data) {
	dbg_alloc->deallocate(data->lines);
	data->line_count = 0;

	dbg_alloc->deallocate(data->texts);
	data->text_count = 0;

	dbg_alloc->deallocate(data->cuboids);
	data->cuboid_count = 0;
}