#include "timing.h"
#include "core.h"



/* ----------------------------------------------- 3D Geometry ----------------------------------------------- */

void Triangle::calculate_normal() {
    this->n = v3_normalize(v3_cross_v3(this->p0 - this->p1, this->p0 - this->p2));
    assert(fabs(v3_length2(this->n) - 1) < F32_EPSILON);
}

f32 Triangle::calculate_surface_area() {
    // https://math.stackexchange.com/questions/128991/how-to-calculate-the-area-of-a-3d-triangle
    v3f h = v3_cross_v3(this->p0 - this->p1, this->p0 - this->p2);
    return v3_length2(h) / 2;
}

b8 Triangle::is_dead() {
    return this->calculate_surface_area() <= F32_EPSILON;
}

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

void add_triangles_for_clipping_plane(Resizable_Array<Triangle> * triangles, Clipping_Plane *plane) {
    for(auto *in : plane->triangles) {
        triangles->add({ in->p0, in->p1, in->p2, in->n });
    }
}



/* ------------------------------------------------- Objects ------------------------------------------------- */

b8 Anchor::shift_corner_onto_triangle(v3f p0, v3f n, Triangle *clipping_triangle, Clip_Result *result) {
    f32 dot = v3_dot_v3(p0 - clipping_triangle->p0, clipping_triangle->n);

    if(dot >= -0.f) { // Prevent the issue when dot is -0.0000...
        //
        // We are on the good side of the triangle, so there's no need to shift anything here.
        //
        return false;
    }

    //
    // Project the current corner onto the plane based on the volume triangle's normal.. This ensures that
    // the volume triangle doesn't lose surface area, but is "bent" onto the clipping plane.
    //
    // \       ^
    //  --\    |
    // ****\*********    <- The left corner of this triangle needs to be shifted upwards to meet the diagonal
    //      --\             plane. If we didn't do this, then the surface area of the left part of the
    //         \            volume triangle would be lost.
    //
    // Note that we don't do the "usual" projection here, but instead we sort of "shift" the vertices along
    // the volume face normal to be on the plane.
    //
    // An intersection like this only happens if the two normals are in a less-than-90 degree angle to each other.
    // If the angle is bigger, then we just want to clip the volume triangle.
    //

    v3f flipped_normal = -clipping_triangle->n; // ray_plane_intersection only checks for collisions that go "into" the plane (meaning the opposite normal direction), we however want the opposite here. @@Speed.

    f32 distance;
    b8 intersection = ray_plane_intersection(p0, n, clipping_triangle->p0, flipped_normal, &distance);
    if(intersection) {
        result->vertices[result->vertex_count] = p0 + distance * n;
        ++result->vertex_count;
    }

    return intersection;
}

u8 Anchor::clip_corner_against_triangle(v3f p0, v3f p1, v3f p2, Triangle *clipping_triangle, Clip_Result *result) {
    v3f delta = p0 - clipping_triangle->p0;
    f32 dot = v3_dot_v3(delta, clipping_triangle->n);

    if(dot >= -0.f) { // Prevent the issue when dot is -0.0000...
        //
        // We are on the good side of the triangle, so there's no need to clip anything here.
        //
        result->vertices[result->vertex_count] = p0;
        ++result->vertex_count;
        return 1;
    }

    printf("Dot: %f, Delta: %f, %f, %f\n", dot, delta.x, delta.y, delta.z);
    
    //
    // This corner point must be clipped. Figure out the clip points, which are the intersections of the edges
    // outgoing from this corner with the clipping triangles.
    // Depending on the triangles, there may be no intersections. This can happen if the volume triangle
    // is parallel to the clipping one, in which case we will just shift the triangle later on, and no clipping
    // needs to happen here.
    //
    
    v3f flipped_normal = -clipping_triangle->n; // ray_plane_intersection only checks for collisions that go "into" the plane (meaning the opposite normal direction), we however want the opposite here. @@Speed.

    v3f e1 = p1 - p0;
    f32 d1;
    b8 i1 = ray_plane_intersection(p0, e1, clipping_triangle->p0, flipped_normal, &d1) && d1 > 0.0f && d1 < 1.0f;

    v3f e2 = p2 - p0;
    f32 d2;
    b8 i2 = ray_plane_intersection(p0, e2, clipping_triangle->p0, flipped_normal, &d2) && d2 > 0.0f && d2 < 1.0f;

    u8 vertex_count = 0;

    printf("I1: %d, I2: %d, D1: %f, D2: %f\n", i1, i2, d1, d2);
    
    if(i1) {
        // This edge intersects with the clipping triangle. Add the intersection to the clip result.
        result->vertices[result->vertex_count] = p0 + e1 * d1;
        ++result->vertex_count;
        ++vertex_count;
    }

    if(i2) {
        // This edge intersects with the clipping triangle. Add the intersection to the clip result.
        result->vertices[result->vertex_count] = p0 + e2 * d2;
        ++result->vertex_count;
        ++vertex_count;
    }
    
    return vertex_count;
}

