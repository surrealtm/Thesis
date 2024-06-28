#include "world.h"
#include "tessel.h"
#include "floodfill.h"
#include "march.h"

#include "os_specific.h"
#include "timing.h"
#include "random.h"
#include "math/intersect.h"
#include "sort.h"



/* ------------------------------------------- Intersection Testing ------------------------------------------- */

struct Delimiter_Intersection {
    real total_distance; // The (squared) distance from this point to d0 + The (squared) distance from this point to d1.
    Delimiter *d0, *d1;
    Triangulated_Plane *p0, *p1;
};

// Adapted from: check_against_triangle in tessel.cpp
static
b8 check_edge_against_triangle(vec3 e0, vec3 e1, vec3 n, Triangle &triangle, vec3 o0, vec3 o1, real &nearest_distance) {
    vec3 direction = e1 - e0;

    Triangle_Intersection_Result<real> result = ray_double_sided_triangle_intersection(e0, direction, triangle.p0, triangle.p1, triangle.p2);

    if(!result.intersection || result.distance < 0.f || result.distance > 1.f) return false; // If the distance is not between 0 and 1, then the intersection is outside of the actual edge.

    //
    // When we find an intersection, we want a heuristic for the "distance" of that intersection
    // to the origin points, so that we can order different intersections based on which have
    // the highest priority (where a shorter distance means a higher priority, because the designer
    // would expect this intersection to happen "before" the other ones).
    //
    // We base this distance on a sort of 2D projection of the planes, because the intersection point
    // might be anywhere on the triangles, depending on which edges we check etc. Imagine the following
    // scenario though (top down view):
    //
    // |------> x
    // |
    // |  |--*-------    <- z = 0
    // v  |  |
    // y  |  *-------    <- z = -10
    //    |  |
    //
    // If we just naively took the distance between the intersection point and the origin centers, then
    // the intersection with z = 0 might be considered closer than the one with z = -10, which isn't what
    // the designer would expect. Therefore, to achieve this "top-down" effect, we cross the two triangles'
    // normals to get an "Up"-vector, and take out that "Up"-vector in the deltas (in this case, ignoring
    // the Z-axis).
    //
    vec3 intersection = e0 + direction * result.distance;
    vec3 _cross = v3_normalize(v3_cross_v3(n, triangle.n));    
    vec3 delta0 = (o0 - intersection) - (o0 - intersection) * _cross;
    vec3 delta1 = (o1 - intersection) - (o1 - intersection) * _cross;

    real distance = v3_length2(delta0) + v3_length2(delta1);
    
    if(distance < nearest_distance) nearest_distance = distance;

    return true;
}

static
void find_intersections(Delimiter *d0, Delimiter *d1, Resizable_Array<Delimiter_Intersection> &intersections) {
    // @@Speed: First check if the Triangulated_Planes have an intersection. If not, then there cannot be any
    // intersection between the actual triangles.
    for(s64 i = 0; i < d0->plane_count; ++i) {
        Triangulated_Plane &p0 = d0->planes[i];

        for(s64 j = 0; j < d1->plane_count; ++j) {
            Triangulated_Plane &p1 = d1->planes[j];
            b8 intersection = false;
            real distance = MAX_F32;

            // @@Speed: Early exit.
            
            for(Triangle &t0 : p0.triangles) {
                for(Triangle &t1 : p1.triangles) {
                    intersection |= check_edge_against_triangle(t0.p0, t0.p1, t0.n, t1, d0->position, d1->position, distance);
                    intersection |= check_edge_against_triangle(t0.p1, t0.p2, t0.n, t1, d0->position, d1->position, distance);
                    intersection |= check_edge_against_triangle(t0.p2, t0.p0, t0.n, t1, d0->position, d1->position, distance);
                    intersection |= check_edge_against_triangle(t1.p0, t1.p1, t1.n, t0, d1->position, d0->position, distance);
                    intersection |= check_edge_against_triangle(t1.p1, t1.p2, t1.n, t0, d1->position, d0->position, distance);
                    intersection |= check_edge_against_triangle(t1.p2, t1.p0, t1.n, t0, d1->position, d0->position, distance);
                }
            }

            if(intersection) {
                intersections.add({ distance, d0, d1, &p0, &p1 });
            }
        }
    }
}

