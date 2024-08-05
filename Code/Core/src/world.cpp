#include "world.h"
#include "bvh.h"
#include "tessel.h"
#include "floodfill.h"
#include "march.h"
#include "assembler.h"

#include "os_specific.h"
#include "timing.h"
#include "random.h"
#include "math/intersect.h"
#include "sort.h"



/* ------------------------------------------- Intersection Testing ------------------------------------------- */

struct Delimiter_Intersection {
    real distance;
    Delimiter *d0, *d1;
    Triangulated_Plane *p0, *p1;
};

struct Delimiter_Triangle_Should_Be_Clipped_Helper {
    vec3 center_to_clip;
    vec3 clip_normal;
};

// Adapted from: check_against_triangle in tessel.cpp
static
b8 check_edge_against_triangle(World *world, vec3 c0, vec3 c1, Triangle &triangle, Triangulated_Plane *p0, Triangulated_Plane *p1, real &nearest_distance) {
    vec3 direction = c1 - c0;

    Triangle_Intersection_Result<real> result = ray_double_sided_triangle_intersection(c0, direction, triangle.p0, triangle.p1, triangle.p2);

    if(!result.intersection || result.distance < 0.f || result.distance > 1.f) return false; // If the distance is not between 0 and 1, then the intersection is outside of the actual edge.

    real this_distance = fabs(point_plane_distance_signed(p0->o, p1->o, p1->n)) + fabs(point_plane_distance_signed(p1->o, p0->o, p0->n));

    if(this_distance < nearest_distance) nearest_distance = this_distance;

    return true;
}

static
void find_intersections(World *world, Delimiter *d0, Delimiter *d1, Triangulated_Plane *p0, Triangulated_Plane *p1, Resizable_Array<Delimiter_Intersection> &intersections) {
    // @@Speed: First check if the Triangulated_Planes have an intersection. If not, then there cannot be any
    // intersection between the actual triangles.
    b8 intersection = false;
    real distance = MAX_F32;

    // We need to find the smallest distance for correct intersection resolution here, so we always need
    // to check all triangles.
    for(Triangle &t0 : p0->triangles) {
        for(Triangle &t1 : p1->triangles) {
            intersection |= check_edge_against_triangle(world, t0.p0, t0.p1, t1, p0, p1, distance);
            intersection |= check_edge_against_triangle(world, t0.p1, t0.p2, t1, p0, p1, distance);
            intersection |= check_edge_against_triangle(world, t0.p2, t0.p0, t1, p0, p1, distance);
            intersection |= check_edge_against_triangle(world, t1.p0, t1.p1, t0, p0, p1, distance);
            intersection |= check_edge_against_triangle(world, t1.p1, t1.p2, t0, p0, p1, distance);
            intersection |= check_edge_against_triangle(world, t1.p2, t1.p0, t0, p0, p1, distance);
        }
    }

    if(intersection) {
        intersections.add({ distance, d0, d1, p0, p1 });
    }
}

static
void find_intersections(World *world, Delimiter *d0, Delimiter *d1, Resizable_Array<Delimiter_Intersection> &intersections) {
    for(s64 i = 0; i < d0->plane_count; ++i) {
        for(s64 j = 0; j < d1->plane_count; ++j) {
            find_intersections(world, d0, d1, &d0->planes[i], &d1->planes[j], intersections);
        }
    }
}

static inline
Sort_Comparison_Result compare_distances(real lhs, real rhs) {
    if(lhs < rhs) return SORT_Lhs_Is_Smaller;
    if(lhs > rhs) return SORT_Lhs_Is_Bigger;
    return SORT_Lhs_Equals_Rhs;
}

static
Sort_Comparison_Result compare_delimiter_intersections(Delimiter_Intersection *lhs, Delimiter_Intersection *rhs) {   
    return compare_distances(lhs->distance, rhs->distance);
}

static
vec3 get_adjusted_clip_normal(vec3 input_normal, vec3 center_to_clip, vec3 clip_point) {
    if(v3_dot_v3(input_normal, center_to_clip - clip_point) < 0.) {
        input_normal = -input_normal;
    }

    return input_normal;
}

