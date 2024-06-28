#pragma once

#include "memutils.h"
#include "world.h"

struct Flood_Fill;

void marching_cubes(Resizable_Array<Triangle> *output, Flood_Fill *ff);