static
Sort_Comparison_Result compare_delimiter_intersections(Delimiter_Intersection *lhs, Delimiter_Intersection *rhs) {
    return (lhs->total_distance < rhs->total_distance) ? SORT_Lhs_Is_Smaller : (lhs->total_distance > rhs->total_distance ? SORT_Lhs_Is_Bigger : SORT_Lhs_Equals_Rhs);
}

static
b8 delimiter_triangle_should_be_clipped(Triangle *generated_triangle, Triangle *clip_triangle, Delimiter *owning_delimiter) {
    //
    // When clipping generated triangles while solving delimiter intersections, we
    // don't want to generate triangles that are on the other side of the clipping
    // triangle than the origin (since that clipping triangle should exactly make this
    // part of the input triangle disappear).
    // Therefore, we check if the generated triangle is on the side of the clipping triangle
    // towards the owning delimiter's origin. For that, we need to ensure the correct normal
    // on the clip triangle (since the "actual" normal doesn't matter, just that the normal
    // faces towards the delimiter origin).
    //
    Triangle adjusted_clip_triangle = *clip_triangle;
    if(v3_dot_v3(adjusted_clip_triangle.n, owning_delimiter->position - adjusted_clip_triangle.p0) < 0.) {
        adjusted_clip_triangle.n = -adjusted_clip_triangle.n;
    }
    
    b8 should_be_clipped = generated_triangle->all_points_in_front_of_plane(&adjusted_clip_triangle); // We need to check this, because if the two intersection points are not on the edges of the generated triangles, then we might generate triangles which are partially behind the clipping plane as intended.
    
    return should_be_clipped;
}

static
void clip_all_delimiter_triangles(Resizable_Array<Triangle> &triangles_to_clip, Resizable_Array<Triangle> &clipping_triangles, Delimiter *owning_delimiter) {
    //
    // First up, we tessellate all triangles on intersection, so that no triangle
    // intersects with any clipping triangle anymore. We might not find any of these
    // triangles, if the clipping Triangulated_Plane has been modified by some other
    // intersection so that it actually isn't intersecting with us anymore.
    // If there is at least one intersection though, we want to remove all triangles
    // behind the clip.
    // Note that we only do this if there has been at least one intersection, take
    // the following scenario:
    //
    //    v- Don't clip these triangles here which are behind the right vertical one.
    //   |----------
    //   |
    //   |  |-------
    //   |  |
    //
    for(s64 i = 0; i < triangles_to_clip.count; ++i) {
        for(s64 j = 0; j < clipping_triangles.count; ++j) {
            Triangle *t0 = &triangles_to_clip.data[i]; // This pointer might not be stable if we are growing triangles_to_clip a lot due to tessellation!
            Triangle *t1 = &clipping_triangles[j];
            tessellate(t0, t1, &triangles_to_clip, false, (Triangle_Should_Be_Clipped) delimiter_triangle_should_be_clipped, owning_delimiter);
        }
    }
}
    
static
void solve_delimiter_intersection(World *world, Delimiter_Intersection *intersection) {
    //
    // When solving this intersection, we need to remember the original d0 clipping triangles,
    // so that we can then clip d0, and later on intersect d1 with the original d0 triangles.
    // This needs to happen because the triangles t0 that would clip and remove the triangles
    // t1 might not exist anymore after they have been clipped, which would lead to unexpected
    // results.
    //
    Resizable_Array<Triangle> original_t0s = intersection->p0->triangles.copy(world->allocator); // @@Speed: Only copy this if we actually need it later. @@Speed: Maybe even start using a temp allocator for this.

    // Clip d0 based on the triangles of d1.
    if(intersection->d0->level >= intersection->d1->level) clip_all_delimiter_triangles(intersection->p0->triangles, intersection->p1->triangles, intersection->d0);

    // Clip d1 based on the original triangles of d0.
    if(intersection->d1->level >= intersection->d0->level) clip_all_delimiter_triangles(intersection->p1->triangles, original_t0s, intersection->d1);

    original_t0s.clear();
}



