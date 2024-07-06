#pragma once

#include "foundation.h"
#include "math/v3.h"
#include "memutils.h"

#include "typedefs.h"

#define CELL_WORLD_SPACE_SIZE      2. // @Incomplete: Make this a dynamic variable, so that the user can define it when creating the world.
#define CELLS_PER_WORLD_SPACE_UNIT (1. / CELL_WORLD_SPACE_SIZE)

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
    
    s64 hx, hy, hz; // Dimensions in cells
    v3i origin; // The first cell that was flooded (in cell coordinates)
    v3i grid_center; // hx / 2, hy / 2, hz / 2
    vec3 world_space_center; // World Space Center of the cell grid.
    Cell *cells;
};

Cell *get_cell(Flood_Fill *ff, v3i position);
vec3 get_cell_world_space_center(Flood_Fill *ff, v3i position);
vec3 get_cell_world_space_center(Flood_Fill *ff, Cell *cell);
void floodfill(Flood_Fill *ff, World *world, Allocator *allocator, vec3 world_space_center);
void deallocate_flood_fill(Flood_Fill *ff);
