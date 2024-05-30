#include "core.h"
#include "tessel.h"

#include "timing.h"
#include "random.h"



/* ----------------------------------------------- 3D Geometry ----------------------------------------------- */

Triangle::Triangle(vec3 p0, vec3 p1, vec3 p2, vec3 n) : p0(p0), p1(p1), p2(p2), n(n) {}

real Triangle::approximate_surface_area() {
    // https://math.stackexchange.com/questions/128991/how-to-calculate-the-area-of-a-3d-triangle
    vec3 h = v3_cross_v3(this->p1 - this->p0, this->p2 - this->p0);
    return v3_length2(h) / 2;
}

b8 Triangle::is_dead() {
    return this->approximate_surface_area() <= CORE_EPSILON;
}

b8 Triangle::is_fully_behind_plane(Triangle *plane) {
    //
    // Returns true if this triangle is on the "backface" side of the plane, meaning
    // in the opposite direction of the plane's normal. This usually means we should clip
    // this triangle.
    //
    real d0 = v3_dot_v3(this->p0 - plane->p0, plane->n);
    real d1 = v3_dot_v3(this->p1 - plane->p0, plane->n);
    real d2 = v3_dot_v3(this->p2 - plane->p0, plane->n);

    //
    // Return true if no point is on the "frontface" of the clipping triangle, AND at least one
    // point is not on the clipping triangle. We don't want to clip triangles which lie on the
    // plane.
    //
    return d0 <= CORE_EPSILON && d1 <= CORE_EPSILON && d2 <= CORE_EPSILON && (d0 < -CORE_EPSILON || d1 < -CORE_EPSILON || d2 < -CORE_EPSILON);
}

b8 Triangle::no_point_behind_plane(Triangle *plane) {
    //
    // Ensures that all points of this triangle lie on the "frontface" of the clipping triangle.
    //
    real d0 = v3_dot_v3(this->p0 - plane->p0, plane->n);
    real d1 = v3_dot_v3(this->p1 - plane->p0, plane->n);
    real d2 = v3_dot_v3(this->p2 - plane->p0, plane->n);

    return d0 >= -CORE_EPSILON || d1 >= -CORE_EPSILON && d2 >= -CORE_EPSILON;
}



/* -------------------------------------------------- Octree -------------------------------------------------- */

void Octree::create(Allocator *allocator, vec3 center, vec3 half_size, u8 depth) {
    tmFunction(TM_OCTREE_COLOR);

    this->depth = depth;
    this->center = center;
    this->half_size = half_size;
    memset(this->children, 0, sizeof(this->children));

    this->contained_anchors.allocator = allocator;
    this->contained_delimiters.allocator = allocator;
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
        vec3 child_center = vec3(this->center.x + (child_index & OCTREE_CHILD_px_flag ? this->half_size.x / 2 : -this->half_size.x / 2),
                                 this->center.y + (child_index & OCTREE_CHILD_py_flag ? this->half_size.y / 2 : -this->half_size.y / 2),
                                 this->center.z + (child_index & OCTREE_CHILD_pz_flag ? this->half_size.z / 2 : -this->half_size.z / 2));
        this->children[child_index] = (Octree *) allocator->allocate(sizeof(Octree));
        this->children[child_index]->create(allocator, child_center, this->half_size / static_cast<real>(2.), this->depth + 1);
    }

    return this->children[child_index]->get_octree_for_aabb(aabb, allocator);
}



/* -------------------------------------------------- World -------------------------------------------------- */

