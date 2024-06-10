#include "floodfill.h"
#include "core.h"

static
Cell *get_cell(Flood_Fill *ff, v3i position) {
    s64 index = position.x * ff->hy * ff->hz + position.y * ff->hz + position.z;
    return &ff->cells[index];
}

v3f get_cell_world_space_center(Flood_Fill *ff, s32 x, s32 y, s32 z) {
    f32 xoffset = ((f32) x - (f32) ff->hx / 2.f) * CELL_WORLD_SPACE_SIZE;
    f32 yoffset = ((f32) y - (f32) ff->hy / 2.f) * CELL_WORLD_SPACE_SIZE;
    f32 zoffset = ((f32) z - (f32) ff->hz / 2.f) * CELL_WORLD_SPACE_SIZE;

    if(ff->hx % 2 == 0) xoffset += CELL_WORLD_SPACE_SIZE / 2.f;
    if(ff->hy % 2 == 0) yoffset += CELL_WORLD_SPACE_SIZE / 2.f;
    if(ff->hz % 2 == 0) zoffset += CELL_WORLD_SPACE_SIZE / 2.f;

    return ff->world_space_center + v3f(xoffset, yoffset, zoffset);
}

Flood_Fill floodfill(World *world, Allocator *allocator) {
    Flood_Fill ff;
    ff.allocator = allocator;
    ff.hx        = (s32) ceil(world->half_size.x * CELLS_PER_WORLD_SPACE_UNIT * 2);
    ff.hy        = (s32) ceil(world->half_size.y * CELLS_PER_WORLD_SPACE_UNIT * 2);
    ff.hz        = (s32) ceil(world->half_size.z * CELLS_PER_WORLD_SPACE_UNIT * 2);
    ff.cells     = (Cell *) ff.allocator->allocate(ff.hx * ff.hy * ff.hz * sizeof(Cell));
    ff.world_space_center = v3f(0, 0, 0);

    memset(ff.cells, 0, ff.hx * ff.hy * ff.hz * sizeof(Cell));
    
    return ff;
}

void deallocate_flood_fill(Flood_Fill *ff) {
    ff->allocator->deallocate(ff->cells);
    ff->cells = null;
    ff->hx    = 0;
    ff->hy    = 0;
    ff->hz    = 0;
}
