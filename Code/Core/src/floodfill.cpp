#include "floodfill.h"
#include "core.h"



/* ---------------------------------------------- Implementation ---------------------------------------------- */

static inline
vec3 get_cell_world_space_center(Flood_Fill *ff, Cell *cell) {
    return get_cell_world_space_center(ff, cell->position.x, cell->position.y, cell->position.z);
}

static inline
b8 flood_fill_condition(Flood_Fill *ff, Cell *dst, Cell *src) {
    vec3 world_space_origin    = get_cell_world_space_center(ff, src);
    vec3 world_space_direction = get_cell_world_space_center(ff, dst) - get_cell_world_space_center(ff, src);
    
    return !ff->world->cast_ray_against_delimiters(world_space_origin, world_space_direction, 1.f);
}

static inline
void definitely_add_cell_to_frontier(Flood_Fill *ff, v3i position) {
    Cell *cell = get_cell(ff, position);
    cell->position = position;
    cell->state = CELL_Currently_In_Frontier;
    ff->frontier.add(cell);
}

static inline
void maybe_add_cell_to_frontier(Flood_Fill *ff, Cell *src, v3i position) {
    if(position.x < 0 || position.x >= ff->hx || position.y < 0 || position.y >= ff->hy || position.z < 0 || position.z >= ff->hz) return;

    Cell *cell = get_cell(ff, position);
    if(cell->state != CELL_Untouched) return;
    cell->position = position;

    if(!flood_fill_condition(ff, cell, src)) return;

    cell->state = CELL_Currently_In_Frontier;
    ff->frontier.add(cell);
}

static inline
void fill_cell(Flood_Fill *ff, Cell *cell) {
    cell->state = CELL_Has_Been_Flooded;

    maybe_add_cell_to_frontier(ff, cell, cell->position + v3i(1, 0, 0));
    maybe_add_cell_to_frontier(ff, cell, cell->position - v3i(1, 0, 0));
    maybe_add_cell_to_frontier(ff, cell, cell->position + v3i(0, 1, 0));
    maybe_add_cell_to_frontier(ff, cell, cell->position - v3i(0, 1, 0));
    maybe_add_cell_to_frontier(ff, cell, cell->position + v3i(0, 0, 1));
    maybe_add_cell_to_frontier(ff, cell, cell->position - v3i(0, 0, 1));
}



/* --------------------------------------------------- Api --------------------------------------------------- */

Cell *get_cell(Flood_Fill *ff, v3i position) {
    s64 index = position.x * ff->hy * ff->hz + position.y * ff->hz + position.z;
    return &ff->cells[index];
}

vec3 get_cell_world_space_center(Flood_Fill *ff, s32 x, s32 y, s32 z) {
    real xoffset = ((real) x - (real) ff->hx / 2.) * CELL_WORLD_SPACE_SIZE;
    real yoffset = ((real) y - (real) ff->hy / 2.) * CELL_WORLD_SPACE_SIZE;
    real zoffset = ((real) z - (real) ff->hz / 2.) * CELL_WORLD_SPACE_SIZE;

    if(ff->hx % 2 == 0) xoffset += CELL_WORLD_SPACE_SIZE / 2.f;
    if(ff->hy % 2 == 0) yoffset += CELL_WORLD_SPACE_SIZE / 2.f;
    if(ff->hz % 2 == 0) zoffset += CELL_WORLD_SPACE_SIZE / 2.f;

    return ff->world_space_center + vec3(xoffset, yoffset, zoffset);
}

Flood_Fill floodfill(World *world, Allocator *allocator, vec3 world_space_center) {
    Flood_Fill ff;
    ff.allocator = allocator;
    ff.hx        = (s32) ceil(world->half_size.x * CELLS_PER_WORLD_SPACE_UNIT * 2);
    ff.hy        = (s32) ceil(world->half_size.y * CELLS_PER_WORLD_SPACE_UNIT * 2);
    ff.hz        = (s32) ceil(world->half_size.z * CELLS_PER_WORLD_SPACE_UNIT * 2);
    ff.cells     = (Cell *) ff.allocator->allocate(ff.hx * ff.hy * ff.hz * sizeof(Cell));
    ff.world_space_center = world_space_center;
    ff.frontier.allocator = allocator;
    ff.world = world;
    
    memset(ff.cells, 0, ff.hx * ff.hy * ff.hz * sizeof(Cell));

    ff.origin = v3i((s32) (ff.hx / 2), (s32) (ff.hy / 2), (s32) (ff.hz / 2));
    definitely_add_cell_to_frontier(&ff, ff.origin);

    while(ff.frontier.count) {
        Cell *head = ff.frontier.pop_first(); // @@Speed: Pop last, that should be much more efficient. We don't care about order here, so that should not be a problem.
        fill_cell(&ff, head);
    }
    
    return ff;
}

void deallocate_flood_fill(Flood_Fill *ff) {
    ff->allocator->deallocate(ff->cells);
    ff->frontier.clear();
    ff->cells = null;
    ff->hx    = 0;
    ff->hy    = 0;
    ff->hz    = 0;
}
