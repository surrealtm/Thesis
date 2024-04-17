#include "timing.h"
#include "os_specific.h"

#include "core.h"



/* ----------------------------------------------- 3D Geometry ----------------------------------------------- */

AABB aabb_from_position_and_size(v3f center, v3f half_sizes) {
    return { center - half_sizes, center + half_sizes };
}

Local_Axes local_axes_from_rotation(v3f euler_radians) {
    // @Incomplete: This doesn't actually rotate the axes!
    return { v3f(1, 0, 0), v3f(0, 1, 0), v3f(0, 0, 1) };
}



/* -------------------------------------------------- Octree -------------------------------------------------- */

void Octree::create(Allocator *allocator, v3f center, v3f half_size, u8 depth) {
    tmFunction(TM_OCTREE_COLOR);

    this->depth = depth;
    this->center = center;
    this->half_size = half_size;
    memset(this->children, 0, sizeof(this->children));

    this->contained_anchors.allocator = allocator;
    this->contained_boundaries.allocator = allocator;
}

Octree *Octree::get_octree_for_aabb(AABB const &aabb, Allocator *allocator) {
    tmFunction(TM_OCTREE_COLOR);
    
    //
    // Transform the AABB into local octree space. If the AABB is out of bounds of this octree,
    // then return null.
    //
    AABB local_aabb = { aabb.min - this->center, aabb.max - this->center };
    if(local_aabb.min.x < -this->half_size.x || local_aabb.min.y < -this->half_size.y || local_aabb.min.z < -this->half_size.z) return null;
    if(local_aabb.max.x >  this->half_size.x || local_aabb.max.y >  this->half_size.y || local_aabb.max.z >  this->half_size.z) return null;

    //
    // If this octree is at the max depth, then we are already done no matter what.
    //
    if(this->depth == MAX_OCTREE_DEPTH) return this;

    //
    // If the AABB crosses any of the axis of this octree, then it is contained inside this octree and we can stop.
    //
    if(local_aabb.min.x <= 0 && local_aabb.max.x >= 0 ||
        local_aabb.min.y <= 0 && local_aabb.max.y >= 0 ||
        local_aabb.min.z <= 0 && local_aabb.max.z >= 0) return this;

    //
    // Find the child which contains this aabb and recurse into there.
    // Since we got here, it means that for all axis, the sign of the min / max values
    // of the AABB is consistent.
    //
    Octree_Child_Index child_index = (Octree_Child_Index) ((local_aabb.min.x > 0 ? OCTREE_CHILD_px_flag : OCTREE_CHILD_nx_flag) +
                                                           (local_aabb.min.y > 0 ? OCTREE_CHILD_py_flag : OCTREE_CHILD_ny_flag) +
                                                           (local_aabb.min.z > 0 ? OCTREE_CHILD_pz_flag : OCTREE_CHILD_nz_flag));
    if(!this->children[child_index]) {
        v3f child_center = v3f(this->center.x + (child_index & OCTREE_CHILD_px_flag ? this->half_size.x / 2 : -this->half_size.x / 2),
                               this->center.y + (child_index & OCTREE_CHILD_py_flag ? this->half_size.y / 2 : -this->half_size.y / 2),
                               this->center.z + (child_index & OCTREE_CHILD_pz_flag ? this->half_size.z / 2 : -this->half_size.z / 2));
        this->children[child_index] = (Octree *) allocator->allocate(sizeof(Octree));
        this->children[child_index]->create(allocator, child_center, this->half_size / 2.f, this->depth + 1);
    }

    return this->children[child_index]->get_octree_for_aabb(aabb, allocator);
}



/* -------------------------------------------------- World -------------------------------------------------- */