static
vec3 get_adjusted_clip_normal(Triangulated_Plane *clipping_plane, vec3 center_to_clip) {
    return get_adjusted_clip_normal(clipping_plane->n, center_to_clip, clipping_plane->triangles[0].p0);
}

static
b8 delimiter_triangle_should_be_clipped(Triangle *generated_triangle, Triangle *clip_triangle, Delimiter_Triangle_Should_Be_Clipped_Helper *helper) {
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
    vec3 adjusted_clip_normal = get_adjusted_clip_normal(helper->clip_normal, helper->center_to_clip, clip_triangle->p0);

    b8 should_be_clipped = generated_triangle->no_point_in_front_of_plane(clip_triangle->p0, adjusted_clip_normal); // We need to check this, because if the two intersection points are not on the edges of the generated triangles, then we might generate triangles which are partially behind the clipping plane as intended.
    
    return should_be_clipped;
}

static
b8 tessellate_all_triangles(Resizable_Array<Triangle> &triangles_to_clip, Resizable_Array<Triangle> &clipping_triangles, vec3 clip_normal, b8 clip_against_plane, b8 clip_triangles_behind, vec3 center_to_clip = vec3(0)) {
    Delimiter_Triangle_Should_Be_Clipped_Helper helper;
    helper.center_to_clip = center_to_clip;
    helper.clip_normal = clip_normal;

    b8 any_intersection = false;
    
    for(s64 i = 0; i < triangles_to_clip.count; ) {       
        b8 increase_i = true;

        for(s64 j = 0; j < clipping_triangles.count; ++j) {
            Triangle *t0 = &triangles_to_clip[i]; // This pointer might not be stable if we are growing triangles_to_clip a lot due to tessellation!
            Triangle *t1 = &clipping_triangles[j];

            Tessellation_Result result;
            if(clip_triangles_behind) {
                result = tessellate(t0, t1, clip_normal, &triangles_to_clip, clip_against_plane, (Triangle_Should_Be_Clipped) delimiter_triangle_should_be_clipped, &helper);
            } else {
                result = tessellate(t0, t1, clip_normal, &triangles_to_clip, clip_against_plane);
            }

            any_intersection |= result != TESSELLATION_No_Intersection;

            if(result == TESSELLATION_Intersection_But_No_Triangles) {
                triangles_to_clip.remove(i); // All would-be triangles were rejected, so remove the input one and go on.
                increase_i = false;
                break;
            }
        }

        if(increase_i) ++i;
    }

    return any_intersection;
}

static
void remove_all_triangles_behind_plane(Resizable_Array<Triangle> &triangles_to_clip, Resizable_Array<Triangle> &plane_triangles, vec3 plane_normal) {
    for(s64 i = 0; i < triangles_to_clip.count; ) {
        Triangle *t0 = &triangles_to_clip[i];
        b8 should_remove_triangle = false;

        // @@Speed: Isn't a single no_point_before_plane enough here? Since that check does not actually depend
        // on the vertices (clue is in the name plane dude).
        for(s64 j = 0; j < plane_triangles.count; ++j) {
            Triangle *t1 = &plane_triangles[j];

            if(!t0->no_point_behind_plane(t1->p0, plane_normal)) {
                should_remove_triangle = true;
                break;
            }
        }

        if(should_remove_triangle) {
            triangles_to_clip.remove(i);
        } else {
            ++i;
        }
    }
}

