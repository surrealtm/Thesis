#include "tessel.h"
#include "core.h"

#include "math/v2.h"
#include "math/intersect.h"
#include "timing.h"

#define TESSEL_DEBUG_PRINT false // @@Ship: Remove this.

struct Tessellator {
    // The corner points and normal of the input triangle, copied since we will modify
    // the input triangle to avoid (re-) allocations.
    vec3 input_corner[3];

    // Intermediate state keeping.
    vec3 intersection_point[2];
    s8 intersection_count;

    s8 far_intersection_point_index;
    s8 near_intersection_point_index;

    s8 extension_corner_index;
    s8 first_corner_index;
    s8 second_corner_index;

    real barycentric_coefficients[2][3]; // The barycentric coefficients of the intersection points in relation to the input corners.

    Triangle_Should_Be_Clipped triangle_should_be_clipped_proc;
    void *triangle_should_be_clipped_user_pointer;
    Triangle *input_triangle;
    Triangle *clip_triangle;
    Resizable_Array<Triangle> *output_array;
    
    // To know whether to add a new triangle or re-use the input one.
    s64 generated_triangle_count;
};


static inline
vec2 convert_barycentric_to_planar(Tessellator *tessellator, s64 intersection_point, s64 c0, s64 c1) {
    return vec2(tessellator->barycentric_coefficients[intersection_point][c0],
               tessellator->barycentric_coefficients[intersection_point][c1]);
}

static inline
vec3 get_barycentric_coefficient_for_corner_index(s64 index) {
    vec3 result;

    switch(index) {
    case 0: result = vec3(1, 0, 0); break;
    case 1: result = vec3(0, 1, 0); break;
    case 2: result = vec3(0, 0, 1); break;
    }

    return result;
}


static inline
b8 points_almost_identical(vec3 p0, vec3 p1) {
    return fabs(p0.x - p1.x) < CORE_EPSILON && fabs(p0.y - p1.y) < CORE_EPSILON && fabs(p0.z - p1.z) < CORE_EPSILON;
}

static inline
b8 far_point_inside_outer_triangle(Tessellator *tessellator, s64 c0, s64 c1, s64 oppposition) {
    //
    // To check if the far point is inside the triangle (near, c0, c1), we essentially map the
    // problem onto a 2D space (since we know all points lie on the triangle plane).
    // We do a regular 2D point-in-triangle check in the barycentric coordinate system.
    //

    vec2 f  = convert_barycentric_to_planar(tessellator, tessellator->far_intersection_point_index,  c0, c1);

    vec2 p0 = convert_barycentric_to_planar(tessellator, tessellator->near_intersection_point_index, c0, c1);
    vec2 p1 = vec2(1, 0);
    vec2 p2 = vec2(0, 1);

    // @Cut'N'Paste: From point_inside_triangle in intersect.cpp.
    {
#define sign(p0, p1, p2) ((p0.x - p2.x) * (p1.y - p2.y) - (p1.x - p2.x) * (p0.y - p2.y))

        real d0 = sign(f, p0, p1);
        real d1 = sign(f, p1, p2);
        real d2 = sign(f, p2, p0);

#undef sign

        // Handle cases in which the point lies on the edge of a triangle, in which case we may
        // get very small positive or negative numbers.
        b8 negative = (d0 < -CORE_SMALL_EPSILON) || (d1 < -CORE_SMALL_EPSILON) || (d2 < -CORE_SMALL_EPSILON);
        b8 positive = (d0 >  CORE_SMALL_EPSILON) || (d1 >  CORE_SMALL_EPSILON) || (d2 >  CORE_SMALL_EPSILON);

        return !(negative && positive);
    }
}

static inline
s8 get_closest_intersection_point_to_corner(Tessellator *tessellator, s64 point_index) {
    return tessellator->barycentric_coefficients[0][point_index] >= tessellator->barycentric_coefficients[1][point_index] ? 0 : 1;
}


static inline
void maybe_add_intersection_point(Tessellator *tessellator, vec3 point) {
    //
    // If a corner of the triangle lies on the clipping plane, we may get two intersection points
    // for that, one for each edge of this corner. We only want to add the point once here.
    // Doing something like 'distance > 0 && distance <= 1' (or 'distance >= 0 && distance < 1')
    // is fishy and adds too much numeric instability unfortunately...
    //
    if(tessellator->intersection_count >= 1 && points_almost_identical(point, tessellator->intersection_point[0])) return;
    if(tessellator->intersection_count >= 2 && points_almost_identical(point, tessellator->intersection_point[1])) return;

    assert(tessellator->intersection_count < 2); // A triangle-plane intersection can only have two (unique) intersection points.
    tessellator->intersection_point[tessellator->intersection_count] = point;
    ++tessellator->intersection_count;
}