void World::create(v3f half_size) {
    tmFunction(TM_WORLD_COLOR);

    //
    // Set up memory management.
    //
    this->arena.create(16 * ONE_MEGABYTE);
    this->pool.create(&this->arena);
    this->pool_allocator = this->pool.allocator();
    this->allocator = &this->pool_allocator;

    //
    // Set up the basic objects.
    //
    this->half_size = half_size;
    this->anchors.allocator    = this->allocator;
    this->boundaries.allocator = this->allocator;
    this->root.create(this->allocator, v3f(0), this->half_size);

    //
    // Create the clipping planes.
    //
    {
        tmZone("create_clipping_planes", TM_WORLD_COLOR);

        this->root_clipping_planes.add({ v3f(-this->half_size.x, 0, 0), v3f(+1, 0, 0), v3f(0, this->half_size.y, 0), v3f(0, 0, this->half_size.z) });
        this->root_clipping_planes.add({ v3f(+this->half_size.x, 0, 0), v3f(-1, 0, 0), v3f(0, this->half_size.y, 0), v3f(0, 0, this->half_size.z) });
        this->root_clipping_planes.add({ v3f(0, -this->half_size.y, 0), v3f(0, +1, 0), v3f(this->half_size.x, 0, 0), v3f(0, 0, this->half_size.z) });
        this->root_clipping_planes.add({ v3f(0, +this->half_size.y, 0), v3f(0, -1, 0), v3f(this->half_size.x, 0, 0), v3f(0, 0, this->half_size.z) });
        this->root_clipping_planes.add({ v3f(0, 0, -this->half_size.z), v3f(0, 0, +1), v3f(this->half_size.x, 0, 0), v3f(0, this->half_size.y, 0) });
        this->root_clipping_planes.add({ v3f(0, 0, +this->half_size.z), v3f(0, 0, -1), v3f(this->half_size.x, 0, 0), v3f(0, this->half_size.y, 0) });
    }
}

void World::destroy() {
    tmFunction(TM_WORLD_COLOR);
    this->arena.destroy();
}

void World::reserve_objects(s64 anchors, s64 boundaries) {
    tmFunction(TM_WORLD_COLOR);
    this->anchors.reserve(anchors);
    this->boundaries.reserve(boundaries);
}

void World::add_anchor(string name, v3f position) {
    tmFunction(TM_WORLD_COLOR);

    Anchor *anchor   = this->anchors.push();
    anchor->name     = copy_string(this->allocator, name);
    anchor->position = position;
}

Boundary *World::add_boundary(string name, v3f position, v3f size, Local_Axes local_axes) {
    tmFunction(TM_WORLD_COLOR);

    Boundary *boundary   = this->boundaries.push();
    boundary->name       = copy_string(this->allocator, name); // @Cleanup: This seems to be veryy fucking slow...
    boundary->position   = position;
    boundary->size       = size;
    boundary->aabb       = aabb_from_position_and_size(position, size);
    boundary->local_axes = local_axes;
    boundary->clipping_planes.allocator = this->allocator;

    return boundary;
}

void World::add_boundary_clipping_plane(Boundary *boundary, u8 axis_index) {
    tmFunction(TM_WORLD_COLOR);

    assert(axis_index < 3);

    v3f u = boundary->local_axes[(axis_index + 1) % 3];
    v3f v = boundary->local_axes[(axis_index + 2) % 3];

    f32 pu = this->get_shortest_distance_to_root_clip(boundary->position,  u);
    f32 nu = this->get_shortest_distance_to_root_clip(boundary->position, -u);
    
    f32 pv = this->get_shortest_distance_to_root_clip(boundary->position,  v);
    f32 nv = this->get_shortest_distance_to_root_clip(boundary->position, -v);

    f32 su = (pu + nu) / 2;
    f32 sv = (pv + nv) / 2;

    f32 ou = (pu - nu) / 2;
    f32 ov = (pv - nv) / 2;
    
    boundary->clipping_planes.add({ boundary->position + u * ou + v * ov, boundary->local_axes[axis_index], u * su, v * sv });
}

void World::add_boundary_clipping_plane(Boundary *boundary, v3f n, v3f u, v3f v) {
    tmFunction(TM_WORLD_COLOR);
    boundary->clipping_planes.add({ boundary->position, n, u, v });
}

f32 World::get_shortest_distance_to_root_clip(v3f position, v3f direction) {
    tmFunction(TM_WORLD_COLOR);

    f32 least_distance = MAX_F32;
    
    for(auto *plane : this->root_clipping_planes) {
        f32 distance_to_plane;
        b8 intersection = ray_plane_intersection(plane->p, plane->n, position, direction, distance_to_plane);
        if(intersection && distance_to_plane < least_distance) least_distance = distance_to_plane;
    }
    
    return least_distance;
}

void World::create_octree() {
    tmFunction(TM_WORLD_COLOR);
    
    //
    // Insert all boundaris.
    //
    for(auto *boundary : this->boundaries) {
        Octree *octree = this->root.get_octree_for_aabb(boundary->aabb, this->allocator);
        assert(octree && "Boundary Object is outside of octree bounds.");
        octree->contained_boundaries.add(boundary);
    }

    //
    // Insert all anchors.
    //
    for(auto *anchor : this->anchors) {
        AABB anchor_aabb = { anchor->position, anchor->position };
        Octree *octree = this->root.get_octree_for_aabb(anchor_aabb, this->allocator);
        assert(octree && "Boundary Object is outside of octree bounds.");
        octree->contained_anchors.add(anchor);
    }
}