/* -------------------------------------------------- World -------------------------------------------------- */

void World::create(vec3 half_size) {
    tmFunction(TM_WORLD_COLOR);

    //
    // Set up memory management.
    //
    this->arena.create(1 * ONE_GIGABYTE);
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
    anchor->triangles.allocator = this->allocator;

    return anchor;
}

Anchor *World::add_anchor(string dbg_name, vec3 position) {
    Anchor *anchor = this->add_anchor(position);
    anchor->dbg_name = copy_string(this->allocator, dbg_name);
    return anchor;
}

Delimiter *World::add_delimiter(vec3 position, vec3 half_size, vec3 rotation, u8 level) {
    tmFunction(TM_WORLD_COLOR);

    quat quaternion = qt_from_euler_turns(rotation);

    Delimiter *delimiter                 = this->delimiters.push();
    delimiter->position                  = position;
    delimiter->local_scaled_axes[AXIS_X] = qt_rotate(quaternion, vec3(half_size.x, 0, 0));
    delimiter->local_scaled_axes[AXIS_Y] = qt_rotate(quaternion, vec3(0, half_size.y, 0));
    delimiter->local_scaled_axes[AXIS_Z] = qt_rotate(quaternion, vec3(0, 0, half_size.z));
    delimiter->local_unit_axes[AXIS_X]   = qt_rotate(quaternion, vec3(1, 0, 0));
    delimiter->local_unit_axes[AXIS_Y]   = qt_rotate(quaternion, vec3(0, 1, 0));
    delimiter->local_unit_axes[AXIS_Z]   = qt_rotate(quaternion, vec3(0, 0, 1));
    delimiter->plane_count               = 0;
    delimiter->level                     = level;
    
    delimiter->dbg_half_size = half_size;
    delimiter->dbg_rotation  = quaternion;

    return delimiter;
}

Delimiter *World::add_delimiter(string dbg_name, vec3 position, vec3 half_size, vec3 rotation, u8 level) {
    Delimiter *delimiter = this->add_delimiter(position, half_size, rotation, level);
    delimiter->dbg_name = copy_string(this->allocator, dbg_name);
    return delimiter;
}

void World::add_delimiter_clipping_planes(Delimiter *delimiter, Axis normal_axis, Virtual_Extension virtual_extension) {
    tmFunction(TM_WORLD_COLOR);

    assert(normal_axis >= 0 && normal_axis < AXIS_COUNT);
    assert(delimiter->plane_count + 2 <= ARRAY_COUNT(delimiter->planes));

    real virtual_extension_scale = max(max(this->half_size.x, this->half_size.y), this->half_size.z) * 2.f; // Uniformly scale the clipping plane to cover the entire world space, before it will probably get cut down later.

    Axis u_axis = (Axis) ((normal_axis + 1) % 3);
    Axis v_axis = (Axis) ((normal_axis + 2) % 3);
    
    vec3 a = delimiter->local_scaled_axes[normal_axis];
    vec3 n = delimiter->local_unit_axes[normal_axis];
    vec3 u = delimiter->local_unit_axes[u_axis];
    vec3 v = delimiter->local_unit_axes[v_axis];

    real left_extension   = virtual_extension & VIRTUAL_EXTENSION_Negative_U ? virtual_extension_scale : v3_length(delimiter->local_scaled_axes[u_axis]);
    real right_extension  = virtual_extension & VIRTUAL_EXTENSION_Positive_U ? virtual_extension_scale : v3_length(delimiter->local_scaled_axes[u_axis]);
    real top_extension    = virtual_extension & VIRTUAL_EXTENSION_Negative_V ? virtual_extension_scale : v3_length(delimiter->local_scaled_axes[v_axis]);
    real bottom_extension = virtual_extension & VIRTUAL_EXTENSION_Positive_V ? virtual_extension_scale : v3_length(delimiter->local_scaled_axes[v_axis]);

    //
    // A delimiter owns an actual volume, which means that the clipping plane
    // shouldn't actually go through the center, but be aligned with the sides
    // of this volume. This, in turn, means that there should actually be two
    // clipping planes, one on each side of the axis.
    //

    Triangulated_Plane *p0 = &delimiter->planes[delimiter->plane_count];
    p0->setup(this->allocator, delimiter->position + a,  n, -u * left_extension, u * right_extension, -v * top_extension, v * bottom_extension);
    ++delimiter->plane_count;
    
    Triangulated_Plane *p1 = &delimiter->planes[delimiter->plane_count];
    p1->setup(this->allocator, delimiter->position - a, -n, -u * left_extension, u * right_extension, -v * top_extension, v * bottom_extension);
    ++delimiter->plane_count;
}