static
void check_edge_against_triangle(Tessellator *tessellator, vec3 e0, vec3 e1, Triangle *triangle) {
    //
    // We check for a double-sided plane intersection here since we don't care about the triangle
    // orientation. We always want to tessellate the triangle, no matter whether the edge passed in
    // through the "forward" or "backward" side of the triangle.
    //

    vec3 direction = e1 - e0;

    Triangle_Intersection_Result<real> result = ray_double_sided_triangle_intersection(e0, direction, triangle->p0, triangle->p1, triangle->p2);

    // If result.distance < 0.f || result.distance > 1.f, then we do get an intersection with the
    // ray, but not inside the edge section of the ray.
    if(!result.intersection || result.distance <= -CORE_SMALL_EPSILON || result.distance >= 1.f + CORE_SMALL_EPSILON) return;

    maybe_add_intersection_point(tessellator, e0 + direction * result.distance);
}

static
void check_edge_against_plane(Tessellator *tessellator, vec3 e0, vec3 e1, Triangle *triangle) {
    //
    // We check for a double-sided plane intersection here since we don't care about the triangle
    // orientation. We always want to tessellate the triangle, no matter whether the edge passed in
    // through the "forward" or "backward" side of the triangle.
    //

    vec3 direction = e1 - e0;
    if(fabs(v3_dot_v3(direction, triangle->n)) < CORE_EPSILON) return; // @@Speed: This is done again in ray_double_sided_plane_intersection, but here we use CORE_EPSILON instead of F32_EPSILON, since F32_EPSILON is too small for our purposes here...

    real distance;
    b8 intersection = ray_double_sided_plane_intersection(e0, direction, triangle->p0, triangle->n, &distance);

    // If result.distance < 0.f || result.distance > 1.f, then we do get an intersection with the
    // ray, but not inside the edge section of the ray.
    if(!intersection || distance <= -CORE_SMALL_EPSILON || distance >= 1.f + CORE_SMALL_EPSILON) return;

    maybe_add_intersection_point(tessellator, e0 + direction * distance);
}

static
void generate_new_triangle(Tessellator *tessellator, vec3 p0, vec3 p1, vec3 p2) {
    //
    // Calculate the (estimated) surface area of the new triangle here. If it is too small, then consider it
    // dead and irrellevant to this algorithm.
    // While dead triangles shouldn't be problematic, they can slow us down a lot (if there are many of them)
    // without providing any benefit.
    // This can happen if an intersection point is on the edge of the input triangle (in which case all three
    // points are on one single line), or if two of the three points are very close to each other, etc.
    //
    if(points_almost_identical(p0, p1) || points_almost_identical(p0, p2) || points_almost_identical(p1, p2)) {
#if TESSEL_DEBUG_PRINT
        printf("    Rejected triangle due to points being identical.\n");
#endif
        return;
    }
        
    vec3 n = v3_cross_v3(p1 - p0, p2 - p0);

    real estimated_surface_area = v3_length2(n) / 2;
    if(estimated_surface_area < CORE_EPSILON) {
#if TESSEL_DEBUG_PRINT
        printf("    Rejected triangle due to estimated surface area being too small.\n");
#endif
        return;
    }

    //
    // Do some custom checking of whether this triangle should be generated at all. This heavily depends on
    // the usage of the tessellation output, and therefore a procedure pointer is provided.
    //
    Triangle would_be_triangle = { p0, p1, p2, tessellator->input_triangle->n };
    if(tessellator->triangle_should_be_clipped_proc && tessellator->triangle_should_be_clipped_proc(&would_be_triangle, tessellator->clip_triangle, tessellator->triangle_should_be_clipped_user_pointer)) {
#if TESSEL_DEBUG_PRINT
        printf("    Rejected triangle due to custom decider callback.\n");
#endif
        return;
    }
    
#if TESSEL_DEBUG_PRINT
    printf("    Generated triangle: %f, %f, %f | %f, %f, %f | %f, %f, %f\n", p0.x, p0.y, p0.z, p1.x, p1.y, p1.z, p2.x, p2.y, p2.z);
    printf("        Surface Area: %f\n", estimated_surface_area);
#endif

    if(tessellator->generated_triangle_count == 0) {
        // Re-use the input triangle to avoid re-allocations.
        tessellator->input_triangle->p0 = p0;
        tessellator->input_triangle->p1 = p1;
        tessellator->input_triangle->p2 = p2;
    } else {
        // Add a new triangle to the output array.
        tessellator->output_array->add({ p0, p1, p2, tessellator->input_triangle->n });
    }

    ++tessellator->generated_triangle_count;
}


