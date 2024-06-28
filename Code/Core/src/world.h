#pragma once

#include "foundation.h"
#include "memutils.h"
#include "string_type.h"

#include "math/v2.h"
#include "math/v3.h"
#include "math/qt.h"

#include "typedefs.h"



/* --------------------------------------------- Telemetry Colors --------------------------------------------- */

#define TM_SYSTEM_COLOR   0
#define TM_WORLD_COLOR    1
#define TM_BVH_COLOR      2
#define TM_TESSEL_COLOR   3
#define TM_FLOODING_COLOR 4
#define TM_MARCHING_COLOR 5



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

    u8 level;
    Triangulated_Plane planes[6]; // A delimiter can have at most 6 planes (two planes on each axis).
    s64 plane_count;

    // Only for debug drawing.
    string dbg_name;
    vec3 dbg_half_size; // Unrotated half sizes.
    quat dbg_rotation;
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
    
    vec3 half_size; // This size is used to initialize the bvh. The bvh implementation does not support dynamic size changing, so this should be fixed.

    // The world owns all objects that are part of this problem. These objects
    // are stored here and can then be referenced in other parts of the algorithm.
    //
    // We assume that these arrays get filled once at the start and then are never
    // modified, so that pointers to these objects are stable.
    Resizable_Array<Anchor> anchors;
    Resizable_Array<Delimiter> delimiters;

    // The clipping planes around the entire world (indicated by the world size), since no volume or delimiter plane should ever go past the dimensions of the world.
    Resizable_Array<Triangle> root_clipping_triangles;
    
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
    
    void create_bvh();
    void clip_delimiters(b8 single_step);
    void calculate_volumes();

    b8 cast_ray_against_delimiters_and_root_planes(vec3 origin, vec3 direction, real distance);
};



/* ---------------------------------------------- Random Utility ---------------------------------------------- */

real get_random_real_uniform(real low, real high);