static
void solve_delimiter_intersection(World *world, Delimiter_Intersection *intersection) {
    //
    // :OriginalDelimiterTriangles
    // When solving this intersection, we need to remember the original d0 clipping triangles,
    // so that we can then clip d0, and later on intersect d1 with the original d0 triangles.
    // This needs to happen because the triangles t0 that would clip and remove the triangles
    // t1 might not exist anymore after they have been clipped, which would lead to unexpected
    // results.
    //
    Resizable_Array<Triangle> original_t0s = intersection->p0->triangles.copy(world->allocator);
    
    // Clip d0 based on the triangles of d1.
    b8 clip_p0_triangles   = intersection->d0 != intersection->d1 && intersection->d0->level >= intersection->d1->level;
    b8 any_p0_intersection = tessellate_all_triangles(intersection->p0->triangles, intersection->p1->triangles, intersection->p1->n, false, clip_p0_triangles, intersection->d0->position);
    if(clip_p0_triangles && any_p0_intersection) remove_all_triangles_behind_plane(intersection->p0->triangles, intersection->p1->triangles, get_adjusted_clip_normal(intersection->p1, intersection->d0->position));

    // Clip d1 based on the original triangles of d0.
    b8 clip_p1_triangles   = intersection->d0 != intersection->d1 && intersection->d1->level >= intersection->d0->level;
    b8 any_p1_intersection = tessellate_all_triangles(intersection->p1->triangles, original_t0s, intersection->p0->n, false, clip_p1_triangles, intersection->d1->position);
    if(clip_p1_triangles && any_p1_intersection) remove_all_triangles_behind_plane(intersection->p1->triangles, original_t0s, get_adjusted_clip_normal(intersection->p0, intersection->d1->position));
    
    original_t0s.clear();
}



/* ---------------------------------------------- Volume Query ---------------------------------------------- */

static
b8 point_inside_volume(Resizable_Array<Triangle> &triangles, vec3 point) {
    s64 count = 0;

    vec3 ray_origin = point;
    vec3 ray_direction = vec3(0, -1, 0);
    
    for(Triangle triangle : triangles) {
        auto triangle_result = ray_double_sided_triangle_intersection(ray_origin, ray_direction, triangle.p0, triangle.p1, triangle.p2);
        if(!triangle_result.intersection) continue;
        if(triangle_result.distance < -CORE_EPSILON) continue;
        if(triangle_result.distance < CORE_EPSILON) return true; // Point is exactly on the triangle, which we consider inside the volume.
        ++count;        
    }
    
    return count % 2 == 1;
}



/* ------------------------------------------ Volume Calculation Job ------------------------------------------ */

struct Volume_Calculation_Job {
    World *world;
    s64 first;
    s64 last;
    real cell_world_space_size;
};

static
void volume_calculation_job(Volume_Calculation_Job *job) {
    tmFunction(TM_WORLD_COLOR);
    
    Flood_Fill ff;
    create_flood_fill(&ff, job->world, job->world->allocator, job->cell_world_space_size);

    for(s64 i = job->first; i <= job->last; ++i) {
        Anchor &anchor = job->world->anchors[i];
        floodfill(&ff, anchor.position);

#if USE_MARCHING_CUBES_FOR_VOLUMES
        marching_cubes(&anchor.volume, &ff);
#else
        assemble(&anchor.volume, job->world, &ff);
#endif
    }

    destroy_flood_fill(&ff);
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

    create_temp_allocator(64 * ONE_MEGABYTE);
    
#if USE_JOB_SYSTEM
    create_job_system(&this->job_system, os_get_number_of_hardware_threads());
#endif
    
    //
    // Set up the basic objects.
    //
    this->half_size = half_size;
    this->anchors.allocator    = this->allocator;
    this->delimiters.allocator = this->allocator;
    
    //
    // Create the clipping planes.
    //
    {       
        // X-Axis
        this->root_clipping_planes[0].create(this->allocator, vec3(-this->half_size.x, 0, 0), vec3(0, this->half_size.y, 0), vec3(0, 0, this->half_size.z));
        this->root_clipping_planes[1].create(this->allocator, vec3(+this->half_size.x, 0, 0), vec3(0, 0, this->half_size.z), vec3(0, this->half_size.y, 0));

        // Y-Axis
        this->root_clipping_planes[2].create(this->allocator, vec3(0, -this->half_size.y, 0), vec3(0, 0, this->half_size.z), vec3(this->half_size.x, 0, 0));
        this->root_clipping_planes[3].create(this->allocator, vec3(0, +this->half_size.y, 0), vec3(this->half_size.x, 0, 0), vec3(0, 0, this->half_size.z));

        // Z-Axis
        this->root_clipping_planes[4].create(this->allocator, vec3(0, 0, -this->half_size.z), vec3(this->half_size.x, 0, 0), vec3(0, this->half_size.y, 0));
        this->root_clipping_planes[5].create(this->allocator, vec3(0, 0, +this->half_size.z), vec3(0, this->half_size.y, 0), vec3(this->half_size.x, 0, 0));
    }
}

