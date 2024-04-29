#include "tessel.h"

static
void check_edge_against_triangle(Tessellator *tessellator, v3f e0, v3f e1, Triangle *triangle) {
    //
    // We check for a double-sided plane intersection here since we don't care about the triangle
    // orientation. We always want to tessellate the triangle, no matter whether the edge passed in
    // through the "forward" or "backward" side of the triangle.
    //

    v3f direction = e1 - e0;
    Triangle_Intersection_Result result = ray_double_sided_triangle_intersection(e0, direction, triangle->p0, triangle->p1, triangle->p2);

    if(result.intersection && result.distance > F32_EPSILON && result.distance < 1.0f - F32_EPSILON && tessellator->intersection_count < 2) {
        // If result.distance < 0.f || result.distance > 1.f, then we do get an intersection with the
        // ray, but not inside the edge section of the ray.
        // If the intersection_count is already >= 2, then we are having some numerical instability on
        // parallel triangles, see :TessellationOfCoplanarTriangles
        tessellator->intersection_point[tessellator->intersection_count] = e0 + direction * result.distance;
        ++tessellator->intersection_count;
    }
}

static
void generate_new_triangle(Tessellator *tessellator, v3f p0, v3f p1, v3f p2) {
    // Check if two edges of this triangle are almost parallel. In this case, the triangle
    // would be considered dead anyway, and we don't even bother generating it in the first
    // place.
    v3f n = v3_cross_v3(p1 - p0, p2 - p0);

    f32 estimated_surface_area = v3_length2(n) / 2;
    if(estimated_surface_area < 0.001f) return; // We compare it against some magic number here... Not sure what a good value would be, but F32_EPSILON is too low and doesn't catch triangles I would consider dead (e.g. when two points are *almost* the same, but the third one is very far away).

    if(tessellator->generated_triangle_count == 0) {
        // Re-use the input triangle to avoid re-allocations.
        tessellator->input_triangle->p0 = p0;
        tessellator->input_triangle->p1 = p1;
        tessellator->input_triangle->p2 = p2;
    } else {
        // Add a new triangle to the output array.
        tessellator->output_array->add({ p0, p1, p2 });
    }

    ++tessellator->generated_triangle_count;
}

static inline
v3f get_barycentric_coefficient_for_corner_index(s64 index) {
    v3f result;

    switch(index) {
    case 0: result = v3f(1, 0, 0); break;
    case 1: result = v3f(0, 1, 0); break;
    case 2: result = v3f(0, 0, 1); break;
    }

    return result;
}

static inline
s8 get_closest_intersection_point_to_corner(Tessellator *tessellator, s64 point_index) {
    return tessellator->barycentric_coefficients[0][point_index] >= tessellator->barycentric_coefficients[1][point_index] ? 0 : 1;
}

static inline
b8 far_point_inside_outer_triangle(Tessellator *tessellator, s64 c0, s64 c1, s64 c2) {
    //
    // We essentially compare the angle between the near-to-far vector with the two edges
    // of this triangle (where c0 and c1 are the corner points of the desired triangle, and
    // c2 is just the helper for the opposite corner), and make sure that the angle is 
    // between the two edges. That way, we know the far point is inside this triangle.
    //
    
    v3f near = v3f(tessellator->barycentric_coefficients[tessellator->near_intersection_point_index][0], tessellator->barycentric_coefficients[tessellator->near_intersection_point_index][1], tessellator->barycentric_coefficients[tessellator->near_intersection_point_index][2]);
    v3f far = v3f(tessellator->barycentric_coefficients[tessellator->far_intersection_point_index][0], tessellator->barycentric_coefficients[tessellator->far_intersection_point_index][1], tessellator->barycentric_coefficients[tessellator->far_intersection_point_index][2]);
    
    v3f h = get_barycentric_coefficient_for_corner_index(c2);
    v3f near_to_c0 = get_barycentric_coefficient_for_corner_index(c0) - near;
    v3f near_to_c1 = get_barycentric_coefficient_for_corner_index(c1) - near;
    v3f near_to_far = far - near;

    return v3_dot_v3(near_to_far, h) >= v3_dot_v3(near_to_c0, h) &&
        v3_dot_v3(near_to_far, h) >= v3_dot_v3(near_to_c1, h);
}

