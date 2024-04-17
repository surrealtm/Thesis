#pragma once

#include "foundation.h"
#include "memutils.h"
#include "strings.h"
#include "math/math.h"


#define TM_OCTREE_COLOR 0
#define TM_WORLD_COLOR  1
#define TM_SYSTEM_COLOR 2


/* ----------------------------------------------- 3D Geometry ----------------------------------------------- */

struct Triangle {
    v3f p0, p1, p2;
};

struct Clipping_Plane {
    v3f p; // The center of this plane.
    v3f n; // The unit normal of this plane, for intersection testing.
    v3f u; // The scaled u vector, indicating the size and orientation of the plane.
    v3f v; // The scaled v vector, indicating the size and orientation of the plane.
};

struct AABB {
    v3f min, max;
};

struct Volume {
    Resizable_Array<Triangle> triangles;
    AABB aabb;
};

struct Local_Axes {
    v3f _[3];

    v3f &operator[](s64 index) { assert(index >= 0 && index <= 3); return this->_[index]; }
};

AABB aabb_from_position_and_size(v3f center, v3f half_sizes);
Local_Axes local_axes_from_rotation(v3f euler_radians);



/* ------------------------------------------------- Objects ------------------------------------------------- */

struct Anchor {
    string name;
    v3f position;
    Volume volume; // This volume is essentially the output of the algorithm.
};

struct Boundary {
    string name;
    v3f position;
    v3f size;
    Local_Axes local_axes; // The three coordinate axis in the local transform (meaning: rotated) of this boundary.
    AABB aabb;
    Resizable_Array<Clipping_Plane> clipping_planes; // Having this be a full-on dynamic array is pretty wasteful, since boundaries should only ever have between 1 and 3 clipping planes...  @@Speed.
};



/* -------------------------------------------------- Octree -------------------------------------------------- */

#define OCTREE_DEPTH 8
#define MAX_OCTREE_DEPTH (OCTREE_DEPTH - 1)

enum Octree_Child_Index {
    OCTREE_CHILD_nx_ny_nz = 0,
    OCTREE_CHILD_nx_ny_pz = 1,
    OCTREE_CHILD_nx_py_nz = 2,
    OCTREE_CHILD_nx_py_pz = 3,
    OCTREE_CHILD_px_ny_nz = 4,
    OCTREE_CHILD_px_ny_pz = 5,
    OCTREE_CHILD_px_py_nz = 6,
    OCTREE_CHILD_px_py_pz = 7,
    OCTREE_CHILD_COUNT    = 8,

    OCTREE_CHILD_px_flag = 4,
    OCTREE_CHILD_py_flag = 2,
    OCTREE_CHILD_pz_flag = 1,

    OCTREE_CHILD_nx_flag = 0,
    OCTREE_CHILD_ny_flag = 0,
    OCTREE_CHILD_nz_flag = 0,
};

struct Octree {
    u8 depth;
    v3f center;
    v3f half_size;
    Octree *children[OCTREE_CHILD_COUNT];

    Resizable_Array<Anchor*> contained_anchors;
    Resizable_Array<Boundary*> contained_boundaries;

    void create(Allocator *allocator, v3f center, v3f half_size, u8 depth = 0);
    Octree *get_octree_for_aabb(AABB const &aabb, Allocator *allocator);
};



/* -------------------------------------------------- World -------------------------------------------------- */

struct World {
    // This world manages its own memory, so that allocations aren't fragmented and so that we can just destroy
    // the memory arena and everything is cleaned up.
    Memory_Arena arena;
    Memory_Pool pool;
    Allocator pool_allocator;
    Allocator *allocator; // Usually a pointer to the pool_allocator, but can be swapped for testing...
    
    v3f half_size; // This size is used to initialize the octree. It must be known in advance for the algorithm to be fast.

    // The world owns all objects that are part of this problem. These objects
    // are stored here and can then be referenced in other parts of the algorithm.
    //
    // We assume that these arrays get filled once at the start and then are never
    // modified, so that pointers to these objects are stable.
    Resizable_Array<Anchor> anchors;
    Resizable_Array<Boundary> boundaries;

    // The clipping planes of the octree, since no volume should ever go past the dimensions of the world.
    Resizable_Array<Clipping_Plane> root_clipping_planes;

    // This octree contains pointers to anchors, boundaries and volumes, to make spatial lookup
    // for objects a lot faster.
    Octree root;

    void create(v3f half_size);
    void destroy();

    void reserve_objects(s64 anchors, s64 boundaries);
    
    void add_anchor(string name, v3f position);
    Boundary *add_boundary(string name, v3f position, v3f size, Local_Axes local_axes);

    void add_boundary_clipping_plane(Boundary *boundary, u8 axis_index);
    void add_boundary_clipping_plane(Boundary *boundary, v3f n, v3f u, v3f v);

    f32 get_shortest_distance_to_root_clip(v3f position, v3f direction);
    
    void create_octree();
};