void World::create(vec3 half_size) {
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
    this->delimiters.allocator = this->allocator;
    this->root_clipping_triangles.allocator = this->allocator;
    this->root.create(this->allocator, vec3(0), this->half_size);

    //
    // Create the clipping planes.
    //
    {
        tmZone("create_root_clipping_triangles", TM_WORLD_COLOR);

        // X-Axis
        this->root_clipping_triangles.add({ vec3(-this->half_size.x,  this->half_size.y,  this->half_size.z), vec3(-this->half_size.x, -this->half_size.y,  this->half_size.z), vec3(-this->half_size.x, -this->half_size.y, -this->half_size.z), vec3(1, 0, 0) });
        this->root_clipping_triangles.add({ vec3(-this->half_size.x,  this->half_size.y, -this->half_size.z), vec3(-this->half_size.x,  this->half_size.y,  this->half_size.z), vec3(-this->half_size.x, -this->half_size.y, -this->half_size.z), vec3(1, 0, 0) });

        this->root_clipping_triangles.add({ vec3( this->half_size.x, -this->half_size.y,  this->half_size.z), vec3( this->half_size.x,  this->half_size.y,  this->half_size.z), vec3( this->half_size.x, -this->half_size.y, -this->half_size.z), vec3(-1, 0, 0) });
        this->root_clipping_triangles.add({ vec3( this->half_size.x,  this->half_size.y,  this->half_size.z), vec3( this->half_size.x,  this->half_size.y, -this->half_size.z), vec3( this->half_size.x, -this->half_size.y, -this->half_size.z), vec3(-1, 0, 0) });

        // Y-Axis
        this->root_clipping_triangles.add({ vec3(-this->half_size.x, -this->half_size.y, -this->half_size.z), vec3(-this->half_size.x, -this->half_size.y,  this->half_size.z), vec3( this->half_size.x, -this->half_size.y,  this->half_size.z), vec3(0, 1, 0) });
        this->root_clipping_triangles.add({ vec3( this->half_size.x, -this->half_size.y,  this->half_size.z), vec3( this->half_size.x, -this->half_size.y, -this->half_size.z), vec3(-this->half_size.x, -this->half_size.y, -this->half_size.z), vec3(0, 1, 0) });

        this->root_clipping_triangles.add({ vec3( this->half_size.x,  this->half_size.y,  this->half_size.z), vec3(-this->half_size.x,  this->half_size.y,  this->half_size.z), vec3(-this->half_size.x,  this->half_size.y, -this->half_size.z), vec3(0, -1, 0) });
        this->root_clipping_triangles.add({ vec3( this->half_size.x,  this->half_size.y, -this->half_size.z), vec3( this->half_size.x,  this->half_size.y,  this->half_size.z), vec3(-this->half_size.x,  this->half_size.y, -this->half_size.z), vec3(0, -1, 0) });

        // Z-Axis
        this->root_clipping_triangles.add({ vec3( this->half_size.x,  this->half_size.y, -this->half_size.z), vec3(-this->half_size.x,  this->half_size.y, -this->half_size.z), vec3(-this->half_size.x, -this->half_size.y, -this->half_size.z), vec3(0, 0, 1) });
        this->root_clipping_triangles.add({ vec3( this->half_size.x, -this->half_size.y, -this->half_size.z), vec3( this->half_size.x,  this->half_size.y, -this->half_size.z), vec3(-this->half_size.x, -this->half_size.y, -this->half_size.z), vec3(0, 0, 1) });

        this->root_clipping_triangles.add({ vec3(-this->half_size.x,  this->half_size.y,  this->half_size.z), vec3( this->half_size.x,  this->half_size.y,  this->half_size.z), vec3(-this->half_size.x, -this->half_size.y,  this->half_size.z), vec3(0, 0, -1) });
        this->root_clipping_triangles.add({ vec3( this->half_size.x,  this->half_size.y,  this->half_size.z), vec3( this->half_size.x, -this->half_size.y,  this->half_size.z), vec3(-this->half_size.x, -this->half_size.y,  this->half_size.z), vec3(0, 0, -1) });
    }
}

void World::destroy() {
    tmFunction(TM_WORLD_COLOR);
    this->arena.destroy();
}

void World::reserve_objects(s64 anchors, s64 delimiters) {
    tmFunction(TM_WORLD_COLOR);
    this->anchors.reserve(anchors);
    this->delimiters.reserve(delimiters);
}

