#pragma once

#include "memutils.h"
#include "math/v3.h"

#include "typedefs.h"

#define BVH_DEGREE 2

struct BVH {
    Allocator *allocator;

    vec3 center;
    vec3 half_size;
    Resizable_Array<Triangle> triangles;

    BVH *children[BVH_DEGREE];
    
    void create(Allocator *allocator, vec3 center, vec3 half_size);
};

BVH *make_bvh(Allocator *allocator, vec3 center, vec3 half_size);
