#pragma once

#include "foundation.h"
#include "memutils.h"
#include "string_type.h"

#include "math/v2.h"
#include "math/v3.h"
#include "math/qt.h"

#include "typedefs.h"



/* --------------------------------------------- Telemetry Colors --------------------------------------------- */

#define TM_SYSTEM_COLOR 0
#define TM_WORLD_COLOR  1
#define TM_OCTREE_COLOR 2
#define TM_ANCHOR_COLOR 3




/* ----------------------------------------------- 3D Geometry ----------------------------------------------- */

enum Axis {
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2,
    AXIS_COUNT = 3,
};

enum Virtual_Extension {
    VIRTUAL_EXTENSION_None       = 0x0,
    VIRTUAL_EXTENSION_Positive_U = 0x1,
    VIRTUAL_EXTENSION_Negative_U = 0x2,
    VIRTUAL_EXTENSION_Positive_V = 0x4,
    VIRTUAL_EXTENSION_Negative_V = 0x8,
    VIRTUAL_EXTENSION_All        = VIRTUAL_EXTENSION_Positive_U | VIRTUAL_EXTENSION_Negative_U | VIRTUAL_EXTENSION_Positive_V | VIRTUAL_EXTENSION_Negative_V,
};

BITWISE(Virtual_Extension);

struct Triangle {
    vec3 p0, p1, p2;
    vec3 n;
    
    Triangle() {};
    Triangle(vec3 p0, vec3 p1, vec3 p2);
    Triangle(vec3 p0, vec3 p1, vec3 p2, vec3 n);
    
    real approximate_surface_area(); // This avoids a square root for performance, since we only roughly want to know whether the triangle is dead or not.
    b8 is_dead();
    b8 all_points_in_front_of_plane(Triangle *plane);
    b8 no_point_behind_plane(Triangle *plane);
};

struct Triangulated_Plane {
    Resizable_Array<Triangle> triangles; // @@Speed: We store the normal in each triangle, while they should all have the same normal. We should probably just store the vertices of the triangles in the list and then the normal once. We might even be able to go the index buffer route, but not sure if that's actually worth it.

    void setup(Allocator *allocator, vec3 c, vec3 n, vec3 left, vec3 right, vec3 top, vec3 bottom);
};

struct AABB {
    vec3 min, max;
};



/* ------------------------------------------------- Objects ------------------------------------------------- */

struct Anchor {
    vec3 position;
    Resizable_Array<Triangle> triangles;

    // Only for debug drawing.
    string dbg_name;
};

struct Delimiter {
    vec3 position;
    vec3 local_scaled_axes[AXIS_COUNT]; // The three coordinate axis in the local transform (meaning: rotated) of this delimiter.
    vec3 local_unit_axes[AXIS_COUNT]; // The three coordinate axis in the local transform (meaning: rotated) of this delimiter.
    AABB aabb;

    u8 level;
    Triangulated_Plane planes[6]; // A delimiter can have at most 6 planes (two planes on each axis).
    s64 plane_count;

    // Only for debug drawing.
    string dbg_name;
    vec3 dbg_half_size; // Unrotated half sizes.
    quat dbg_rotation;
};

struct Delimiter_Intersection {
    real total_distance; // The (squared) distance from this point to d0 + The (squared) distance from this point to d1.
    Delimiter *d0, *d1;
    Triangulated_Plane *p0, *p1;
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
    vec3 center;
    vec3 half_size;
    Octree *children[OCTREE_CHILD_COUNT];

    Resizable_Array<Anchor *>    contained_anchors;
    Resizable_Array<Delimiter *> contained_delimiters;

    void create(Allocator *allocator, vec3 center, vec3 half_size, u8 depth = 0);
    Octree *get_octree_for_aabb(AABB const &aabb, Allocator *allocator);
};



/* -------------------------------------------------- World -------------------------------------------------- */

struct World {
    // This world manages its own memory, so that allocations aren't fragmented and so that we can just destroy
    // the memory arena and everything is cleaned up.
    // @@Speed: In some cases it might be even better to use the arena allocator directly, if we know something
    // is never gonna get freed, so that we don't have the time nor space overhead of doing memory blocks.
    Memory_Arena arena;
    Memory_Pool pool;
    Allocator pool_allocator;
    Allocator *allocator; // Usually a pointer to the pool_allocator, but can be swapped for testing...
    
    vec3 half_size; // This size is used to initialize the octree. The octree implementation does not support dynamic size changing, so this should be fixed.

    // The world owns all objects that are part of this problem. These objects
    // are stored here and can then be referenced in other parts of the algorithm.
    //
    // We assume that these arrays get filled once at the start and then are never
    // modified, so that pointers to these objects are stable.
    Resizable_Array<Anchor> anchors;
    Resizable_Array<Delimiter> delimiters;

    // The clipping planes of the octree, since no volume should ever go past the dimensions of the world.
    Resizable_Array<Triangle> root_clipping_triangles;

    // This octree contains pointers to anchors, delimiters and volumes, to make spatial lookup
    // for objects a lot faster.
    Octree root;
    
    struct Flood_Fill *current_flood_fill; // Just for debug drawing.
    
    void create(vec3 half_size);
    void destroy();

    void reserve_objects(s64 anchors, s64 delimiters);
    
    Anchor *add_anchor(vec3 position);
    Anchor *add_anchor(string dbg_name, vec3 position);
    Delimiter *add_delimiter(vec3 position, vec3 size, vec3 rotation, u8 level);
    Delimiter *add_delimiter(string dbg_name, vec3 position, vec3 size, vec3 rotation, u8 level);
    void add_delimiter_clipping_planes(Delimiter *delimiter, Axis normal_axis, Virtual_Extension virtual_extension = VIRTUAL_EXTENSION_All);
    void add_centered_delimiter_clipping_plane(Delimiter *delimiter, Axis normal_axis, Virtual_Extension virtual_extension = VIRTUAL_EXTENSION_All);
    
    void create_octree();
    void clip_delimiters(b8 single_step);
    void calculate_volumes();

    b8 cast_ray_against_delimiters_and_root_planes(vec3 origin, vec3 direction, real distance);
};



/* ---------------------------------------------- Random Utility ---------------------------------------------- */

real get_random_real_uniform(real low, real high);
