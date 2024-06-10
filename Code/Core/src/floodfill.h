#pragma once

#include "foundation.h"
#include "math/v3.h"

#define CELL_WORLD_SPACE_SIZE 1.f
#define CELLS_PER_WORLD_SPACE_UNIT (1.f / CELL_WORLD_SPACE_SIZE)

struct World;
struct Allocator;

enum Cell_State {
    CELL_Untouched          = 0x0,
    CELL_Has_Been_Flooded   = 0x1,
};

struct Cell {
    Cell_State state;
};

struct Flood_Fill {
    Allocator *allocator;
    s64 hx, hy, hz; // Dimensions in cells
    v3f world_space_center; // Center of the cell grid.
    Cell *cells;
};

v3f get_cell_world_space_center(Flood_Fill *ff, s32 x, s32 y, s32 z);
Flood_Fill floodfill(World *world, Allocator *allocator);
void deallocate_flood_fill(Flood_Fill *ff);
