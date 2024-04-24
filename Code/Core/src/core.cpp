#include "timing.h"
#include "core.h"
#include "tessel.h"



/* ----------------------------------------------- 3D Geometry ----------------------------------------------- */

Triangle::Triangle(v3f p0, v3f p1, v3f p2) {
    this->p0 = p0;
    this->p1 = p1;
    this->p2 = p2;
}

f32 Triangle::approximate_surface_area() {
    // https://math.stackexchange.com/questions/128991/how-to-calculate-the-area-of-a-3d-triangle
    v3f h = v3_cross_v3(this->p0 - this->p1, this->p0 - this->p2);
    return v3_length2(h) / 2;
}

b8 Triangle::is_dead() {
    return this->approximate_surface_area() <= F32_EPSILON;
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
    this->root_clipping_triangles.allocator = this->allocator;
    this->root.create(this->allocator, v3f(0), this->half_size);

    //
    // Create the clipping planes.
    //
    {
        tmZone("create_root_clipping_triangles", TM_WORLD_COLOR);

        // X-Axis
        this->root_clipping_triangles.add({ v3f(-this->half_size.x,  this->half_size.y,  this->half_size.z), v3f(-this->half_size.x, -this->half_size.y,  this->half_size.z), v3f(-this->half_size.x, -this->half_size.y, -this->half_size.z) });
        this->root_clipping_triangles.add({ v3f(-this->half_size.x,  this->half_size.y,  this->half_size.z), v3f(-this->half_size.x,  this->half_size.y, -this->half_size.z), v3f(-this->half_size.x, -this->half_size.y, -this->half_size.z) });
        this->root_clipping_triangles.add({ v3f( this->half_size.x,  this->half_size.y,  this->half_size.z), v3f( this->half_size.x, -this->half_size.y,  this->half_size.z), v3f( this->half_size.x, -this->half_size.y, -this->half_size.z) });
        this->root_clipping_triangles.add({ v3f( this->half_size.x,  this->half_size.y,  this->half_size.z), v3f( this->half_size.x,  this->half_size.y, -this->half_size.z), v3f( this->half_size.x, -this->half_size.y, -this->half_size.z) });

        // Y-Axis
        this->root_clipping_triangles.add({ v3f( this->half_size.x, -this->half_size.y,  this->half_size.z), v3f(-this->half_size.x, -this->half_size.y,  this->half_size.z), v3f(-this->half_size.x, -this->half_size.y, -this->half_size.z) });
        this->root_clipping_triangles.add({ v3f( this->half_size.x, -this->half_size.y,  this->half_size.z), v3f( this->half_size.x, -this->half_size.y, -this->half_size.z), v3f(-this->half_size.x, -this->half_size.y, -this->half_size.z) });
        this->root_clipping_triangles.add({ v3f( this->half_size.x,  this->half_size.y,  this->half_size.z), v3f(-this->half_size.x,  this->half_size.y,  this->half_size.z), v3f(-this->half_size.x,  this->half_size.y, -this->half_size.z) });
        this->root_clipping_triangles.add({ v3f( this->half_size.x,  this->half_size.y,  this->half_size.z), v3f( this->half_size.x,  this->half_size.y, -this->half_size.z), v3f(-this->half_size.x,  this->half_size.y, -this->half_size.z) });

        // Z-Axis
        this->root_clipping_triangles.add({ v3f( this->half_size.x,  this->half_size.y, -this->half_size.z), v3f(-this->half_size.x,  this->half_size.y, -this->half_size.z), v3f(-this->half_size.x, -this->half_size.y, -this->half_size.z) });
        this->root_clipping_triangles.add({ v3f( this->half_size.x,  this->half_size.y, -this->half_size.z), v3f( this->half_size.x, -this->half_size.y, -this->half_size.z), v3f(-this->half_size.x, -this->half_size.y, -this->half_size.z) });
        this->root_clipping_triangles.add({ v3f( this->half_size.x,  this->half_size.y,  this->half_size.z), v3f(-this->half_size.x,  this->half_size.y,  this->half_size.z), v3f(-this->half_size.x, -this->half_size.y,  this->half_size.z) });
        this->root_clipping_triangles.add({ v3f( this->half_size.x,  this->half_size.y,  this->half_size.z), v3f( this->half_size.x, -this->half_size.y,  this->half_size.z), v3f(-this->half_size.x, -this->half_size.y,  this->half_size.z) });
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

Anchor *World::add_anchor(string name, v3f position) {
    tmFunction(TM_WORLD_COLOR);

    Anchor *anchor   = this->anchors.push();
    anchor->position = position;
    anchor->volume_triangles.allocator = this->allocator;
    anchor->dbg_name = copy_string(this->allocator, name);
    
    return anchor;
}

Boundary *World::add_boundary(string name, v3f position, v3f half_size, v3f rotation) {
    tmFunction(TM_WORLD_COLOR);

    qtf quaternion = qt_from_euler_turns(rotation);

    Boundary *boundary                  = this->boundaries.push();
    boundary->position                  = position;
    boundary->aabb                      = { position - half_size, position + half_size };
    boundary->local_scaled_axes[AXIS_X] = qt_rotate(quaternion, v3f(half_size.x, 0, 0));
    boundary->local_scaled_axes[AXIS_Y] = qt_rotate(quaternion, v3f(0, half_size.y, 0));
    boundary->local_scaled_axes[AXIS_Z] = qt_rotate(quaternion, v3f(0, 0, half_size.z));
    boundary->local_unit_axes[AXIS_X]   = qt_rotate(quaternion, v3f(1, 0, 0));
    boundary->local_unit_axes[AXIS_Y]   = qt_rotate(quaternion, v3f(0, 1, 0));
    boundary->local_unit_axes[AXIS_Z]   = qt_rotate(quaternion, v3f(0, 0, 1));
    boundary->clipping_triangles.allocator = this->allocator;

    boundary->dbg_name      = copy_string(this->allocator, name); // @@Speed: This seems to be veryy fucking slow...
    boundary->dbg_half_size = half_size;
    boundary->dbg_rotation  = quaternion;

    return boundary;
}

void World::add_boundary_clipping_planes(Boundary *boundary, Axis normal_axis) {
    tmFunction(TM_WORLD_COLOR);

    assert(normal_axis >= 0 && normal_axis < AXIS_COUNT);

    f32 extension = max(max(this->half_size.x, this->half_size.y), this->half_size.z) * 2.0f; // This clipping plane should stretch across the entire world, before it might be clipped down.

    v3f a = boundary->local_scaled_axes[normal_axis];
    v3f u = boundary->local_unit_axes[(normal_axis + 1) % 3] * extension;
    v3f v = boundary->local_unit_axes[(normal_axis + 2) % 3] * extension;

    //
    // A boundary owns an actual volume, which means that the clipping plane
    // shouldn't actually go through the center, but be aligned with the sides
    // of this volume. This, in turn, means that there should actually be two
    // clipping planes, one on each side of the axis.
    //
    
    {
        v3f c = boundary->position + a;
        v3f p0 = c - u - v;
        v3f p1 = c - u + v;
        v3f p2 = c + u - v;
        v3f p3 = c + u + v;
        boundary->clipping_triangles.add( { p0, p1, p3 });
        boundary->clipping_triangles.add( { p0, p2, p3 });
    }

    {
        v3f c = boundary->position - a;
        v3f p0 = c - u - v;
        v3f p1 = c - u + v;
        v3f p2 = c + u - v;
        v3f p3 = c + u + v;
        boundary->clipping_triangles.add( { p0, p1, p3 });
        boundary->clipping_triangles.add( { p0, p2, p3 });
    }
}

void World::add_centered_boundary_clipping_plane(Boundary *boundary, Axis normal_axis) {
tmFunction(TM_WORLD_COLOR);

    assert(normal_axis >= 0 && normal_axis < AXIS_COUNT);

    f32 extension = max(max(this->half_size.x, this->half_size.y), this->half_size.z) * 2.0f; // This clipping plane should stretch across the entire world, before it might be clipped down.

    v3f a = boundary->local_scaled_axes[normal_axis];
    v3f u = boundary->local_unit_axes[(normal_axis + 1) % 3] * extension;
    v3f v = boundary->local_unit_axes[(normal_axis + 2) % 3] * extension;

    v3f c = boundary->position;
    v3f p0 = c - u - v;
    v3f p1 = c - u + v;
    v3f p2 = c + u - v;
    v3f p3 = c + u + v;
    boundary->clipping_triangles.add( { p0, p1, p3 });
    boundary->clipping_triangles.add( { p0, p2, p3});
}

void World::make_root_volume(Resizable_Array<Triangle> *volume) {
    tmFunction(TM_WORLD_COLOR);

    /*
    // nocheckin
    for(auto *triangle : this->root_clipping_triangles) {
        volume->add(*triangle);
    }
    */

    {
        Triangle *clip = &this->root_clipping_triangles[2];
        volume->add(*clip);
    }
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

void World::calculate_volumes(b8 single_step) {
    tmFunction(TM_WORLD_COLOR);

    this->dbg_step_anchor_index            = 0;
    this->dbg_step_boundary_index          = 0;
    this->dbg_step_clipping_triangle_index = 0;
    this->dbg_step_volume_triangle_index   = 0;
    this->did_step_before                  = false;
    if(!single_step) this->calculate_volumes_step(single_step);
}

// :DbgStep
void World::calculate_volumes_step(b8 single_step) {
    // So that the 'goto' doesn't (wrongly) complain about the skipping
    // of variable initialization.
    Anchor *anchor;
    Triangle *volume_triangle, *clipping_triangle;
    Boundary *boundary;
    s64 new_triangle_count;

    if(single_step && this->did_step_before) goto continue_from_single_step_exit;
    this->did_step_before = true;

    for(; this->dbg_step_anchor_index < this->anchors.count; ++this->dbg_step_anchor_index) {
        if(!this->anchors[this->dbg_step_anchor_index].volume_triangles.count) this->make_root_volume(&this->anchors[this->dbg_step_anchor_index].volume_triangles); // Don't create the root volume in case this anchor has a precomputed base volume.

        for(; this->dbg_step_volume_triangle_index < this->anchors[this->dbg_step_anchor_index].volume_triangles.count; ++this->dbg_step_volume_triangle_index) { // We are modifying this volume array inside the loop!
            for(; this->dbg_step_boundary_index < this->boundaries.count; ++this->dbg_step_boundary_index) {
                for(; this->dbg_step_clipping_triangle_index < this->boundaries[this->dbg_step_boundary_index].clipping_triangles.count;) {
                    anchor            = &this->anchors[this->dbg_step_anchor_index];
                    volume_triangle   = &anchor->volume_triangles[this->dbg_step_volume_triangle_index];
                    boundary          = &this->boundaries[this->dbg_step_boundary_index];
                    clipping_triangle = &boundary->clipping_triangles[this->dbg_step_clipping_triangle_index];

                    new_triangle_count = tessellate(volume_triangle, clipping_triangle, &anchor->volume_triangles);
                    
                    ++this->dbg_step_clipping_triangle_index;

                    if(single_step) return;
                                
                continue_from_single_step_exit:
                    continue;
                }

                this->dbg_step_clipping_triangle_index = 0;
            }

            this->dbg_step_boundary_index = 0;
        }

        this->dbg_step_volume_triangle_index = 0;
    }

    // If we actually finish the volume calcuation, then reset all these things.
    this->did_step_before                  = false;
    this->dbg_step_anchor_index            = MAX_S64;
    this->dbg_step_boundary_index          = MAX_S64;
    this->dbg_step_clipping_triangle_index = MAX_S64;
    this->dbg_step_volume_triangle_index   = MAX_S64;
}
