#pragma once

#include "memutils.h"
#include "math/v3.h"

#include "typedefs.h"

struct BVH {
    Allocator *allocator;

    vec3 center;
    vec3 half_size;
    Resizable_Array<Triangle> triangles;

    void create(Allocator *allocator, vec3 center, vec3 half_size);
};
