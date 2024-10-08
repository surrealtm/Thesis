#pragma once

#include "foundation.h"
#include "memutils.h"
#include "string_type.h"
#include "jobs.h"

#include "math/v2.h"
#include "math/v3.h"
#include "math/qt.h"

#include "typedefs.h"
#include "bvh.h"



/* ------------------------------------------------- Objects ------------------------------------------------- */

struct Anchor {
    s64 id; // Just the index into the world's anchor array.
    vec3 position;
    Resizable_Array<Triangle> volume;

    // Only for debug drawing.
    string dbg_name;
};

struct Delimiter {
    s64 id;
    u8 level;
    vec3 position;
    vec3 local_scaled_axes[AXIS_COUNT]; // The three coordinate axis in the local transform (meaning: rotated) of this delimiter.
    vec3 local_unit_axes[AXIS_COUNT]; // The three coordinate axis in the local transform (meaning: rotated) of this delimiter.

    Triangulated_Plane planes[6]; // A delimiter can have at most 6 planes (two planes on each axis).
    s64 plane_count;

    // @@Ship: Only for debug drawing.
    string dbg_name;
    vec3 dbg_half_size; // Unrotated half sizes.
    quat dbg_rotation;
};



/* -------------------------------------------------- World -------------------------------------------------- */

struct World {
    // --- World API
    void create(vec3 half_size);
    void destroy();

    void reserve_objects(s64 anchors, s64 delimiters);
    
    Anchor *add_anchor(vec3 position);
    Anchor *add_anchor(string dbg_name, vec3 position);
    Delimiter *add_delimiter(vec3 position, vec3 size, vec3 rotation, u8 level);
    Delimiter *add_delimiter(vec3 position, vec3 size, quat rotation, u8 level);
    Delimiter *add_delimiter(string dbg_name, vec3 position, vec3 size, vec3 rotation, u8 level);
    Delimiter *add_delimiter(string dbg_name, vec3 position, vec3 size, quat rotation, u8 level);
    void add_delimiter_plane(Delimiter *delimiter, Axis_Index normal_axis, b8 centered = false, Virtual_Extension virtual_extension = VIRTUAL_EXTENSION_All);
    void add_both_delimiter_planes(Delimiter *delimiter, Axis_Index normal_axis, Virtual_Extension virtual_extension = VIRTUAL_EXTENSION_All);
    void calculate_volumes(real cell_world_space_size = 10.);
    Anchor *query(vec3 point);
    


    // --- Internal data structures

    // This world manages its own memory, so that allocations aren't fragmented and so that we can just destroy
    // the memory arena and everything is cleaned up.
    // @@Speed: In some cases it might be even better to use the arena allocator directly, if we know something
    // is never gonna get freed, so that we don't have the time nor space overhead of doing memory blocks.
    Memory_Arena arena;
    Memory_Pool pool;
    Allocator pool_allocator;
    Allocator *allocator; // Usually a pointer to the pool_allocator, but can be swapped for testing...

#if USE_JOB_SYSTEM
    Mutex mutex;
    Job_System job_system;
#endif
    
    vec3 half_size; // This size is used to initialize the bvh. The bvh implementation does not support dynamic size changing, so this should be fixed.

    // The world owns all objects that are part of this problem. These objects
    // are stored here and can then be referenced in other parts of the algorithm.
    //
    // We assume that these arrays get filled once at the start and then are never
    // modified, so that pointers to these objects are stable.
    Resizable_Array<Anchor> anchors;
    Resizable_Array<Delimiter> delimiters;

    // The clipping planes around the entire world (indicated by the world size), since no volume or delimiter
    // plane should ever go past the dimensions of the world.
    Triangulated_Plane root_clipping_planes[6];

    // After having clipped all delimiters, all triangles of all delimiters get added to this BVH. We then use
    // this data structure to quickly determine which delimiter triangles are bordering which anchors (by doing
    // some raycasting checks based on the floodfilling results). This BVH does not contain any volume triangles
    // since we don't actually do any work on the volumes, after they have been created).
    BVH bvh;

    
    // :RootPlanesBVH
    // The root triangles are fucking terrible for the BVH as they tend to be really large and covering the entire
    // world space, which would lead to the BVH nodes to not have any shrinkage (and therefore benefit) at all.
    // Instead, we must manually cast against them.
    Resizable_Array<BVH_Entry> root_bvh_entries; 
    

    // --- Internal implementation
    void create_bvh();
    void create_bvh_from_triangles(Resizable_Array<Triangle> &triangles);
    void clip_delimiters();
    void build_anchor_volumes(real cell_world_space_size);

    b8 point_inside_bounds(vec3 point);
    b8 cast_ray_against_delimiters_and_root_planes(vec3 ray_origin, vec3 ray_direction, real max_ray_distance);
};



/* ---------------------------------------------- Random Utility ---------------------------------------------- */

real get_random_real_uniform(real low, real high);
