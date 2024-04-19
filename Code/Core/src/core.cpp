#include "timing.h"
#include "os_specific.h"

#include "core.h"



/* ----------------------------------------------- 3D Geometry ----------------------------------------------- */

Clipping_Plane::Clipping_Plane(Allocator *allocator, v3f p, v3f n, v3f u, v3f v) {
    v3f p0 = p - u - v;
    v3f p1 = p - u + v;
    v3f p2 = p + u - v;
    v3f p3 = p + u + v;

    this->triangles.allocator = allocator;
    this->triangles.add({ p0, p1, p3, n });
    this->triangles.add({ p0, p2, p3, n });
}

AABB aabb_from_position_and_size(v3f center, v3f half_sizes) {
    return { center - half_sizes, center + half_sizes };
}

Local_Axes local_axes_rotated(qtf quat, v3f axis_scale) {
    return { qt_rotate(quat, v3f(axis_scale.x, 0, 0)), qt_rotate(quat, v3f(0, axis_scale.y, 0)), qt_rotate(quat, v3f(0, 0, axis_scale.z)) };
}



/* ------------------------------------------------- Objects ------------------------------------------------- */

void add_triangles_for_clipping_plane(Resizable_Array<Triangle> * triangles, Clipping_Plane *plane) {
    for(auto *in : plane->triangles) {
        triangles->add({ in->p0, in->p1, in->p2, in->n });
    }
}

void Anchor::clip_vertex(v3f *vertex, Triangle *triangle) {
    tmFunction(TM_ANCHOR_COLOR);

    f32 distance = point_plane_distance_signed(*vertex, triangle->p0, triangle->n);
    
    if(distance < 0) {
        *vertex = *vertex - distance * triangle->n;
    }
}

void Anchor::clip_against_plane(Clipping_Plane *plane) {
    for(auto *plane_triangle : plane->triangles) {
        if(v3_dot_v3(plane_triangle->n, this->position - plane_triangle->p0) < 0.f) continue; // Don't clip against back-facing planes.

        for(auto *volume_triangle : this->volume) {
            this->clip_vertex(&volume_triangle->p0, plane_triangle);
            this->clip_vertex(&volume_triangle->p1, plane_triangle);
            this->clip_vertex(&volume_triangle->p2, plane_triangle);
        }
    }
}

void Anchor::clip_against_boundary(Boundary *boundary) {
    tmFunction(TM_ANCHOR_COLOR);

    for(auto *plane : boundary->clipping_planes) this->clip_against_plane(plane);
}