void World::destroy() {
    this->arena.destroy();

#if USE_JOB_SYSTEM
    destroy_job_system(&this->job_system, JOB_SYSTEM_Detach_Workers);
#endif
}

void World::reserve_objects(s64 anchors, s64 delimiters) {
    this->anchors.reserve(anchors);
    this->delimiters.reserve(delimiters);
}

Anchor *World::add_anchor(vec3 position) {
    Anchor *anchor   = this->anchors.push();
    anchor->id       = this->anchors.count - 1;
    anchor->position = position;
    anchor->volume.allocator = this->allocator;

    return anchor;
}

Anchor *World::add_anchor(string dbg_name, vec3 position) {
    Anchor *anchor = this->add_anchor(position);
    anchor->dbg_name = copy_string(this->allocator, dbg_name);
    return anchor;
}

Delimiter *World::add_delimiter(vec3 position, vec3 half_size, vec3 rotation, u8 level) {
    return this->add_delimiter(position, half_size, qt_from_euler_turns(rotation), level);
}

Delimiter *World::add_delimiter(vec3 position, vec3 half_size, quat rotation, u8 level) {
    Delimiter *delimiter                 = this->delimiters.push();
    delimiter->id                        = this->delimiters.count - 1;
    delimiter->position                  = position;
    delimiter->local_scaled_axes[AXIS_X] = qt_rotate(rotation, vec3(half_size.x, 0, 0));
    delimiter->local_scaled_axes[AXIS_Y] = qt_rotate(rotation, vec3(0, half_size.y, 0));
    delimiter->local_scaled_axes[AXIS_Z] = qt_rotate(rotation, vec3(0, 0, half_size.z));
    delimiter->local_unit_axes[AXIS_X]   = qt_rotate(rotation, vec3(1, 0, 0));
    delimiter->local_unit_axes[AXIS_Y]   = qt_rotate(rotation, vec3(0, 1, 0));
    delimiter->local_unit_axes[AXIS_Z]   = qt_rotate(rotation, vec3(0, 0, 1));
    delimiter->plane_count               = 0;
    delimiter->level                     = level;
    
    delimiter->dbg_half_size = half_size;
    delimiter->dbg_rotation  = rotation;

    return delimiter;
}

Delimiter *World::add_delimiter(string dbg_name, vec3 position, vec3 half_size, vec3 rotation, u8 level) {
    Delimiter *delimiter = this->add_delimiter(position, half_size, rotation, level);
    delimiter->dbg_name = copy_string(this->allocator, dbg_name);
    return delimiter;
}

Delimiter *World::add_delimiter(string dbg_name, vec3 position, vec3 half_size, quat rotation, u8 level) {
    Delimiter *delimiter = this->add_delimiter(position, half_size, rotation, level);
    delimiter->dbg_name = copy_string(this->allocator, dbg_name);
    return delimiter;
}

void World::add_delimiter_plane(Delimiter *delimiter, Axis_Index normal_axis, b8 centered, Virtual_Extension virtual_extension) {
    assert(normal_axis >= 0 && normal_axis < AXIS_SIGNED_COUNT);
    assert(delimiter->plane_count + 1 <= ARRAY_COUNT(delimiter->planes));

    real virtual_extension_scale = max(max(this->half_size.x, this->half_size.y), this->half_size.z) * 2.f; // Uniformly scale the clipping plane to cover the entire world space, before it will probably get cut down later.
    
    Axis_Index n_axis = (Axis_Index) ((normal_axis + 0) % AXIS_COUNT);
    Axis_Index u_axis = (Axis_Index) ((normal_axis + 1) % AXIS_COUNT);
    Axis_Index v_axis = (Axis_Index) ((normal_axis + 2) % AXIS_COUNT);

    vec3 a = delimiter->local_scaled_axes[n_axis] * axis_sign(normal_axis);
    vec3 n = delimiter->local_unit_axes[n_axis]   * axis_sign(normal_axis);
    vec3 u = delimiter->local_unit_axes[u_axis];
    vec3 v = delimiter->local_unit_axes[v_axis];
    
    vec3 forward_extension = +(centered != false ? 0 : a);
    vec3 left_extension    = -(virtual_extension & VIRTUAL_EXTENSION_Negative_U ? u * virtual_extension_scale : delimiter->local_scaled_axes[u_axis]);
    vec3 right_extension   = +(virtual_extension & VIRTUAL_EXTENSION_Positive_U ? u * virtual_extension_scale : delimiter->local_scaled_axes[u_axis]);
    vec3 top_extension     = -(virtual_extension & VIRTUAL_EXTENSION_Negative_V ? v * virtual_extension_scale : delimiter->local_scaled_axes[v_axis]);
    vec3 bottom_extension  = +(virtual_extension & VIRTUAL_EXTENSION_Positive_V ? v * virtual_extension_scale : delimiter->local_scaled_axes[v_axis]);
    
    Triangulated_Plane *p0 = &delimiter->planes[delimiter->plane_count];
    p0->create(this->allocator, delimiter->position + forward_extension, n, left_extension, right_extension, top_extension, bottom_extension);
    ++delimiter->plane_count;
}

