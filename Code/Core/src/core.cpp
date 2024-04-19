#include "timing.h"
#include "os_specific.h"

#include "core.h"



/* -------------------------------------------------- Volume -------------------------------------------------- */

void Volume::recalculate_aabb() {
    tmFunction(TM_VOLUME_COLOR);

    this->aabb.max = v3f(MIN_F32);
    this->aabb.min = v3f(MAX_F32);

    for(auto *triangle : this->triangles) {
        this->aabb.max.x = max(this->aabb.max.x, triangle->p0.x);
        this->aabb.max.y = max(this->aabb.max.y, triangle->p0.y);
        this->aabb.max.z = max(this->aabb.max.z, triangle->p0.z);

        this->aabb.max.x = max(this->aabb.max.x, triangle->p1.x);
        this->aabb.max.y = max(this->aabb.max.y, triangle->p1.y);
        this->aabb.max.z = max(this->aabb.max.z, triangle->p1.z);

        this->aabb.max.x = max(this->aabb.max.x, triangle->p2.x);
        this->aabb.max.y = max(this->aabb.max.y, triangle->p2.y);
        this->aabb.max.z = max(this->aabb.max.z, triangle->p2.z);

        this->aabb.min.x = min(this->aabb.min.x, triangle->p0.x);
        this->aabb.min.y = min(this->aabb.min.y, triangle->p0.y);
        this->aabb.min.z = min(this->aabb.min.z, triangle->p0.z);

        this->aabb.min.x = min(this->aabb.min.x, triangle->p1.x);
        this->aabb.min.y = min(this->aabb.min.y, triangle->p1.y);
        this->aabb.min.z = min(this->aabb.min.z, triangle->p1.z);

        this->aabb.min.x = min(this->aabb.min.x, triangle->p2.x);
        this->aabb.min.y = min(this->aabb.min.y, triangle->p2.y);
        this->aabb.min.z = min(this->aabb.min.z, triangle->p2.z);
    }

    this->aabb_dirty = false;
}

void Volume::maybe_recalculate_aabb() {
    if(this->aabb_dirty) this->recalculate_aabb();
}

void Volume::add_triangles_for_clipping_plane(Clipping_Plane *plane) {
    v3f p0 = plane->p + plane->u + plane->v;
    v3f p1 = plane->p + plane->u - plane->v;
    v3f p2 = plane->p - plane->u + plane->v;
    v3f p3 = plane->p - plane->u - plane->v;
    
    this->triangles.add({ p0, p1, p3 });
    this->triangles.add({ p0, p2, p3 });

    this->aabb_dirty = true;
}

void Volume::clip_vertex_against_plane(v3f *vertex, Clipping_Plane *plane) {
    f32 distance = point_plane_distance_signed(*vertex, plane->p, plane->n);
    if(distance > 0) {
        vertex->x += plane->n.x * distance;
        vertex->y += plane->n.y * distance;
        vertex->z += plane->n.z * distance;
    }
}

void Volume::clip_against_plane(Clipping_Plane *plane) {
    tmFunction(TM_VOLUME_COLOR);

    // Don't clip against back-facing planes.
    if(v3_dot_v3(plane->n, plane->p - this->anchor_point) > 0.f) return;
    
    for(auto *triangle : this->triangles) {
        this->clip_vertex_against_plane(&triangle->p0, plane);    
        this->clip_vertex_against_plane(&triangle->p1, plane);    
        this->clip_vertex_against_plane(&triangle->p2, plane);    
    }
    
    this->aabb_dirty = true;
}

void Volume::clip_against_boundary(Boundary *boundary) {
    tmFunction(TM_VOLUME_COLOR);

    for(auto *plane : boundary->clipping_planes) this->clip_against_plane(plane);
}



/* ----------------------------------------------- 3D Geometry ----------------------------------------------- */

AABB aabb_from_position_and_size(v3f center, v3f half_sizes) {
    return { center - half_sizes, center + half_sizes };
}

Local_Axes local_axes_from_rotation(qtf rotation) {
    return { qt_rotate(rotation, v3f(1, 0, 0)), qt_rotate(rotation, v3f(0, 1, 0)), qt_rotate(rotation, v3f(0, 0, 1)) };
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
    anchor->position = position;
    
    anchor->dbg_name = copy_string(this->allocator, name);
}

