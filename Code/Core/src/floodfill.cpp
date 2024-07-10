#include "floodfill.h"
#include "world.h"

#include "timing.h"


/* ---------------------------------------------- Implementation ---------------------------------------------- */

static inline
v3i world_space_to_cell_space(Flood_Fill *ff, vec3 world_space) {
    vec3 relative_to_center = world_space - ff->world_space_center;
    vec3 scaled_relative    = relative_to_center * vec3(CELLS_PER_WORLD_SPACE_UNIT);
    return v3i((s32) clamp(round(scaled_relative.x + ff->hx / 2. - 1), 0, ff->hx - 1),
               (s32) clamp(round(scaled_relative.y + ff->hy / 2. - 1), 0, ff->hy - 1),
               (s32) clamp(round(scaled_relative.z + ff->hz / 2. - 1), 0, ff->hz - 1));
}

static inline
b8 flood_fill_condition(Flood_Fill *ff, Cell *dst, Cell *src) {
    vec3 world_space_origin    = get_cell_world_space_center(ff, src);
    vec3 world_space_direction = get_cell_world_space_center(ff, dst) - get_cell_world_space_center(ff, src);
    
    return !ff->world->cast_ray_against_delimiters_and_root_planes(world_space_origin, world_space_direction, 1.f); // World space direction is scaled to reflect the actual distance between the cells, so we only care about intersections on inside this direction vector.
}

static inline
void definitely_add_cell_to_frontier(Flood_Fill *ff, v3i position) {
    Cell *cell = get_cell(ff, position);
    cell->position = position;
    cell->state = CELL_Currently_In_Frontier;
    cell->added_from_cell = position;
    ff->frontier.add(cell);
}

static inline
void maybe_add_cell_to_frontier(Flood_Fill *ff, Cell *src, v3i position) {
    if(position.x < 0 || position.x >= ff->hx || position.y < 0 || position.y >= ff->hy || position.z < 0 || position.z >= ff->hz) return;

    Cell *cell = get_cell(ff, position);
    if(cell->state != CELL_Untouched) return;
    cell->position        = position;
    cell->added_from_cell = src->position;
    
    if(!flood_fill_condition(ff, cell, src)) return;

    cell->state = CELL_Currently_In_Frontier;
    ff->frontier.add(cell);
}

static inline
void fill_cell(Flood_Fill *ff, Cell *cell) {
    cell->state = CELL_Has_Been_Flooded;
    ff->flooded_cells.add(cell);

    maybe_add_cell_to_frontier(ff, cell, cell->position + v3i(1, 0, 0));
    maybe_add_cell_to_frontier(ff, cell, cell->position - v3i(1, 0, 0));
    maybe_add_cell_to_frontier(ff, cell, cell->position + v3i(0, 1, 0));
    maybe_add_cell_to_frontier(ff, cell, cell->position - v3i(0, 1, 0));
    maybe_add_cell_to_frontier(ff, cell, cell->position + v3i(0, 0, 1));
    maybe_add_cell_to_frontier(ff, cell, cell->position - v3i(0, 0, 1));
}



/* --------------------------------------------------- Api --------------------------------------------------- */

static
s32 ceil_to_uneven(real value) {
    s32 result = (s32) ceil(value);
    result = (result % 1 == 0) ? result + 1 : result;
    return result;
}

Cell *get_cell(Flood_Fill *ff, v3i position) {
    if(position.x < 0 || position.x >= ff->hx || position.y < 0 || position.y >= ff->hy || position.z < 0 || position.z >= ff->hz) return null;
    
    s64 index = position.x * ff->hy * ff->hz + position.y * ff->hz + position.z;
    return &ff->cells[index];
}

vec3 get_cell_world_space_center(Flood_Fill *ff, v3i position) {
    real xoffset = position.x * CELL_WORLD_SPACE_SIZE - ff->origin.x * CELL_WORLD_SPACE_SIZE;
    real yoffset = position.y * CELL_WORLD_SPACE_SIZE - ff->origin.y * CELL_WORLD_SPACE_SIZE;
    real zoffset = position.z * CELL_WORLD_SPACE_SIZE - ff->origin.z * CELL_WORLD_SPACE_SIZE;

    return ff->world_space_center + vec3(xoffset, yoffset, zoffset);
}

vec3 get_cell_world_space_center(Flood_Fill *ff, Cell *cell) {
    return get_cell_world_space_center(ff, cell->position);
}

void floodfill(Flood_Fill *ff, World *world, Allocator *allocator, vec3 world_space_center) {
    tmFunction(TM_FLOODING_COLOR);

    ff->allocator = allocator;

    // Make sure that we have an uneven number of cells, so that the origin cell is actually centered on the
    // world space center (with an even number of cells, an edge between two cells would be centered on the
    // world space center)
    ff->hx        = ceil_to_uneven(world->half_size.x * CELLS_PER_WORLD_SPACE_UNIT * 2);
    ff->hy        = ceil_to_uneven(world->half_size.y * CELLS_PER_WORLD_SPACE_UNIT * 2);
    ff->hz        = ceil_to_uneven(world->half_size.z * CELLS_PER_WORLD_SPACE_UNIT * 2);

    ff->cells     = (Cell *) ff->allocator->allocate(ff->hx * ff->hy * ff->hz * sizeof(Cell));
    ff->world_space_center = world_space_center; // The world is centered around (0, 0, 0) by default.
    ff->frontier.allocator = allocator;
    ff->world = world;
    
    memset(ff->cells, 0, ff->hx * ff->hy * ff->hz * sizeof(Cell)); // @@Speed: The allocator should guarantee zero-initialization anyway, so this seems unncessary. We probably want to reuse a Flood_Fill struct later though, so at that point it might become necessary again...

    ff->origin = world_space_to_cell_space(ff, world_space_center);
    definitely_add_cell_to_frontier(ff, ff->origin);
    
    while(ff->frontier.count) {
        Cell *head = ff->frontier.pop_first(); // @@Speed: Pop last, that should be much more efficient. We don't care about order here, so that should not be a problem.
        fill_cell(ff, head);
    }
}

void deallocate_flood_fill(Flood_Fill *ff) {
    ff->allocator->deallocate(ff->cells);
    ff->flooded_cells.clear();
    ff->frontier.clear();
    ff->cells = null;
    ff->hx    = 0;
    ff->hy    = 0;
    ff->hz    = 0;
}