void World::add_both_delimiter_planes(Delimiter *delimiter, Axis_Index normal_axis, Virtual_Extension virtual_extension) {
    this->add_delimiter_plane(delimiter, normal_axis, false, virtual_extension);
    this->add_delimiter_plane(delimiter, (Axis_Index) (normal_axis + AXIS_COUNT), false, virtual_extension);
}

void World::calculate_volumes(real cell_world_space_size) {
    this->clip_delimiters();
    this->create_bvh();
    this->build_anchor_volumes(cell_world_space_size);
}

Anchor *World::query(vec3 point) {
    //
    // For now, just be stupid and query every single volume by doing a ray cast against every
    // single triangle of the volume.
    //
    for(Anchor &all : this->anchors) {
        if(point_inside_volume(all.volume, point)) return &all;
    }
    
    return null;
}


void World::create_bvh() {
    tmFunction(TM_WORLD_COLOR);

    this->bvh = this->allocator->New<BVH>();
    this->bvh->create(this->allocator);

    for(Triangulated_Plane &root_plane : this->root_clipping_planes) {
        for(Triangle &root_triangle : root_plane.triangles) {
            this->bvh->add(root_triangle);
        }
    }
    
    for(Delimiter &delimiter : this->delimiters) {
        for(s64 i = 0; i < delimiter.plane_count; ++i) {
            Triangulated_Plane &plane = delimiter.planes[i];
            for(Triangle &triangle : plane.triangles) {
                this->bvh->add(triangle);
            }
        }
    }
    
    this->bvh->subdivide();
}

void World::create_bvh_from_triangles(Resizable_Array<Triangle> &triangles) {
    tmFunction(TM_WORLD_COLOR);

    this->bvh = this->allocator->New<BVH>();
    this->bvh->create(this->allocator);

    for(Triangle &triangle : triangles) this->bvh->add(triangle);
    
    this->bvh->subdivide();
}

