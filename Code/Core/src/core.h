#pragma once

#include "foundation.h"
#include "memutils.h"
#include "strings.h"

#include "math/v2.h"
#include "math/v3.h"
#include "math/qt.h"



/* --------------------------------------------- Telemetry Colors --------------------------------------------- */

#define TM_SYSTEM_COLOR 0
#define TM_WORLD_COLOR  1
#define TM_OCTREE_COLOR 2
#define TM_ANCHOR_COLOR 3



/* --------------------------------------------- Type Definitions --------------------------------------------- */

//
// This algorithm is supposed to work with both single and double floating point precision, so that the usual
// speed / memory - accuracy tradeoff can be done by the user compiling this code. We use double precision
// by default.
//
#if CORE_SINGLE_PRECISION
typedef f32 real;
typedef v2<f32> vec2;
typedef v3<f32> vec3;
typedef qt<f32> quat;
constexpr real CORE_EPSILON = 0.001f; // :CORE_EPSILON    We have a higher floating point tolerance here than F32_EPSILON since we are talking about actual geometry units, where 1.f is roughly 1 meter semantically. Having this be smaller just leads to many different issues...
constexpr real CORE_SMALL_EPSILON = 0.00001f; // :CORE_SMALL_EPSILON    For some specific cases, we do want higher precision checks, if we know the range of values we are talking about etc...
#else
typedef f64 real;
typedef v2<f64> vec2;
typedef v3<f64> vec3;
typedef qt<f32> quat;
constexpr real CORE_EPSILON = 0.00001; // :CORE_EPSILON
constexpr real CORE_SMALL_EPSILON = 0.0000001; // :CORE_SMALL_EPSILON
#endif



/* ----------------------------------------------- 3D Geometry ----------------------------------------------- */

enum Axis {
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2,
    AXIS_COUNT = 3,
};

struct Triangle {
    v3f p0, p1, p2;
    v3f n;
    
    Triangle() {};
    Triangle(v3f p0, v3f p1, v3f p2, v3f n);
    
    f32 approximate_surface_area(); // This avoids a square root for performance, since we only roughly want to know whether the triangle is dead or not.
    b8 is_dead();
    b8 is_fully_behind_plane(Triangle *plane);
    b8 no_point_behind_plane(Triangle *plane); // Just for assertions.
};

struct AABB {
    v3f min, max;
};



/* ------------------------------------------------- Objects ------------------------------------------------- */

struct Anchor {
    v3f position;
    Resizable_Array<Triangle> volume_triangles;

    // Only for debug drawing.
    string dbg_name;
};

struct Boundary {
    v3f position;
    AABB aabb;
    v3f local_scaled_axes[AXIS_COUNT]; // The three coordinate axis in the local transform (meaning: rotated) of this boundary.
    v3f local_unit_axes[AXIS_COUNT]; // The three coordinate axis in the local transform (meaning: rotated) of this boundary.

    Resizable_Array<Triangle> clipping_triangles; // Having this be a full-on dynamic array is pretty wasteful, since boundaries should only ever have between 1 and 3 clipping planes...  @@Speed.

    // Only for debug drawing.
    string dbg_name;
    v3f dbg_half_size; // Unrotated half sizes.
    qtf dbg_rotation;
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
    // @@Speed: In some cases it might be even better to use the arena allocator directly, if we know something
    // is never gonna get freed, so that we don't have the time nor space overhead of doing memory blocks.
    Memory_Arena arena;
    Memory_Pool pool;
    Allocator pool_allocator;
    Allocator *allocator; // Usually a pointer to the pool_allocator, but can be swapped for testing...
    
    v3f half_size; // This size is used to initialize the octree. The octree implementation does not support dynamic size changing, so this should be fixed.

    // The world owns all objects that are part of this problem. These objects
    // are stored here and can then be referenced in other parts of the algorithm.
    //
    // We assume that these arrays get filled once at the start and then are never
    // modified, so that pointers to these objects are stable.
    Resizable_Array<Anchor> anchors;
    Resizable_Array<Boundary> boundaries;

    // The clipping planes of the octree, since no volume should ever go past the dimensions of the world.
    Resizable_Array<Triangle> root_clipping_triangles;

    // This octree contains pointers to anchors, boundaries and volumes, to make spatial lookup
    // for objects a lot faster.
    Octree root;

    void create(v3f half_size);
    void destroy();

    void reserve_objects(s64 anchors, s64 boundaries);
    
    Anchor *add_anchor(string name, v3f position);
    Boundary *add_boundary(string name, v3f position, v3f size, v3f rotation);
    void add_boundary_clipping_planes(Boundary *boundary, Axis normal_axis);
    void add_centered_boundary_clipping_plane(Boundary *boundary, Axis normal_axis);
    
    void create_octree();
    void clip_boundaries(b8 single_step);

    /* DbgStep */

    b8 dbg_step_initialized;
    b8 dbg_step_completed;
    b8 dbg_step_clipping_triangle_should_be_removed;
    s64 dbg_step_boundary;
    s64 dbg_step_clipping_triangle;
    s64 dbg_step_root_triangle;
};
