#pragma once

#include "memutils.h"
#include "typedefs.h"

struct World;
struct Flood_Fill;

Resizable_Array<Triangle> assemble(World *world, Flood_Fill *ff, Allocator *allocator);