s64 tessellate(Triangle *input, Triangle *clip, Resizable_Array<Triangle> *output) {
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

    Tessellator tessellator;
    tessellator.input_corner[0] = input->p0;
    tessellator.input_corner[1] = input->p1;
    tessellator.input_corner[2] = input->p2;
    tessellator.input_triangle  = input;
    tessellator.output_array    = output;

    tessellator.intersection_count = 0;

    check_edge_against_triangle(&tessellator, input->p0, input->p1, clip);
    check_edge_against_triangle(&tessellator, input->p1, input->p2, clip);
    check_edge_against_triangle(&tessellator, input->p2, input->p0, clip);
    check_edge_against_triangle(&tessellator, clip->p0, clip->p1, input);
    check_edge_against_triangle(&tessellator, clip->p1, clip->p2, input);
    check_edge_against_triangle(&tessellator, clip->p2, clip->p0, input);

    //
    // :TessellationOfCoplanarTriangles
    // While in theory there may only ever be 0 or 2 intersection points, we may experience some
    // numerical problems if an edge or even the entire triangle lies on the clipping plane. In this
    // case, we may get 1 or even three intersection points, depending on the instability. If that
    // happens, we know that the triangle lies on the clipping plane and therefore does not require
    // further tessellation.
    //
    if(tessellator.intersection_count != 2) return 0;

    tessellator.generated_triangle_count = 0;

    calculate_barycentric_coefficients(tessellator.input_corner[0], tessellator.input_corner[1], tessellator.input_corner[2], tessellator.intersection_point[0], &tessellator.barycentric_coefficients[0][0], &tessellator.barycentric_coefficients[0][1], &tessellator.barycentric_coefficients[0][2]);
    calculate_barycentric_coefficients(tessellator.input_corner[0], tessellator.input_corner[1], tessellator.input_corner[2], tessellator.intersection_point[1], &tessellator.barycentric_coefficients[1][0], &tessellator.barycentric_coefficients[1][1], &tessellator.barycentric_coefficients[1][2]);
    
    b8 intersection_point_is_corner[2] = {
        tessellator.barycentric_coefficients[0][0] >= 1.f - F32_EPSILON || tessellator.barycentric_coefficients[0][1] >= 1.f - F32_EPSILON || tessellator.barycentric_coefficients[0][2] >= 1.f - F32_EPSILON,
        tessellator.barycentric_coefficients[1][0] >= 1.f - F32_EPSILON || tessellator.barycentric_coefficients[1][1] >= 1.f - F32_EPSILON || tessellator.barycentric_coefficients[1][2] >= 1.f - F32_EPSILON };

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

    v3f intersection_edge = tessellator.intersection_point[0] - tessellator.intersection_point[1];
    s8 closest_intersection_point_index[3] = { get_closest_intersection_point_to_corner(&tessellator, 0),
                                               get_closest_intersection_point_to_corner(&tessellator, 1),
                                               get_closest_intersection_point_to_corner(&tessellator, 2) };

    v3f corner_to_intersection[3] = { tessellator.input_corner[0] - tessellator.intersection_point[closest_intersection_point_index[0]],
                                      tessellator.input_corner[1] - tessellator.intersection_point[closest_intersection_point_index[1]],
                                      tessellator.input_corner[2] - tessellator.intersection_point[closest_intersection_point_index[2]] };

    // Calculate the angle between each corner to its nearest intersection point, and the intersection
    // edge. Normalize since we want to look at the angle between corner_to_intersection and
    // intersection_edge.
    f32 corner_extension_factor[3] = { fabsf(v3_dot_v3(corner_to_intersection[0], intersection_edge) / v3_length2(corner_to_intersection[0])),
                                       fabsf(v3_dot_v3(corner_to_intersection[1], intersection_edge) / v3_length2(corner_to_intersection[1])),
                                       fabsf(v3_dot_v3(corner_to_intersection[2], intersection_edge) / v3_length2(corner_to_intersection[2])) };

    tessellator.extension_corner_index = (corner_extension_factor[0] > corner_extension_factor[1]) ?
        (corner_extension_factor[0] > corner_extension_factor[2] ? 0 : 2) :
        (corner_extension_factor[1] > corner_extension_factor[2] ? 1 : 2);
    tessellator.first_other_corner_index  = (tessellator.extension_corner_index + 1) % 3;
    tessellator.second_other_corner_index = (tessellator.extension_corner_index + 2) % 3;
    tessellator.near_intersection_point_index = closest_intersection_point_index[tessellator.extension_corner_index];
    tessellator.far_intersection_point_index  = (closest_intersection_point_index[tessellator.extension_corner_index] + 1) % 2;

    v3f ext    = tessellator.input_corner[tessellator.extension_corner_index];
    v3f first  = tessellator.input_corner[tessellator.first_other_corner_index];
    v3f second = tessellator.input_corner[tessellator.second_other_corner_index];
    v3f near   = tessellator.intersection_point[tessellator.near_intersection_point_index];
    v3f far    = tessellator.intersection_point[tessellator.far_intersection_point_index];

    //
    // Now finally generate the new triangles. Dead triangles will be caught in generate_new_triangle.
    // The 'near' point essentially splits the input triangle into three subtriangles, where
    // each pair of input vertices gets connected with the 'near' point.
    // We then look in which of these subtriangles the 'far' point is, and subdivide this
    // subtriangle into three more sub-subtriangles, resulting in a total of 5 new triangles (minus the
    // empty one caught by generate_new_triangle).
    //

    if(far_point_inside_outer_triangle(&tessellator, tessellator.first_other_corner_index, tessellator.second_other_corner_index, tessellator.extension_corner_index)) {
        // The far point is inside the triangle (near, first, second).
        generate_new_triangle(&tessellator, ext, first, near);
        generate_new_triangle(&tessellator, ext, near, second);
        generate_new_triangle(&tessellator, far, first, second);
        generate_new_triangle(&tessellator, near, first, far);
        generate_new_triangle(&tessellator, near, far, second);
    } else if(far_point_inside_outer_triangle(&tessellator, tessellator.extension_corner_index, tessellator.first_other_corner_index, tessellator.second_other_corner_index)) {
        // The far point is inside the triangle (near, ext, first)
        generate_new_triangle(&tessellator, ext, second, near);
        generate_new_triangle(&tessellator, near, first, second);
        generate_new_triangle(&tessellator, far, first, ext);
        generate_new_triangle(&tessellator, near, far, first);
        generate_new_triangle(&tessellator, near, ext, far);
    } else {
        // The far point must be inside the triangle (near, ext, second)
        assert(far_point_inside_outer_triangle(&tessellator, tessellator.extension_corner_index, tessellator.second_other_corner_index, tessellator.first_other_corner_index));
        generate_new_triangle(&tessellator, ext, first, near);
        generate_new_triangle(&tessellator, near, first, second);
        generate_new_triangle(&tessellator, far, second, ext);
        generate_new_triangle(&tessellator, near, far, second);
        generate_new_triangle(&tessellator, near, ext, far);
    }

    return tessellator.generated_triangle_count;
}
