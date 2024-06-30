#pragma once

#include "memutils.h"
#include "math/v3.h"

#include "typedefs.h"

struct BVH_Entry {
    // User-supplied input
    Triangle triangle;

    // Internal processing
    vec3 center;
};

struct BVH_Node {
    vec3 min, max;
    s64 first_entry_index, entry_count;
    BVH_Node *children[2];
};

struct BVH {
    Allocator *allocator;

    BVH_Node root;
    Resizable_Array<BVH_Entry> entries;
    
    void create(Allocator *allocator);
    void add(Triangle triangle);
    void subdivide();
};