Boundary *World::add_boundary(string name, v3f position, v3f size, v3f rotation) {
    tmFunction(TM_WORLD_COLOR);

    qtf quaternion = qt_from_euler_turns(rotation);
    Local_Axes local_axes = local_axes_from_rotation(quaternion);

    Boundary *boundary      = this->boundaries.push();
    boundary->position      = position;
    boundary->aabb          = aabb_from_position_and_size(position, size);
    boundary->local_axes[0] = local_axes[0] * size;
    boundary->local_axes[1] = local_axes[1] * size;
    boundary->local_axes[2] = local_axes[2] * size;
    boundary->clipping_planes.allocator = this->allocator;
    
    boundary->dbg_name      = copy_string(this->allocator, name); // @Cleanup: This seems to be veryy fucking slow...
    boundary->dbg_rotation  = quaternion;
    boundary->dbg_size      = size;
    
    return boundary;
}

void World::add_boundary_clipping_planes(Boundary *boundary, Axis normal_axis) {
    tmFunction(TM_WORLD_COLOR);

    assert(axis_index >= 0 && axis_index < AXIS_COUNT);

    v3f a = boundary->local_axes[normal_axis];
    v3f u = boundary->local_axes[(normal_axis + 1) % 3];
    v3f v = boundary->local_axes[(normal_axis + 2) % 3];
    v3f n = v3_normalize(a);
    
    //
    // A boundary owns an actual volume, which means that the clipping plane
    // shouldn't actually go through the center, but be aligned with the sides
    // of this volume. This, in turn, means that there should actually be two
    // clipping planes, one on each side of the axis.
    //
    this->add_boundary_clipping_plane_checked(boundary, boundary->position + a,  n, u, v);
    this->add_boundary_clipping_plane_checked(boundary, boundary->position - a, -n, u, v);
}

void World::add_boundary_clipping_plane_checked(Boundary *boundary, v3f p, v3f n, v3f u, v3f v) {
    tmFunction(TM_WORLD_COLOR);

    f32 pu = this->get_shortest_distance_to_root_clip(p,  u);
    f32 nu = this->get_shortest_distance_to_root_clip(p, -u);
    
    f32 pv = this->get_shortest_distance_to_root_clip(p,  v);
    f32 nv = this->get_shortest_distance_to_root_clip(p, -v);

    f32 su = (pu + nu) / 2;
    f32 sv = (pv + nv) / 2;

    f32 ou = (pu - nu) / 2;
    f32 ov = (pv - nv) / 2;

    boundary->clipping_planes.add({ p + u * ou + v * ov, n, u * su, v * sv });
}

void World::add_boundary_clipping_plane_unchecked(Boundary *boundary, v3f p, v3f n, v3f u, v3f v) {
    tmFunction(TM_WORLD_COLOR);
    boundary->clipping_planes.add({ p, n, u, v });
}

f32 World::get_shortest_distance_to_root_clip(v3f position, v3f direction) {
    tmFunction(TM_WORLD_COLOR);

    f32 least_distance = MAX_F32;
    
    for(auto *plane : this->root_clipping_planes) {
        f32 distance_to_plane;
        b8 intersection = ray_plane_intersection(position, direction, plane->p, plane->n, &distance_to_plane);
        if(intersection && distance_to_plane < least_distance) least_distance = distance_to_plane;
    }
    
    return least_distance;
}

Volume World::make_root_volume() {
    tmFunction(TM_WORLD_COLOR);

    Volume volume;
    volume.triangles.allocator = this->allocator;
    volume.aabb_dirty = true;

    for(auto *clipping_plane : this->root_clipping_planes) {
        volume.add_triangles_for_clipping_plane(clipping_plane);
    }

    volume.maybe_recalculate_aabb();
    
    return volume;
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

void World::calculate_volumes() {
    tmFunction(TM_WORLD_COLOR);

    //
    // Clip all boundaries against each other.
    //
    {
        tmZone("clip_all_boundaries", TM_WORLD_COLOR);
        
        for(auto *boundary : this->boundaries) {
            for(auto *plane : boundary->clipping_planes) {
                // @Incomplete.
            }
        }
    }

    //
    // Create a volume for each anchor, and clip it against every boundary.
    //
    {
        tmZone("build_all_volumes", TM_WORLD_COLOR);

        for(auto *anchor : this->anchors) {
            anchor->volume = this->make_root_volume();
            anchor->volume.anchor_point = anchor->position;
            
            for(auto *boundary : this->boundaries) {
                anchor->volume.clip_against_boundary(boundary);
            }
        }
    }
}