void World::add_centered_delimiter_clipping_plane(Delimiter *delimiter, Axis normal_axis, Virtual_Extension virtual_extension) {
    tmFunction(TM_WORLD_COLOR);

    assert(normal_axis >= 0 && normal_axis < AXIS_COUNT);
    assert(delimiter->plane_count + 1 <= ARRAY_COUNT(delimiter->planes));

    real virtual_extension_scale = max(max(this->half_size.x, this->half_size.y), this->half_size.z) * 2.f; // Uniformly scale the clipping plane to cover the entire world space, before it will probably get cut down later.
    
    Axis u_axis = (Axis) ((normal_axis + 1) % 3);
    Axis v_axis = (Axis) ((normal_axis + 2) % 3);
    
    vec3 a = delimiter->local_scaled_axes[normal_axis];
    vec3 n = delimiter->local_unit_axes[normal_axis];
    vec3 u = delimiter->local_unit_axes[(normal_axis + 1) % 3];
    vec3 v = delimiter->local_unit_axes[(normal_axis + 2) % 3];
    
    real left_extension   = virtual_extension & VIRTUAL_EXTENSION_Negative_U ? virtual_extension_scale : v3_length(delimiter->local_scaled_axes[u_axis]);
    real right_extension  = virtual_extension & VIRTUAL_EXTENSION_Positive_U ? virtual_extension_scale : v3_length(delimiter->local_scaled_axes[u_axis]);
    real top_extension    = virtual_extension & VIRTUAL_EXTENSION_Negative_V ? virtual_extension_scale : v3_length(delimiter->local_scaled_axes[v_axis]);
    real bottom_extension = virtual_extension & VIRTUAL_EXTENSION_Positive_V ? virtual_extension_scale : v3_length(delimiter->local_scaled_axes[v_axis]);
    
    Triangulated_Plane *p0 = &delimiter->planes[delimiter->plane_count];
    p0->setup(this->allocator, delimiter->position, n, -u * left_extension, u * right_extension, -v * top_extension, v * bottom_extension);
    ++delimiter->plane_count;
}

void World::create_bvh() {
    tmFunction(TM_WORLD_COLOR);

}

void World::clip_delimiters(b8 single_step) {
    tmFunction(TM_WORLD_COLOR);

    {
        //
        // Find all intersections between any two delimiters and store them.
        //
        Resizable_Array<Delimiter_Intersection> intersections;
        intersections.allocator = this->allocator;

        for(s64 i = 0; i < this->delimiters.count; ++i) {
            for(s64 j = i + 1; j < this->delimiters.count; ++j) {
                find_intersections(&this->delimiters[i], &this->delimiters[j], intersections);
            }
        }

        //
        // Sort the intersections array.
        //
        sort(intersections.data, intersections.count, compare_delimiter_intersections);

        //
        // Solve all the intersections by clipping the two delimiters against
        // each other (if that intersection is actually still present, but that
        // is handled by the tessellation).
        //
        for(Delimiter_Intersection &all : intersections) {
            solve_delimiter_intersection(this, &all);
        }

        intersections.clear();
    }
    
    {
        //
        // Clip all delimiter planes against the root clipping planes, so that the delimiters
        // don't unnecessarily extend beyond the world borders.
        // This probably isn't actually necessary since we don't care about them being outside
        // the world boundaries (I think?), but it makes for a better visualiazation.
        //
        // @@Speed:
        // We might just take this out for performance reasons, or move it until after the delimiters
        // have been clipped against each other, which may be less work in total if more triangles
        // have already been clipped?
        //
        for(Delimiter &delimiter : this->delimiters) {
            for(s64 i = 0; i < delimiter.plane_count; ++i) {
                Triangulated_Plane *plane = &delimiter.planes[i];
                for(s64 j = 0; j < plane->triangles.count;) { // We are modifying this array in the loop!
                    Triangle *delimiter_triangle = &plane->triangles[j];
                    b8 delimiter_triangle_should_be_clipped = false;

                    for(Triangle &root_triangle : this->root_clipping_triangles) {
                        tessellate(delimiter_triangle, &root_triangle, &plane->triangles, true);
                        delimiter_triangle_should_be_clipped |= delimiter_triangle->all_points_in_front_of_plane(&root_triangle); // @@Speed: Early exit. @@Speed: We could pass a custom check to the tessellator for this, to prevent the creation of triangles we know are out of bounds anyway? (We still need to check this triangle though, since it won't get removed even if it is out of bounds...)
                    }

                    if(delimiter_triangle_should_be_clipped) {
                        plane->triangles.remove(j);
                    } else {
                        ++j;
                    }
                }
            }
        }
    }
}