void World::clip_delimiters() {
    tmFunction(TM_WORLD_COLOR);

    u64 temp_mark = mark_temp_allocator();
    
    {
        //
        // Find all intersections between any two delimiters and store them.
        //
        Resizable_Array<Delimiter_Intersection> intersections;
        intersections.allocator = &temp;

        for(s64 i = 0; i < this->delimiters.count; ++i) {
            Delimiter *d0 = &this->delimiters[i];
            
            // Find intersections between clipping planes of the same delimiter, which can happen
            // if a delimiter has planes on different axis. These planes will only tessellate each
            // other (so that we can properly assemble anchor volumes), but they will not clip
            // each other.
            for(s64 j = 0; j < d0->plane_count; ++j) {
                for(s64 k = j + 1; k < d0->plane_count; ++k) {
                    find_intersections(this, d0, d0, &d0->planes[j], &d0->planes[k], intersections);
                }
            }

            // Find intersections with all other delimiters. Only check delimiters after this one in
            // the array to avoid duplicates.
            for(s64 j = i + 1; j < this->delimiters.count; ++j) {
                Delimiter *d1 = &this->delimiters[j];
                find_intersections(this, d0, d1, intersections);
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
        // Clip and tessellate all delimiter planes against the root clipping planes, and tessellate the root
        // clipping planes, so that they can be used in the assembling stage later on.
        //
        for(Delimiter &delimiter : this->delimiters) {
            for(s64 i = 0; i < delimiter.plane_count; ++i) {
                Triangulated_Plane *delimiter_plane = &delimiter.planes[i];
                for(Triangulated_Plane &root_plane : this->root_clipping_planes) {
                    // :OriginalDelimiterTriangles
                    Resizable_Array<Triangle> original_t0s = delimiter_plane->triangles.copy(&temp);
                    
                    tessellate_all_triangles(delimiter_plane->triangles, root_plane.triangles, root_plane.n, true, true, delimiter.position);
                    remove_all_triangles_behind_plane(delimiter_plane->triangles, root_plane.triangles, root_plane.n);

                    tessellate_all_triangles(root_plane.triangles, original_t0s, delimiter_plane->n, false, false);
                }
            }
        }
    }

    release_temp_allocator(temp_mark);
}

void World::build_anchor_volumes(real cell_world_space_size) {
    tmFunction(TM_WORLD_COLOR);

    u64 temp_mark = mark_temp_allocator();

#if USE_JOB_SYSTEM    
    // Set up the different jobs. Each anchor takes so long to calculate that it's probably worth it making
    // every single one a single job.
    s64 job_count = this->anchors.count;
    Volume_Calculation_Job *jobs = (Volume_Calculation_Job *) this->allocator->allocate(job_count * sizeof(Volume_Calculation_Job));

    s64 prev_last = -1;
    for(s64 i = 0; i < job_count; ++i) {
        s64 first = prev_last + 1;
        s64 last  = (i + 1 == job_count) ? (this->anchors.count - 1) : first + (this->anchors.count / job_count) - 1;
        jobs[i].world = this;
        jobs[i].first = first;
        jobs[i].last  = last;
        jobs[i].cell_world_space_size = cell_world_space_size;
        prev_last = last;

    }

    // Spawn the jobs building the actual anchors
    for(s64 i = 0; i < job_count; ++i) {
        spawn_job(&this->job_system, { (Job_Procedure) volume_calculation_job, &jobs[i] });
    }
                  
    // Wait for the jobs to complete
    wait_for_all_jobs(&this->job_system);    
#else
    Volume_Calculation_Job job;
    job.world = this;
    job.first = 0;
    job.last  = this->anchors.count - 1;
    job.cell_world_space_size = cell_world_space_size;
    volume_calculation_job(&job);
#endif

    release_temp_allocator(temp_mark);
}

b8 World::point_inside_bounds(vec3 point) {
    return point.x >= -this->half_size.x && point.x <= +this->half_size.x &&
        point.y >= -this->half_size.y && point.y <= +this->half_size.y &&
        point.z >= -this->half_size.z && point.z <= +this->half_size.z;
}

b8 World::cast_ray_against_delimiters_and_root_planes(vec3 ray_origin, vec3 ray_direction, real max_ray_distance) {
    const b8 early_return = true; // @@Ship: Remove this parameter from all procedures if possible.
    
#if USE_BVH_FOR_RAYCASTS
    auto result = this->bvh->cast_ray(ray_origin, ray_direction, max_ray_distance, early_return);
    return result.hit_something;
#else
    b8 hit_something = false;

    for(Triangulated_Plane &root_plane : this->root_clipping_planes) {
        if(root_plane.cast_ray(ray_origin, ray_direction, max_ray_distance, early_return)) {
            hit_something = true;
            if(early_return) goto early_exit;
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
            if(plane->cast_ray(ray_origin, ray_direction, max_ray_distance, early_return)) {
                hit_something = true;
                if(early_return) goto early_exit;
            }
        }
    }

 early_exit:
    return hit_something;
#endif
}



/* ---------------------------------------------- Random Utility ---------------------------------------------- */

real get_random_real_uniform(real low, real high) {
#if CORE_SINGLE_PRECISION
    return default_random_generator.random_f32(low, high);
#else
    return default_random_generator.random_f64(low, high);
#endif
}