b8 Anchor::clip_triangle_against_triangle(Triangle *clipping_triangle, Triangle *volume_triangle) {
    // Don't clip against back-facing triangle. This probably isn't what we want to do in the long run...
    if(v3_dot_v3(clipping_triangle->n, this->position - clipping_triangle->p0) <= 0.0f) return false;

    // This triangle is dead and will be eliminated later on. No point in trying to clip anything here,
    // it would just cause a headache dealing with invalid normals.
    if(volume_triangle->is_dead()) return false;
    
    printf("Clipping Volume triangle: %f, %f, %f  |  %f, %f, %f  |  %f, %f, %f\n", volume_triangle->p0.x, volume_triangle->p0.y, volume_triangle->p0.z, volume_triangle->p1.x, volume_triangle->p1.y, volume_triangle->p1.z, volume_triangle->p2.x, volume_triangle->p2.y, volume_triangle->p2.z);
    printf(" Using Clipping triangle: %f, %f, %f  |  %f, %f, %f  |  %f, %f, %f\n", clipping_triangle->p0.x, clipping_triangle->p0.y, clipping_triangle->p0.z, clipping_triangle->p1.x, clipping_triangle->p1.y, clipping_triangle->p1.z, clipping_triangle->p2.x, clipping_triangle->p2.y, clipping_triangle->p2.z);
    
    Clip_Result clip_result;
    clip_result.vertex_count = 0;

    //
    // Clip each corner point of the volume triangle against the clip triangle.
    // This will check whether the point is on the "good" side of the triangle (meaning on the
    // side towards which the clip normal points) or not. If the corner is on the "bad" side,
    // it means we must clip this part of the triangle.
    //
    
    b8 triangle_requires_reevaluation = false;

    clip_result.p0_count = this->clip_corner_against_triangle(volume_triangle->p0, volume_triangle->p1, volume_triangle->p2, clipping_triangle, &clip_result);
    clip_result.p2_count = this->clip_corner_against_triangle(volume_triangle->p2, volume_triangle->p0, volume_triangle->p1, clipping_triangle, &clip_result);
    clip_result.p1_count = this->clip_corner_against_triangle(volume_triangle->p1, volume_triangle->p2, volume_triangle->p0, clipping_triangle, &clip_result);

    clip_result.p0_shifted = this->shift_corner_onto_triangle(volume_triangle->p0, volume_triangle->n, clipping_triangle, &clip_result);
    clip_result.p2_shifted = this->shift_corner_onto_triangle(volume_triangle->p2, volume_triangle->n, clipping_triangle, &clip_result);
    clip_result.p1_shifted = this->shift_corner_onto_triangle(volume_triangle->p1, volume_triangle->n, clipping_triangle, &clip_result);
    
    assert(clip_result.vertex_count >= 3 && clip_result.vertex_count <= 9); // Make sure we got a valid amount of clip points.
    
    //
    // Subdivide this triangle.
    // First up, connect all the edge intersections. All these lie on the "original" triangle plane,
    // since they are somewhere on the edges of the original plane.
    //

    s64 edge_clip_vertices = clip_result.p0_count + clip_result.p1_count + clip_result.p2_count;
    
    // Reuse this triangle to avoid unnecessary (de-) allocations.
    volume_triangle->p0 = clip_result.vertices[0];
    volume_triangle->p1 = clip_result.vertices[1];
    volume_triangle->p2 = clip_result.vertices[2];

    // Add new triangles for the new vertices.
    for(s64 i = 3; i < edge_clip_vertices; ++i) {
        this->volume.add({ clip_result.vertices[0], clip_result.vertices[i - 1], clip_result.vertices[i], volume_triangle->n });
    }

    //
    // Now, create new triangles to connect the shifted-up clipping points, if necessary.
    // These are orientented differently, as the shifted points no longer lie on the original volume plane.
    // Therefore, we also need to calculate the new normal vector for these.
    //
    if(clip_result.p0_shifted || clip_result.p1_shifted || clip_result.p2_shifted) {
        s64 shifted_vertices = clip_result.vertex_count - edge_clip_vertices;

        if(clip_result.vertex_count > 3) {
            // If there are more than 3 vertices, then there must've been one one edge-clip. If there has been at
            // least one edge-clip, then at least one corner must've been on the good side of the triangle and
            // was therefore not shifted.
            s64 p0_index = 0;
            s64 p1_index = clip_result.p0_count + clip_result.p2_count;
            s64 p2_index = clip_result.p0_count;
            
            if(clip_result.p0_shifted && clip_result.p1_shifted) {
                assert(clip_result.p0_count == 1 && clip_result.p1_count == 1 && shifted_vertices == 2);
                this->volume.add({ clip_result.vertices[p0_index], clip_result.vertices[p1_index], clip_result.vertices[edge_clip_vertices] });
                this->volume.add({ clip_result.vertices[p0_index], clip_result.vertices[edge_clip_vertices], clip_result.vertices[edge_clip_vertices + 1] });
            } else if(clip_result.p0_shifted && clip_result.p2_shifted) {
                assert(clip_result.p0_count == 1 && clip_result.p2_count == 1 && shifted_vertices == 2);
                this->volume.add({ clip_result.vertices[p0_index], clip_result.vertices[p2_index], clip_result.vertices[edge_clip_vertices] });
                this->volume.add({ clip_result.vertices[p0_index], clip_result.vertices[edge_clip_vertices], clip_result.vertices[edge_clip_vertices + 1] });
            } else if(clip_result.p1_shifted && clip_result.p2_shifted) {
                assert(clip_result.p1_count == 1 && clip_result.p2_count == 1 && shifted_vertices == 2);
                this->volume.add({ clip_result.vertices[p1_index], clip_result.vertices[p2_index], clip_result.vertices[edge_clip_vertices] });
                this->volume.add({ clip_result.vertices[p1_index], clip_result.vertices[edge_clip_vertices], clip_result.vertices[edge_clip_vertices + 1] });
            } else if(clip_result.p0_shifted) {
                assert(clip_result.p0_count == 2 && shifted_vertices == 1);
                this->volume.add({ clip_result.vertices[p0_index], clip_result.vertices[p0_index + 1], clip_result.vertices[edge_clip_vertices] });
            } else if(clip_result.p1_shifted) {
                assert(clip_result.p1_count == 2 && shifted_vertices == 1);
                this->volume.add({ clip_result.vertices[p1_index], clip_result.vertices[p1_index + 1], clip_result.vertices[edge_clip_vertices] });
            } else if(clip_result.p2_shifted) {
                assert(clip_result.p2_count == 2 && shifted_vertices == 1);
                this->volume.add({ clip_result.vertices[p2_index], clip_result.vertices[p2_index + 1], clip_result.vertices[edge_clip_vertices] });
            } else {
                assert(false);
            }
        } else {
            // If there were no edge clip vertices (e.g. parallel triangles), then we have already created our
            // triangle above, by assigning the first three clip result vertices to volume_triangle. We just
            // need to re-calculate the normal and we are good to go.
            assert(clip_result.vertex_count == 3);
            if(!volume_triangle->is_dead()) volume_triangle->calculate_normal(); // We may have created a dead triangle here, in which case no normal can be created. @@Speed.
        }
    }
    
    return triangle_requires_reevaluation;
}

