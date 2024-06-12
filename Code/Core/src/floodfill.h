#pragma once

#include "foundation.h"
#include "math/v3.h"
#include "memutils.h"

#define CELL_WORLD_SPACE_SIZE 1.f
#define CELLS_PER_WORLD_SPACE_UNIT (1.f / CELL_WORLD_SPACE_SIZE)

struct World;
struct Allocator;

enum Cell_State {
    CELL_Untouched             = 0x0,
    CELL_Currently_In_Frontier = 0x1,
    CELL_Has_Been_Flooded      = 0x2,
};

struct Cell {
    v3i position; // So that we can just store pointers to Cells in the frontier, and don't have to also remember the position in the frontier...   Note: This will only be filled when it is first added to the frontier!
    Cell_State state;
};

struct Flood_Fill {
    Allocator *allocator;
    Resizable_Array<Cell*> frontier;

    s64 hx, hy, hz; // Dimensions in cells
    v3i origin; // The first cell that was flooded (in cell coordinates)
    v3i grid_center; // hx / 2, hy / 2, hz / 2
    v3f world_space_center; // World Space Center of the cell grid.
    Cell *cells;
};

Cell *get_cell(Flood_Fill *ff, v3i position);
v3f get_cell_world_space_center(Flood_Fill *ff, s32 x, s32 y, s32 z);
Flood_Fill floodfill(World *world, Allocator *allocator);
void deallocate_flood_fill(Flood_Fill *ff);