s64 tessellate(Triangle *input, Triangle *clip, Resizable_Array<Triangle> *output, b8 clip_against_plane, Triangle_Should_Be_Clipped triangle_should_be_clipped_proc, void *triangle_should_be_clipped_user_pointer) {
    tmFunction(TM_TESSEL_COLOR);

    Tessellator tessellator;
    tessellator.input_corner[0] = input->p0;
    tessellator.input_corner[1] = input->p1;
    tessellator.input_corner[2] = input->p2;
    tessellator.input_triangle  = input;
    tessellator.clip_triangle   = clip;
    tessellator.output_array    = output;
    tessellator.triangle_should_be_clipped_proc = triangle_should_be_clipped_proc;
    tessellator.triangle_should_be_clipped_user_pointer = triangle_should_be_clipped_user_pointer;
    
    tessellator.intersection_count = 0;

    if(!clip_against_plane) {
        //
        // https://stackoverflow.com/questions/7113344/find-whether-two-triangles-intersect-or-not
        // Two triangles can intersect in the following two ways:
        //   1. Two edges of one triangle intersect the face of the other triangle.
        //   2. One edge of each triangle intersects the face of the other triangle.
        // In both cases, we get two edge-to-face intersections, meaning we got a total
        // of two intersection points. We then triangulate the input around these two
        // intersection points.
        //
        // If the two triangles don't intersect, then there is no need to tessellate here
        // and we are done.
        //
        check_edge_against_triangle(&tessellator, input->p0, input->p1, clip);
        check_edge_against_triangle(&tessellator, input->p1, input->p2, clip);
        check_edge_against_triangle(&tessellator, input->p2, input->p0, clip);
        check_edge_against_triangle(&tessellator, clip->p0, clip->p1, input);
        check_edge_against_triangle(&tessellator, clip->p1, clip->p2, input);
        check_edge_against_triangle(&tessellator, clip->p2, clip->p0, input);
    } else {
        //
        // A plane and a triangle intersect if two edges of the triangle intersect the
        // plane. We therefore also should get two intersection points (on intersection),
        // but we only need to check the input edges against the plane, since they cannot
        // "miss" the plane (since it is infinite), like they can miss a triangle in the
        // above case.
        //
        check_edge_against_plane(&tessellator, input->p0, input->p1, clip);
        check_edge_against_plane(&tessellator, input->p1, input->p2, clip);
        check_edge_against_plane(&tessellator, input->p2, input->p0, clip);
    }

    //
    // :TessellationOfCoplanarTriangles
    // While in theory there may only ever be 0 or 2 intersection points, we may experience some
    // numerical problems if an edge or even the entire triangle lies on the clipping plane. In this
    // case, we may get 1 or even three intersection points, depending on the instability. If that
    // happens, we know that the triangle lies on the clipping plane and therefore does not require
    // further tessellation.
    //
    if(tessellator.intersection_count != 2) return 0;

    calculate_barycentric_coefficients(tessellator.input_corner[0], tessellator.input_corner[1], tessellator.input_corner[2], tessellator.intersection_point[0], &tessellator.barycentric_coefficients[0][0], &tessellator.barycentric_coefficients[0][1], &tessellator.barycentric_coefficients[0][2]);
    calculate_barycentric_coefficients(tessellator.input_corner[0], tessellator.input_corner[1], tessellator.input_corner[2], tessellator.intersection_point[1], &tessellator.barycentric_coefficients[1][0], &tessellator.barycentric_coefficients[1][1], &tessellator.barycentric_coefficients[1][2]);

    /*
      assert(tessellator.barycentric_coefficients[0][0] >= 0.f && tessellator.barycentric_coefficients[0][0] <= 1.f);
      assert(tessellator.barycentric_coefficients[0][1] >= 0.f && tessellator.barycentric_coefficients[0][1] <= 1.f);
      assert(tessellator.barycentric_coefficients[0][2] >= 0.f && tessellator.barycentric_coefficients[0][2] <= 1.f);
      assert(tessellator.barycentric_coefficients[1][0] >= 0.f && tessellator.barycentric_coefficients[1][0] <= 1.f);
      assert(tessellator.barycentric_coefficients[1][1] >= 0.f && tessellator.barycentric_coefficients[1][1] <= 1.f);
      assert(tessellator.barycentric_coefficients[1][2] >= 0.f && tessellator.barycentric_coefficients[1][2] <= 1.f);
    */

    b8 intersection_point_is_corner[2] = {
        tessellator.barycentric_coefficients[0][0] >= 1. - CORE_EPSILON || tessellator.barycentric_coefficients[0][1] >= 1. - CORE_EPSILON || tessellator.barycentric_coefficients[0][2] >= 1. - CORE_EPSILON,
        tessellator.barycentric_coefficients[1][0] >= 1. - CORE_EPSILON || tessellator.barycentric_coefficients[1][1] >= 1. - CORE_EPSILON || tessellator.barycentric_coefficients[1][2] >= 1. - CORE_EPSILON
    };

    //
    // If both intersection points are very close to (or exactly on) the corners, then we don't bother
    // tessellation since there must already be an edge between the intersection points on the input
    // triangle (duh).
    // This can happen if this triangle here is the result of a previous tessellation, so two corners
    // are on a clipping plane. Due to precision errors, we might get intersections between those
    // corners, since the edge is on the clipping triangle...
    //
    if(intersection_point_is_corner[0] && intersection_point_is_corner[1]) return 0;
    
    //
    // So there are two intersection points on the triangle. We now want to tessellate the triangle
    // so that there is an edge connecting the two intersection points. This means that, depending
    // on where the intersection points are inside the triangle, we may need to split the triangle
    // up into 5 new ones. If an intersection point lies on the edge of the input triangle, then that
    // saves us one new triangle. If an intersection point lies on the corner of the input triangle,
    // that saves up two triangles.
    //

    //
    // To actually do the tessellation, we first need to find the "extension" corner of the input
    // triangle. This is the corner which most closely represents the extension of the intersection
    // edge. We use this corner to extend the intersection edge to the corner of the triangle, and
    // build triangles along these two edges.
    //

    vec3 intersection_edge = tessellator.intersection_point[0] - tessellator.intersection_point[1];
    s8 closest_intersection_point_index[3] = { get_closest_intersection_point_to_corner(&tessellator, 0),
                                               get_closest_intersection_point_to_corner(&tessellator, 1),
                                               get_closest_intersection_point_to_corner(&tessellator, 2) };

    vec3 corner_to_intersection[3] = { tessellator.input_corner[0] - tessellator.intersection_point[closest_intersection_point_index[0]],
                                       tessellator.input_corner[1] - tessellator.intersection_point[closest_intersection_point_index[1]],
                                       tessellator.input_corner[2] - tessellator.intersection_point[closest_intersection_point_index[2]] };

    // Calculate the angle between each corner to its nearest intersection point, and the intersection
    // edge. Normalize since we want to look at the angle between corner_to_intersection and
    // intersection_edge.
    real corner_extension_factor[3] = { real_abs(v3_dot_v3(corner_to_intersection[0], intersection_edge) / v3_length2(corner_to_intersection[0])),
                                        real_abs(v3_dot_v3(corner_to_intersection[1], intersection_edge) / v3_length2(corner_to_intersection[1])),
                                        real_abs(v3_dot_v3(corner_to_intersection[2], intersection_edge) / v3_length2(corner_to_intersection[2])) };

    tessellator.extension_corner_index = (corner_extension_factor[0] > corner_extension_factor[1]) ?
        (corner_extension_factor[0] > corner_extension_factor[2] ? 0 : 2) :
        (corner_extension_factor[1] > corner_extension_factor[2] ? 1 : 2);
    tessellator.first_corner_index     = (tessellator.extension_corner_index + 1) % 3;
    tessellator.second_corner_index    = (tessellator.extension_corner_index + 2) % 3;
    tessellator.near_intersection_point_index = closest_intersection_point_index[tessellator.extension_corner_index];
    tessellator.far_intersection_point_index  = (closest_intersection_point_index[tessellator.extension_corner_index] + 1) % 2;

    vec3 ext    = tessellator.input_corner[tessellator.extension_corner_index];
    vec3 first  = tessellator.input_corner[tessellator.first_corner_index];
    vec3 second = tessellator.input_corner[tessellator.second_corner_index];
    vec3 near   = tessellator.intersection_point[tessellator.near_intersection_point_index];
    vec3 far    = tessellator.intersection_point[tessellator.far_intersection_point_index];

#if TESSEL_DEBUG_PRINT
    printf("Tessellating triangle: %f, %f, %f | %f, %f, %f | %f, %f, %f\n", ext.x, ext.y, ext.z, first.x, first.y, first.z, second.x, second.y, second.z);
    printf("    Clipping triangle: %f, %f, %f | %f, %f, %f | %f, %f, %f\n", clip->p0.x, clip->p0.y, clip->p0.z, clip->p1.x, clip->p1.y, clip->p1.z, clip->p2.x, clip->p2.y, clip->p2.z);

    printf("    Near: %f, %f, %f | %f, %f, %f\n", near.x, near.y, near.z, far.x, far.y, far.z);

    printf("    Barycentric: %f, %f, %f | %f, %f, %f\n", tessellator.barycentric_coefficients[tessellator.near_intersection_point_index][tessellator.extension_corner_index],
           tessellator.barycentric_coefficients[tessellator.near_intersection_point_index][tessellator.first_corner_index],
           tessellator.barycentric_coefficients[tessellator.near_intersection_point_index][tessellator.second_corner_index],
           tessellator.barycentric_coefficients[tessellator.far_intersection_point_index][tessellator.extension_corner_index],
           tessellator.barycentric_coefficients[tessellator.far_intersection_point_index][tessellator.first_corner_index],
           tessellator.barycentric_coefficients[tessellator.far_intersection_point_index][tessellator.second_corner_index]);
#endif

    //
    // Now finally generate the new triangles. Dead triangles will be caught in generate_new_triangle.
    // The 'near' point essentially splits the input triangle into three subtriangles, where
    // each pair of input vertices gets connected with the 'near' point.
    // We then look in which of these subtriangles the 'far' point is, and subdivide this
    // subtriangle into three more sub-subtriangles, resulting in a total of 5 new triangles (minus the
    // empty one caught by generate_new_triangle).
    //

    tessellator.generated_triangle_count = 0;

    if(far_point_inside_outer_triangle(&tessellator, tessellator.first_corner_index, tessellator.second_corner_index, tessellator.extension_corner_index)) {
        // The far point is inside the triangle (near, first, second).
#if TESSEL_DEBUG_PRINT
        printf("    First - Second\n");
#endif
        generate_new_triangle(&tessellator, ext, first, near);
        generate_new_triangle(&tessellator, ext, near, second);
        generate_new_triangle(&tessellator, far, first, second);
        generate_new_triangle(&tessellator, near, first, far);
        generate_new_triangle(&tessellator, near, far, second);
    } else if(far_point_inside_outer_triangle(&tessellator, tessellator.extension_corner_index, tessellator.first_corner_index, tessellator.second_corner_index)) {
        // The far point is inside the triangle (near, ext, first)
#if TESSEL_DEBUG_PRINT
        printf("    Ext - First\n");
#endif
        generate_new_triangle(&tessellator, ext, second, near);
        generate_new_triangle(&tessellator, near, first, second);
        generate_new_triangle(&tessellator, far, first, ext);
        generate_new_triangle(&tessellator, near, far, first);
        generate_new_triangle(&tessellator, near, ext, far);
    } else {
        // The far point must be inside the triangle (near, second, extension)
        assert(far_point_inside_outer_triangle(&tessellator, tessellator.second_corner_index, tessellator.extension_corner_index, tessellator.first_corner_index));
#if TESSEL_DEBUG_PRINT
        printf("    Ext - Second\n");
#endif
        generate_new_triangle(&tessellator, ext, first, near);
        generate_new_triangle(&tessellator, near, first, second);
        generate_new_triangle(&tessellator, far, second, ext);
        generate_new_triangle(&tessellator, near, far, second);
        generate_new_triangle(&tessellator, near, ext, far);
    }

    return tessellator.generated_triangle_count;
}
