#include "foundation.h"
#include "memutils.h"
#include "strings.h"
#include "math/math.h"

struct Triangle {
    v3f p0, p1, p2;
};

struct AABB {
    v3f min, max;
};

struct Volume {
    Resizable_Array<Triangle> triangles;
    AABB aabb;
};



struct Anchor {
    string name;
    v3f position;
    Volume volume; // This volume is essentially the output of the algorithm.
};

struct Boundary {
    string name;
    AABB aabb;
};



struct Octree {
    v3f center;
    v3f dimensions;
    Octree *children[8];

    Resizable_Array<Anchor*> anchors;
    Resizable_Array<Boundary*> boundaries;

    void create(Allocator *allocator, v3f center, v3f dimensions);
    Octree *get_octree_for_aabb(AABB const &aabb, Allocator *allocator);
};

struct World {
    // This world manages its own memory, so that allocations aren't fragmented and so that we can just destroy
    // the memory arena and everything is cleaned up.
    Memory_Arena arena;
    Memory_Pool pool;
    Allocator allocator;
    
    v3f size; // This size is used to initialize the octree. It must be known in advance for the algorithm to be fast.

    // The world owns all objects that are part of this problem. These objects
    // are stored here and can then be referenced in other parts of the algorithm.
    //
    // We assume that these arrays get filled once at the start and then are never
    // modified, so that pointers to these objects are stable.
    Resizable_Array<Anchor> anchors;
    Resizable_Array<Boundary> boundaries;

    // This octree contains pointers to anchors, boundaries and volumes, to make spatial lookup
    // for objects a lot faster.
    Octree root;

    void create(v3f size);
    void destroy();

    void add_anchor(string name, v3f position);
    void add_boundary(string name, AABB aabb);

    void create_octree();
};
