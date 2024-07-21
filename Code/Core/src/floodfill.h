#pragma once

#include "foundation.h"
#include "math/v3.h"
#include "memutils.h"

#include "typedefs.h"

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

    v3i added_from_cell; // @@Ship: Remove this.
};

struct Flood_Fill {
    Allocator *allocator;
    World *world;

    Resizable_Array<Cell *> frontier;
    Resizable_Array<Cell *> flooded_cells; // So that we can quickly iterate over all flooded cells in the assembler.
    
    s32 hx, hy, hz; // Dimensions in cells
    real cell_world_space_size; // In world space
    vec3 cell_to_world_space_transform;
    vec3 world_to_cell_space_transform;
    v3i origin; // The first cell that was flooded (in cell coordinates)

    Cell *cells;
};

Cell *get_cell(Flood_Fill *ff, v3i position);
vec3 get_cell_world_space_center(Flood_Fill *ff, v3i position);
vec3 get_cell_world_space_center(Flood_Fill *ff, Cell *cell);
void floodfill(Flood_Fill *ff, World *world, Allocator *allocator, vec3 world_space_center, real cell_world_space_size);
void deallocate_flood_fill(Flood_Fill *ff);