void Anchor::eliminate_dead_triangles() {
    tmFunction(TM_ANCHOR_COLOR);

    for(s64 i = 0; i < this->volume.count;) {
        Triangle *triangle = &this->volume[i];
        if(triangle->is_dead()) {
            this->volume.remove(i);
        } else {
            ++i;
        }
    }
}

void Anchor::dbg_print_volume() {
    printf("---------------- ANCHOR '%.*s' ----------------\n", (u32) this->dbg_name.count, this->dbg_name.data);

    for(auto *triangle : this->volume) {
        printf(" { %f, %f, %f,    %f, %f, %f,    %f, %f, %f },\n", triangle->p0.x, triangle->p0.y, triangle->p0.z, triangle->p1.x, triangle->p1.y, triangle->p1.z, triangle->p2.x, triangle->p2.y, triangle->p2.z);
    }

    printf("---------------- ANCHOR '%.*s' ----------------\n", (u32) this->dbg_name.count, this->dbg_name.data);
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

Anchor *World::add_anchor(string name, v3f position) {
    tmFunction(TM_WORLD_COLOR);

    Anchor *anchor   = this->anchors.push();
    anchor->position = position;
    anchor->volume.allocator = this->allocator;
    anchor->dbg_name = copy_string(this->allocator, name);
    
    return anchor;
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
    boundary->clipping_planes.add( { this->allocator, boundary->position + a,  n, u * extension, v * extension });
    boundary->clipping_planes.add( { this->allocator, boundary->position - a, -n, u * extension, v * extension });
}

void World::add_centered_boundary_clipping_plane(Boundary *boundary, Axis normal_axis) {
    tmFunction(TM_WORLD_COLOR);

    assert(normal_axis >= 0 && normal_axis < AXIS_COUNT);

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
    boundary->clipping_planes.add( { this->allocator, boundary->position,  n, u * extension, v * extension });
}

void World::make_root_volume(Resizable_Array<Triangle> *triangles) {
    tmFunction(TM_WORLD_COLOR);

    /*
    // nocheckin
    for(auto *clipping_plane : this->root_clipping_planes) {
        add_triangles_for_clipping_plane(triangles, clipping_plane);
    }
    */

    //add_triangles_for_clipping_plane(triangles, &this->root_clipping_planes[0]);
    add_triangles_for_clipping_plane(triangles, &this->root_clipping_planes[1]);
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
    this->dbg_step_clipping_plane_index    = 0;
    this->dbg_step_clipping_triangle_index = 0;
    this->dbg_step_volume_triangle_index   = 0;
    this->did_step_before                  = false;
    if(!single_step) this->calculate_volumes_step(single_step);
}

void World::calculate_volumes_step(b8 single_step) {
    if(single_step && this->did_step_before) goto continue_from_single_step_exit;
    this->did_step_before = true;

    for(; this->dbg_step_anchor_index < this->anchors.count; ++this->dbg_step_anchor_index) {
        if(!this->anchors[this->dbg_step_anchor_index].volume.count) this->make_root_volume(&this->anchors[this->dbg_step_anchor_index].volume); // Don't create the root volume in case this anchor has a precomputed base volume.

        for(; this->dbg_step_volume_triangle_index < this->anchors[this->dbg_step_anchor_index].volume.count; ++this->dbg_step_volume_triangle_index) { // We are modifying this volume array inside the loop!
            do {
                // nocheckin: Explain this
                this->dbg_step_triangle_requires_reevaluation = false;

                for(; this->dbg_step_boundary_index < this->boundaries.count; ++this->dbg_step_boundary_index) {
                    for(; this->dbg_step_clipping_plane_index < this->boundaries[this->dbg_step_boundary_index].clipping_planes.count; ++this->dbg_step_clipping_plane_index) {
                        for(; this->dbg_step_clipping_triangle_index < this->boundaries[this->dbg_step_boundary_index].clipping_planes[this->dbg_step_clipping_plane_index].triangles.count;) {
                            this->dbg_step_triangle_requires_reevaluation |= this->anchors[this->dbg_step_anchor_index].clip_triangle_against_triangle(&this->boundaries[this->dbg_step_boundary_index].clipping_planes[this->dbg_step_clipping_plane_index].triangles[this->dbg_step_clipping_triangle_index], &this->anchors[this->dbg_step_anchor_index].volume[this->dbg_step_volume_triangle_index]);
                            ++this->dbg_step_clipping_triangle_index;

                            if(single_step) return;
                                
                        continue_from_single_step_exit:
                            continue;
                        }

                        this->dbg_step_clipping_triangle_index = 0;
                    }

                    this->dbg_step_clipping_plane_index = 0;
                }

                this->dbg_step_boundary_index = 0;
            } while(this->dbg_step_triangle_requires_reevaluation);
        }

        this->anchors[this->dbg_step_anchor_index].eliminate_dead_triangles();
        this->dbg_step_volume_triangle_index = 0;
    }

    // If we actually finish the volume calcuation, then reset all these things.
    this->did_step_before                  = false;
    this->dbg_step_anchor_index            = MAX_S64;
    this->dbg_step_boundary_index          = MAX_S64;
    this->dbg_step_clipping_plane_index    = MAX_S64;
    this->dbg_step_clipping_triangle_index = MAX_S64;
    this->dbg_step_volume_triangle_index   = MAX_S64;
}
