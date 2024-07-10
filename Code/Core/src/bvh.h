#pragma once

#include "memutils.h"
#include "math/v3.h"

#include "typedefs.h"

#define MAX_BVH_DEPTH             9
#define MIN_BVH_ENTRIES_TO_SPLIT  4

//
// https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/
//

struct BVH_Stats {
    s64 max_leaf_depth;
    s64 min_leaf_depth;
    s64 min_entries_in_leaf;
    s64 max_entries_in_leaf;
    s64 total_node_count;
    s64 total_entry_count;

    // @Incomplete: Maybe figure out a way to represent the "shrinking" volume between the root node and all the
    // leaf nodes. Essentially, the bigger this shrinkage, the better the BVH I think. (At least some metric).
    // Right now, for the center_block test, this shrinkage is actually 0 (and therefore, the additional
    // computational overhead is large).
    
    void print_to_stdout();
};

struct BVH_Entry {
    // User-supplied input
    Triangle triangle;

    // Internal processing
    vec3 center;
};

struct BVH_Cast_Result {
    b8 hit_something;
    real hit_distance;
    Triangle *hit_triangle;
};

struct BVH_Node {
    vec3 min, max;
    s64 first_entry_index, entry_count;
    BVH_Node *children[2];
    b8 leaf;

    void update_stats(BVH_Stats *stats, s64 depth);
};

struct BVH {
    Allocator *allocator;

    BVH_Node root;

    // @@Speed: It might be better to not index into this array directory in the bvh nodes, but instead have
    // another level of indirection, so that the nodes have a continuous slice in this indirection array, but
    // we don't shuffle the actual entries around (which might be slow...)
    Resizable_Array<BVH_Entry> entries;
    
    void create(Allocator *allocator);
    void add(Triangle triangle);
    void subdivide();

    BVH_Cast_Result cast_ray(vec3 origin, vec3 direction, real max_distance, b8 early_return = false);
    
    Resizable_Array<BVH_Node *> find_leafs_at_position(Allocator *allocator, vec3 position);

    BVH_Stats stats();
};


Resizable_Array<Triangle> build_sample_triangle_mesh(Allocator *allocator); // @@Ship
