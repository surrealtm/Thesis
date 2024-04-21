#include "timing.h"
#include "os_specific.h"
#include "threads.h" // For thread_sleep. nocheckin

#include "core.h"

#define DEBUG_PRINT false // nocheckin

/* ----------------------------------------------- 3D Geometry ----------------------------------------------- */

void Triangle::calculate_normal() {
    this->n = v3_normalize(v3_cross_v3(this->p0 - this->p1, this->p0 - this->p2));
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

f32 triangle_surface_area_approximation(v3f p0, v3f p1, v3f p2) {
    // https://math.stackexchange.com/questions/128991/how-to-calculate-the-area-of-a-3d-triangle
    v3f h = v3_cross_v3(p0 - p1, p0 - p2);
    return v3_length2(h) / 2;
}



/* ------------------------------------------------- Objects ------------------------------------------------- */

void add_triangles_for_clipping_plane(Resizable_Array<Triangle> * triangles, Clipping_Plane *plane) {
    for(auto *in : plane->triangles) {
        triangles->add({ in->p0, in->p1, in->p2, in->n });
    }
}

b8 Anchor::clip_corner_against_triangle(v3f p0, v3f p1, v3f p2, v3f n, Triangle *clipping_triangle, Clip_Result *result) {
    f32 dot = v3_dot_v3(p0 - clipping_triangle->p0, clipping_triangle->n);

    if(dot >= -F32_EPSILON) { // Prevent the issue when dot is -0.0000...
        // We are on the good side of the triangle, so there's no need to clip anything here.
        result->vertices[result->vertex_count] = p0;
        ++result->vertex_count;
        return false;
    }
    
    b8 triangle_was_shifted = false;
    
    v3f flipped_normal = -clipping_triangle->n; // ray_plane_intersection only checks for collisions that go "into" the plane (meaning the opposite normal direction), we however want the opposite here. @@Speed.

    v3f e1 = p1 - p0;
    f32 d1;
    b8 i1 = ray_plane_intersection(p0, e1, clipping_triangle->p0, flipped_normal, &d1) && d1 < 1.f; // If d1 > 1, then there is an intersection but beyond the actual edge.

    v3f e2 = p2 - p0;
    f32 d2;
    b8 i2 = ray_plane_intersection(p0, e2, clipping_triangle->p0, flipped_normal, &d2) && d2 < 1.0f; // If d2 > 1, then there is an intersection but beyond the actual edge.
    
    if(i1 && d1 < 1.f) {
        // This edge intersects with the clipping triangle. Add the intersection to the clip result.
        result->vertices[result->vertex_count] = p0 + e1 * d1;
        ++result->vertex_count;
    }

    if(i2 && d2 < 1.f) {
        // This edge intersects with the clipping triangle. Add the intersection to the clip result.
        result->vertices[result->vertex_count] = p0 + e2 * d2;
        ++result->vertex_count;        
    }

    if(!(i1 || i2)) {
        // If we are on the bad side of the clipping plane, and neither edge actually intersects with
        // the plane (consider parallel triangles, shifted on the Y axis), then project the current
        // corner onto the plane. 
        
        /*
        // Note that we don't do the "usual" projection here, but instead we
        // sort of "shift" the vertices to be on the plane.
        f32 distance;
        b8 intersection = ray_plane_intersection(p0, n, clipping_triangle->p0, clipping_triangle->n, &distance);
        result->vertices[result->vertex_count] = p0 + distance * n;
        ++result->vertex_count;
        */

        f32 distance = point_plane_distance_signed(p0, clipping_triangle->p0, clipping_triangle->n);
        result->vertices[result->vertex_count] = p0 - distance * clipping_triangle->n;
        ++result->vertex_count;

        triangle_was_shifted = true;
    }

    return triangle_was_shifted;
}

b8 Anchor::clip_triangle_against_triangle(Triangle *clipping_triangle, Triangle *volume_triangle) {
    // Don't clip against back-facing triangle. This probably isn't what we want to do in the long run...
    if(v3_dot_v3(clipping_triangle->n, this->position - clipping_triangle->p0) <= 0.0f) return false;
    
    Clip_Result clip_result;
    clip_result.vertex_count = 0;

    //
    // Clip each corner point of the volume triangle against the clip triangle.
    // This will check whether the point is on the "good" side of the triangle (meaning on the
    // side towards which the clip normal points) or not. If the corner is on the "bad" side,
    // it means we must clip this part of the triangle away. We do that by calculating the two
    // intersection points between the edges this corner is connected to and the clip triangle.
    // 
    //    |\    < ----   This corner must be clipped, so get the two intersections between the
    //    | \            edges and the clip triangle (marked by the '*').
    //    |  \
    // ***|***\************
    //    |    \    |
    //    |-----\   v
    // 
    // After we have clipped all corner points, we have between 3 and 6 output vertices, which we then
    // triangulate again. This way, we have ensured that this triangle got clipped into the correct
    // shape.
    //

#if DEBUG_PRINT
        printf("Clipping triangle: %f, %f, %f | %f, %f, %f | %f, %f, %f | %f, %f, %f.\n", 
               clipping_triangle->p0.x, clipping_triangle->p0.y, clipping_triangle->p0.z,
               clipping_triangle->p1.x, clipping_triangle->p1.y, clipping_triangle->p1.z,
               clipping_triangle->p2.x, clipping_triangle->p2.y, clipping_triangle->p2.z,
               clipping_triangle->n.x, clipping_triangle->n.y, clipping_triangle->n.z);

        printf("Volume triangle:   %f, %f, %f | %f, %f, %f | %f, %f, %f.\n", 
               volume_triangle->p0.x, volume_triangle->p0.y, volume_triangle->p0.z,
               volume_triangle->p1.x, volume_triangle->p1.y, volume_triangle->p1.z,
               volume_triangle->p2.x, volume_triangle->p2.y, volume_triangle->p2.z);
#endif

    // Note: The order in which the edges are checked is important, so that our triangulation works
    // correctly! Essentially, we want to create the clip results in clockwise order, so that we can
    // just iterate over the vertices and get good triangles.
    //
    // p0                p1
    // |---c1------c2-----/
    // c0            ----/
    // |      ---c3-/
    // c5 -c4--/
    // |/
    // p2

    b8 triangle_was_shifted = false;

    triangle_was_shifted |= this->clip_corner_against_triangle(volume_triangle->p0, volume_triangle->p2, volume_triangle->p1, volume_triangle->n, clipping_triangle, &clip_result);
    triangle_was_shifted |= this->clip_corner_against_triangle(volume_triangle->p1, volume_triangle->p0, volume_triangle->p2, volume_triangle->n, clipping_triangle, &clip_result);
    triangle_was_shifted |= this->clip_corner_against_triangle(volume_triangle->p2, volume_triangle->p1, volume_triangle->p0, volume_triangle->n, clipping_triangle, &clip_result);
            
    assert(clip_result.vertex_count >= 3 && clip_result.vertex_count <= 6);
            
    //
    // Subdivide this triangle. For now, just do the stupid thing and connect the different vertices.
    // This may lead to non-optimal triangles (stretched or whatever), but eh.
    //

    // Reuse this triangle to avoid unnecessary (de-) allocations.
    volume_triangle->p0 = clip_result.vertices[0];
    volume_triangle->p1 = clip_result.vertices[1];
    volume_triangle->p2 = clip_result.vertices[2];
    volume_triangle->calculate_normal();

    // Add new triangles for the new vertices.
    for(s64 i = 3; i < clip_result.vertex_count; ++i) {
        Triangle *triangle = this->volume.push();
        triangle->p0 = clip_result.vertices[0];
        triangle->p1 = clip_result.vertices[i - 1];
        triangle->p2 = clip_result.vertices[i];
        triangle->calculate_normal();
    }

#if DEBUG_PRINT
    printf("Clip result:       ");

    for(s64 i = 0; i < clip_result.vertex_count; ++i) {
        printf("%f, %f, %f ", clip_result.vertices[i].x, clip_result.vertices[i].y, clip_result.vertices[i].z);
        if(i + 1 < clip_result.vertex_count) printf("| ");
    }

    printf(".\n");
    //thread_sleep(0.1f);
#endif

    return triangle_was_shifted;
}

void Anchor::eliminate_dead_triangles() {
    tmFunction(TM_ANCHOR_COLOR);

    for(s64 i = 0; i < this->volume.count;) {
        Triangle *triangle = &this->volume[i];
        if(triangle_surface_area_approximation(triangle->p0, triangle->p1, triangle->p2) < F32_EPSILON) {
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

    auto *clipping_plane = &this->root_clipping_planes[0];
    add_triangles_for_clipping_plane(triangles, clipping_plane);

    clipping_plane = &this->root_clipping_planes[1];
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

void World::calculate_volumes(b8 single_step) {
    tmFunction(TM_WORLD_COLOR);

    this->dbg_step_anchor_index            = 0;
    this->dbg_step_boundary_index          = 0;
    this->dbg_step_clipping_plane_index    = 0;
    this->dbg_step_clipping_triangle_index = 0;
    this->dbg_step_volume_triangle_index   = 0;
    this->did_step_before                  = false;
    this->calculate_volumes_step(single_step);
}

void World::calculate_volumes_step(b8 single_step) {
    if(single_step && this->did_step_before) goto continue_from_single_step_exit;
    this->did_step_before = true;
    
    for(; this->dbg_step_anchor_index < this->anchors.count; ++this->dbg_step_anchor_index) {
        this->make_root_volume(&this->anchors[this->dbg_step_anchor_index].volume);

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