Anchor *World::add_anchor(vec3 position) {
    tmFunction(TM_WORLD_COLOR);

    Anchor *anchor   = this->anchors.push();
    anchor->position = position;
    anchor->volume_triangles.allocator = this->allocator;

    return anchor;
}

Anchor *World::add_anchor(string dbg_name, vec3 position) {
    Anchor *anchor = this->add_anchor(position);
    anchor->dbg_name = dbg_name;
    return anchor;
}

Delimiter *World::add_delimiter(vec3 position, vec3 half_size, vec3 rotation) {
    tmFunction(TM_WORLD_COLOR);

    quat quaternion = qt_from_euler_turns(rotation);

    Delimiter *delimiter                 = this->delimiters.push();
    delimiter->position                  = position;
    delimiter->aabb                      = { position - half_size, position + half_size };
    delimiter->local_scaled_axes[AXIS_X] = qt_rotate(quaternion, vec3(half_size.x, 0, 0));
    delimiter->local_scaled_axes[AXIS_Y] = qt_rotate(quaternion, vec3(0, half_size.y, 0));
    delimiter->local_scaled_axes[AXIS_Z] = qt_rotate(quaternion, vec3(0, 0, half_size.z));
    delimiter->local_unit_axes[AXIS_X]   = qt_rotate(quaternion, vec3(1, 0, 0));
    delimiter->local_unit_axes[AXIS_Y]   = qt_rotate(quaternion, vec3(0, 1, 0));
    delimiter->local_unit_axes[AXIS_Z]   = qt_rotate(quaternion, vec3(0, 0, 1));
    delimiter->clipping_triangles.allocator = this->allocator;

    delimiter->dbg_half_size = half_size;
    delimiter->dbg_rotation  = quaternion;

    return delimiter;
}

Delimiter *World::add_delimiter(string dbg_name, vec3 position, vec3 half_size, vec3 rotation) {
    Delimiter *delimiter = this->add_delimiter(position, half_size, rotation);
    delimiter->dbg_name = dbg_name;
    return delimiter;
}

void World::add_delimiter_clipping_planes(Delimiter *delimiter, Axis normal_axis) {
    tmFunction(TM_WORLD_COLOR);

    assert(normal_axis >= 0 && normal_axis < AXIS_COUNT);

    real extension = max(max(this->half_size.x, this->half_size.y), this->half_size.z) * 2.f; // Uniformly scale the clipping plane to cover the entire world space, before it will probably get cut down later.

    vec3 a = delimiter->local_scaled_axes[normal_axis];
    vec3 n = delimiter->local_unit_axes[normal_axis];
    vec3 u = delimiter->local_unit_axes[(normal_axis + 1) % 3] * extension;
    vec3 v = delimiter->local_unit_axes[(normal_axis + 2) % 3] * extension;

    //
    // A delimiter owns an actual volume, which means that the clipping plane
    // shouldn't actually go through the center, but be aligned with the sides
    // of this volume. This, in turn, means that there should actually be two
    // clipping planes, one on each side of the axis.
    //

    {
        vec3 c = delimiter->position + a;
        vec3 p0 = c - u - v;
        vec3 p1 = c - u + v;
        vec3 p2 = c + u - v;
        vec3 p3 = c + u + v;
        delimiter->clipping_triangles.add({ p0, p3, p1, n });
        delimiter->clipping_triangles.add({ p0, p2, p3, n });
    }

    {
        vec3 c = delimiter->position - a;
        vec3 p0 = c - u - v;
        vec3 p1 = c - u + v;
        vec3 p2 = c + u - v;
        vec3 p3 = c + u + v;
        delimiter->clipping_triangles.add({ p0, p1, p3, n });
        delimiter->clipping_triangles.add({ p0, p3, p2, n });
    }
}