void World::calculate_volumes() {
    tmFunction(TM_WORLD_COLOR);

    // @@Ship: Remove this.
    Hardware_Time calculation_start = os_get_hardware_time();
    Hardware_Time anchor_start = calculation_start;
    s64 anchor_index = 0;
    
    // @@Speed: Start reusing the Flood_Fill struct (i.e. the allocated cell field, etc.), instead of doing
    // so fucking much memory allocation all the time.
    for(Anchor &anchor : this->anchors) {
        if(this->current_flood_fill != null) deallocate_flood_fill(this->current_flood_fill);

        this->current_flood_fill = this->allocator->New<Flood_Fill>();
        floodfill(this->current_flood_fill, this, this->allocator, anchor.position);

        marching_cubes(&anchor.triangles, this->current_flood_fill);

        // @@Ship: Remove this
        ++anchor_index;

        Hardware_Time now = os_get_hardware_time();
        if(os_convert_hardware_time(now - calculation_start, Seconds) > 1) {
            printf("Calculated volume for anchor %" PRId64 " of %" PRId64 " (total: %fs, this anchor: %fs)\n", anchor_index, this->anchors.count, os_convert_hardware_time(now - calculation_start, Seconds), os_convert_hardware_time(now - anchor_start, Seconds));
            anchor_start = now;
        }
    }
}

b8 World::cast_ray_against_delimiters_and_root_planes(vec3 origin, vec3 direction, real distance) {
    tmFunction(TM_WORLD_COLOR);

    for(Triangle &triangle : this->root_clipping_triangles) {
        auto result = ray_double_sided_triangle_intersection(origin, direction, triangle.p0, triangle.p1, triangle.p2);
        if(result.intersection && result.distance >= 0.f && result.distance <= distance) { // distance isn't normalized!
            return true;
        }        
    }
    
    // @@Speed: This can be massively improved.
    // 1. First up, start using the bvh to figure out which triangles should actually be checked (since that
    //    number is going wayyy down).
    // 2. Then, cast a single ray against the entire Triangulated_Plane before trying to intersect the individual
    //    triangles. Again, we should be able to cull out a lot of compuation like this.
    for(Delimiter &delimiter : this->delimiters) {
        for(s64 i = 0; i < delimiter.plane_count; ++i) {
            Triangulated_Plane *plane = &delimiter.planes[i];
            for(Triangle &triangle : plane->triangles) {
                auto result = ray_double_sided_triangle_intersection(origin, direction, triangle.p0, triangle.p1, triangle.p2);
                if(result.intersection && result.distance >= 0.f && result.distance <= distance) { // distance isn't normalized!
                    return true;
                }
            }
        }
    }
    
    return false;
}



/* ---------------------------------------------- Random Utility ---------------------------------------------- */

real get_random_real_uniform(real low, real high) {
#if CORE_SINGLE_PRECISION
    return get_random_f32_uniform(low, high);
#else
    return get_random_f64_uniform(low, high);
#endif
}