void Boundary::add_clipping_plane(Allocator *allocator, v3f p, v3f n, v3f u, v3f v) {
    this->clipping_planes.add(Clipping_Plane(allocator, p, n, u, v));
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
    this->root_clipping_planes.allocator = this->allocator;
    this->root.create(this->allocator, v3f(0), this->half_size);

    //
    // Create the clipping planes.
    //
    {
        tmZone("create_root_clipping_planes", TM_WORLD_COLOR);

        this->root_clipping_planes.add({ this->allocator, v3f(-this->half_size.x, 0, 0), v3f(+1, 0, 0), v3f(0, this->half_size.y, 0), v3f(0, 0, this->half_size.z) });
        this->root_clipping_planes.add({ this->allocator, v3f(+this->half_size.x, 0, 0), v3f(-1, 0, 0), v3f(0, this->half_size.y, 0), v3f(0, 0, this->half_size.z) });
        this->root_clipping_planes.add({ this->allocator, v3f(0, -this->half_size.y, 0), v3f(0, +1, 0), v3f(this->half_size.x, 0, 0), v3f(0, 0, this->half_size.z) });
        this->root_clipping_planes.add({ this->allocator, v3f(0, +this->half_size.y, 0), v3f(0, -1, 0), v3f(this->half_size.x, 0, 0), v3f(0, 0, this->half_size.z) });
        this->root_clipping_planes.add({ this->allocator, v3f(0, 0, -this->half_size.z), v3f(0, 0, +1), v3f(this->half_size.x, 0, 0), v3f(0, this->half_size.y, 0) });
        this->root_clipping_planes.add({ this->allocator, v3f(0, 0, +this->half_size.z), v3f(0, 0, -1), v3f(this->half_size.x, 0, 0), v3f(0, this->half_size.y, 0) });
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
    anchor->position = position;
    anchor->volume.allocator = this->allocator;

    anchor->dbg_name = copy_string(this->allocator, name);
}

Boundary *World::add_boundary(string name, v3f position, v3f half_size, v3f rotation) {
    tmFunction(TM_WORLD_COLOR);

    qtf quaternion = qt_from_euler_turns(rotation);

    Boundary *boundary          = this->boundaries.push();
    boundary->position          = position;
    boundary->aabb              = aabb_from_position_and_size(position, half_size);
    boundary->local_scaled_axes = local_axes_rotated(quaternion, half_size);
    boundary->local_unit_axes   = local_axes_rotated(quaternion, v3f(1));
    boundary->clipping_planes.allocator = this->allocator;

    boundary->dbg_name      = copy_string(this->allocator, name); // @Cleanup: This seems to be veryy fucking slow...
    boundary->dbg_half_size = half_size;
    boundary->dbg_rotation  = quaternion;

    return boundary;
}

void World::add_boundary_clipping_planes(Boundary *boundary, Axis normal_axis) {
    tmFunction(TM_WORLD_COLOR);

    assert(normal_axis >= 0 && normal_axis < AXIS_COUNT);

    v3f a = boundary->local_scaled_axes[normal_axis];
    v3f n = boundary->local_unit_axes[normal_axis];
    v3f u = boundary->local_unit_axes[(normal_axis + 1) % 3];
    v3f v = boundary->local_unit_axes[(normal_axis + 2) % 3];

    //
    // A boundary owns an actual volume, which means that the clipping plane
    // shouldn't actually go through the center, but be aligned with the sides
    // of this volume. This, in turn, means that there should actually be two
    // clipping planes, one on each side of the axis.
    //

    f32 extension = max(max(this->half_size.x, this->half_size.y), this->half_size.z) * 2.0f; // This clipping plane should stretch across the entire world, before it might be clipped down.
    boundary->add_clipping_plane(this->allocator, boundary->position + a,  n, u * extension, v * extension);
    boundary->add_clipping_plane(this->allocator, boundary->position - a, -n, u * extension, v * extension);
}

void World::make_root_volume(Resizable_Array<Triangle> *triangles) {
    tmFunction(TM_WORLD_COLOR);
    
    /*
    // nocheckin
    for(auto *clipping_plane : this->root_clipping_planes) {
        add_triangles_for_clipping_plane(triangles, clipping_plane);
    }
    */

    auto *clipping_plane = &this->root_clipping_planes[2];
    add_triangles_for_clipping_plane(triangles, clipping_plane);
}

void World::create_octree() {
    tmFunction(TM_WORLD_COLOR);

    //
    // Insert all boundaries.
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

void World::clip_all_boundaries() {
    tmFunction(TM_WORLD_COLOR);

    for(auto *boundary : this->boundaries) {
        for(auto *plane : boundary->clipping_planes) {
            // @Incomplete.
        }
    }
}

void World::calculate_volumes() {
    tmFunction(TM_WORLD_COLOR);

    this->clip_all_boundaries();

    for(auto *anchor : this->anchors) {
        this->make_root_volume(&anchor->volume);

        for(auto *boundary : this->boundaries) {
            anchor->clip_against_boundary(boundary);
        }
    }
}



void World::begin_calculate_volumes_stepping() {
    tmFunction(TM_WORLD_COLOR);

    this->clip_all_boundaries();

    this->dbg_step_anchor_iterator = this->anchors.begin();
    this->dbg_step_boundary_iterator = this->boundaries.begin();
}

void World::calculate_volumes_step() {
    tmFunction(TM_WORLD_COLOR);

    if(this->dbg_step_anchor_iterator == this->anchors.end()) return;

    Anchor *anchor = this->dbg_step_anchor_iterator.pointer;

    if(this->dbg_step_boundary_iterator == this->boundaries.begin()) this->make_root_volume(&anchor->volume);

    Boundary *boundary = this->dbg_step_boundary_iterator.pointer;
    anchor->clip_against_boundary(boundary);

    ++this->dbg_step_boundary_iterator;

    if(this->dbg_step_boundary_iterator == this->boundaries.end()) {
        this->dbg_step_boundary_iterator = this->boundaries.begin();
        ++this->dbg_step_anchor_iterator;
    }
}