void World::add_centered_delimiter_clipping_plane(Delimiter *delimiter, Axis normal_axis) {
    tmFunction(TM_WORLD_COLOR);

    assert(normal_axis >= 0 && normal_axis < AXIS_COUNT);

    real extension = max(max(this->half_size.x, this->half_size.y), this->half_size.z) * 2.f; // Uniformly scale the clipping plane to cover the entire world space, before it will probably get cut down later.

    vec3 a = delimiter->local_scaled_axes[normal_axis];
    vec3 n = delimiter->local_unit_axes[normal_axis];
    vec3 u = delimiter->local_unit_axes[(normal_axis + 1) % 3] * extension;
    vec3 v = delimiter->local_unit_axes[(normal_axis + 2) % 3] * extension;

    vec3 c = delimiter->position;
    vec3 p0 = c - u - v;
    vec3 p1 = c - u + v;
    vec3 p2 = c + u - v;
    vec3 p3 = c + u + v;
    delimiter->clipping_triangles.add({ p0, p1, p3, n });
    delimiter->clipping_triangles.add({ p0, p2, p3, n });
}

void World::create_octree() {
    tmFunction(TM_WORLD_COLOR);

    //
    // Insert all delimiters.
    //
    for(auto &delimiter : this->delimiters) {
        Octree *octree = this->root.get_octree_for_aabb(delimiter.aabb, this->allocator);
        assert(octree && "Delimiter Object is outside of octree bounds.");
        octree->contained_delimiters.add(&delimiter);
    }

    //
    // Insert all anchors.
    //
    for(auto &anchor : this->anchors) {
        AABB anchor_aabb = { anchor.position, anchor.position };
        Octree *octree = this->root.get_octree_for_aabb(anchor_aabb, this->allocator);
        assert(octree && "Delimiter Object is outside of octree bounds.");
        octree->contained_anchors.add(&anchor);
    }
}

void World::clip_delimiters(b8 single_step) {
    tmFunction(TM_WORLD_COLOR);

    if(this->dbg_step_completed) return;

    if(!this->dbg_step_initialized) {
        this->dbg_step_delimiter         = 0;
        this->dbg_step_clipping_triangle = 0;
        this->dbg_step_root_triangle     = 0;
        this->dbg_step_initialized       = true;
        this->dbg_step_completed         = false;
        this->dbg_step_clipping_triangle_should_be_removed = false;
        if(single_step) return;
    }

    while(this->dbg_step_delimiter < this->delimiters.count) {
        auto *delimiter = &this->delimiters[this->dbg_step_delimiter];

        while(this->dbg_step_clipping_triangle < delimiter->clipping_triangles.count) { // We are adding to this array in tessellate() and removing triangles inside this loop!
            auto *delimiter_triangle = &delimiter->clipping_triangles[this->dbg_step_clipping_triangle];

            //
            // Clip this delimiter against the root volume.
            // The root triangles should act more like planes, and tessellate everything that clips
            // their plane, not just their triangle face.
            //
            while(this->dbg_step_root_triangle < this->root_clipping_triangles.count) {
                auto *root_triangle = &this->root_clipping_triangles[this->dbg_step_root_triangle];

                tessellate(delimiter_triangle, root_triangle, &delimiter->clipping_triangles, true); // Clip against the root plane, and not just the root triangle.
                                                                                                          this->dbg_step_clipping_triangle_should_be_removed |= delimiter_triangle->is_fully_behind_plane(root_triangle);

                ++this->dbg_step_root_triangle;

                if(single_step) return;
            }

            if(this->dbg_step_clipping_triangle_should_be_removed) {
                delimiter->clipping_triangles.remove(this->dbg_step_clipping_triangle);
            } else {
                ++this->dbg_step_clipping_triangle;
            }

            this->dbg_step_root_triangle = 0;
            this->dbg_step_clipping_triangle_should_be_removed = false;

            if(single_step) return;
        }

        this->dbg_step_clipping_triangle = 0;
        ++this->dbg_step_delimiter;
        if(single_step) return;
    }

    this->dbg_step_completed = true;
}



/* ---------------------------------------------- Random Utility ---------------------------------------------- */

real get_random_real_uniform(real low, real high) {
#if CORE_SINGLE_PRECISION
    return get_random_f32_uniform(low, high);
#else
    return get_random_f64_uniform(low, high);
#endif
}
