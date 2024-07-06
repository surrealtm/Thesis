#pragma once

#include "memutils.h"
#include "typedefs.h"

struct World;
struct Flood_Fill;

void assemble(Resizable_Array<Triangle> *volume, World *world, Flood_Fill *ff);
