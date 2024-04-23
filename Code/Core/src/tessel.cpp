#include "tessel.h"


static
void check_edge_against_triangle(Tesselator *tesselator, v3f e0, v3f e1, Triangle *triangle) {
    //
    // We check for a double-sided plane intersection here since we don't care about the triangle
    // orientation. We always want to tesselate the triangle, no matter whether the edge passed in
    // through the "forward" or "backward" side of the triangle.
    //

    v3f direction = e1 - e0;
    Triangle_Intersection_Result result = ray_double_sided_triangle_intersection(e0, direction, triangle->p0, triangle->p1, triangle->p2);

    if(result.intersection) {
        assert(tesselator->intersection_count < 2);
        tesselator->intersection_point[tesselator->intersection_count] = e0 + direction * result.distance;
        ++tesselator->intersection_count;
    }
}

static
void find_near_intersection_index(Tesselator *tesselator) {
    f32 biggest_coefficient = MIN_F32;

    for(u8 i = 0; i < 2; ++i) {
        for(u8 c = 0; c < 3; ++c) {
            if(tesselator->barycentric_coefficients[i][c] > biggest_coefficient) {
                tesselator->near_intersection_point_index = i;
                tesselator->near_corner_index = c;
                biggest_coefficient = tesselator->barycentric_coefficients[i][c];
            }
        }
    }
}

static
void generate_new_triangle(Tesselator *tesselator, v3f p0, v3f p1, v3f p2) {
    if(tesselator->outputted_triangle_count == 0) {
        // Re-use the input triangle to avoid re-allocations.
        tesselator->input_triangle->p0 = p0;
        tesselator->input_triangle->p1 = p1;
        tesselator->input_triangle->p2 = p2;
        tesselator->input_triangle->n  = tesselator->input_normal;
    } else {
        // Add a new triangle to the output array.
        tesselator->output_array->add({ p0, p1, p2, tesselator->input_normal });
    }

    ++tesselator->outputted_triangle_count;
}


void tesselate(Triangle *input, Triangle *clip, Resizable_Array<Triangle> *output) {
    //
    // Two triangles can intersect in the following two ways:
    //   1. Two edges of one triangle intersect the face of the other triangle.
    //   2. One edge of each triangle intersects the face of the other triangle.
    // In both cases, we get two edge-to-face intersections, meaning we got a total
    // of two intersection points. We then triangulate the input around these two
    // intersection points.
    //
    // If the two triangles don't intersect, then there is no need to tesselate here
    // and we are done.
    //
    Tesselator tesselator;
    tesselator.input_corner[0] = input->p0;
    tesselator.input_corner[1] = input->p1;
    tesselator.input_corner[2] = input->p2;
    tesselator.input_normal    = input->n;
    tesselator.input_triangle  = input;
    tesselator.output_array    = output;

    tesselator.intersection_count = 0;

    check_edge_against_triangle(&tesselator, input->p0, input->p1, clip);
    check_edge_against_triangle(&tesselator, input->p1, input->p2, clip);
    check_edge_against_triangle(&tesselator, input->p2, input->p0, clip);
    check_edge_against_triangle(&tesselator, clip->p0, clip->p1, input);
    check_edge_against_triangle(&tesselator, clip->p1, clip->p2, input);
    check_edge_against_triangle(&tesselator, clip->p2, clip->p0, input);

    assert(tesselator.intersection_count == 0 || tesselator.intersection_count == 2);

    //
    // No intersections, we are done here.
    //
    if(tesselator.intersection_count == 0) return;

    //
    // We've got our two intersection points. These could be anywhere on our input triangle.
    // To figure out the best way to tesselate our input triangle, we look at the barycentric
    // coefficients of the intersection points.
    // If one coefficient of a point is zero, then we know the point lies on an edge of the
    //     input triangle. In that case, we only need to split the edge, which means we need
    //     to generate one less new triangle.
    // If two coefficients of a point are zero (therefore the last coefficient must be 1), then
    //     we know the point is a corner point of our input triangle, in which case we don't
    //     need to tesselate anything regarding this intersection point.
    // If both intersection points are corner points, then we can just do an early exit.
    //
    calculate_barycentric_coefficients(input->p0, input->p1, input->p2, tesselator.intersection_point[0], &tesselator.barycentric_coefficients[0][0], &tesselator.barycentric_coefficients[0][1], &tesselator.barycentric_coefficients[0][2]);
    calculate_barycentric_coefficients(input->p0, input->p1, input->p2, tesselator.intersection_point[1], &tesselator.barycentric_coefficients[1][0], &tesselator.barycentric_coefficients[1][1], &tesselator.barycentric_coefficients[1][2]);

    tesselator.intersection_point_is_corner[0] = tesselator.barycentric_coefficients[0][0] >= 1.f - F32_EPSILON || tesselator.barycentric_coefficients[0][1] >= 1.f - F32_EPSILON || tesselator.barycentric_coefficients[0][2] >= 1.f - F32_EPSILON;
    tesselator.intersection_point_is_corner[1] = tesselator.barycentric_coefficients[1][0] >= 1.f - F32_EPSILON || tesselator.barycentric_coefficients[1][1] >= 1.f - F32_EPSILON || tesselator.barycentric_coefficients[1][2] >= 1.f - F32_EPSILON;

    if(tesselator.intersection_point_is_corner[0] && tesselator.intersection_point_is_corner[1]) return;

    //
    // At this point, we need to select the different triangles to generate. The input triangle
    // may need to be split into 5 new triangles (discarding the old one). Read the comment
    // above to see when we can skip some of these triangles (since they'd have an area of 0).
    //
    // We assign both of the intersection points a role: One is the "center", and one is the "near"
    // point. The center point gets connected to all three corners and the "near" point. That leaves
    // us with three triangles. The "near" point then subdivides one of these triangles again into
    // three triangles (discarding the old one), which makes our five new triangles mentioned before.
    //
    // The "near" point is the intersection point closest to a corner. The "center" point is then
    // just the other one. We choose the "near" point like this to generate the best possible
    // triangles we can (where a better triangle is one whose sides are of more-equal length; the
    // triangle is less stretched). Consider the scenario where we'd instead choose the other way
    // round, the triangle connecting p0, i0, p1 would be very thin and stretched.
    //
    // We also need to figure the other "near-connection" point out, for which we have the remaining
    // two corners as options. While theoretically both would work, we want to choose the one corner
    // which does not generate overlap of the triangles. In the example below, choosing p2 as
    // "near-connection" would generate have a little overlap on the triangles (i1, p0, p1) and
    // (i0, i1, p2). Therefore, we choose p1 as "near-connection", because the angle between
    // (p1, i1) . (i0, i1) is smaller than the angle between (p2, i1) . (i0, i1).
    //
    // We generate the following triangles for this example:
    //
    //     i0, i1, p0
    //     i0, i1, p1
    //     i0, p0, p1
    //     i1, p1, p2
    //     i1, p0, p2
    //
    //                       p0
    //                       /\
    //                 i0   /* \            <- "near"
    //                     /    \
    //                    /      \
    //              i1   /    *   \         <- "center"
    //                  /          \
    //                 /            \
    //             p1 /--------------\ p2
    //

    find_near_intersection_index(&tesselator);
    tesselator.center_intersection_point_index = (tesselator.near_intersection_point_index + 1) % 2;
    tesselator.outputted_triangle_count = 0;

    v3f near = tesselator.intersection_point[tesselator.near_intersection_point_index],
        center = tesselator.intersection_point[tesselator.center_intersection_point_index];
    
    //
    // Choose the "near-connection" corner, as described above.
    //
    {
        v3f near_to_center = center - near;
        s8 c0 = (tesselator.near_corner_index + 1) % 3;
        s8 c1 = (tesselator.near_corner_index + 2) % 3;
        if(v3_dot_v3(near_to_center, center - tesselator.input_corner[c0]) > v3_dot_v3(near_to_center, center - tesselator.input_corner[c1])) {
            tesselator.medium_corner_index = c0;
            tesselator.far_corner_index    = c1;
        } else {
            tesselator.medium_corner_index = c1;
            tesselator.far_corner_index    = c0;
        }
    }
    
    //
    // Actually start connecting the different vertices into new triangles.
    //
    if(!tesselator.intersection_point_is_corner[tesselator.near_intersection_point_index]) {
        // Example: i0, p0, p1
        generate_new_triangle(&tesselator, tesselator.intersection_point[tesselator.near_intersection_point_index], tesselator.input_corner[tesselator.near_corner_index], tesselator.intersection_point[tesselator.medium_corner_index]);

        // Example: i0, i1, p0
        generate_new_triangle(&tesselator, tesselator.intersection_point[tesselator.near_intersection_point_index], tesselator.intersection_point[tesselator.center_intersection_point_index], tesselator.intersection_point[tesselator.near_corner_index]);

        // Example: i0, i1, p1
        generate_new_triangle(&tesselator, tesselator.input_corner[tesselator.near_intersection_point_index], tesselator.input_corner[tesselator.center_intersection_point_index], tesselator.intersection_point[tesselator.medium_corner_index]);
    } else {
        // Example: i1, p0, p1. Not actually present in the example since i0 is not a corner point.
        generate_new_triangle(&tesselator, tesselator.intersection_point[tesselator.center_intersection_point_index], tesselator.input_corner[tesselator.near_corner_index], tesselator.input_corner[tesselator.medium_corner_index]);
    }

    // Example: i1, p0, p2
    generate_new_triangle(&tesselator, tesselator.intersection_point[tesselator.center_intersection_point_index], tesselator.input_corner[tesselator.near_corner_index], tesselator.input_corner[tesselator.far_corner_index]);

    // Example: i1, p1, p2
    generate_new_triangle(&tesselator, tesselator.intersection_point[tesselator.center_intersection_point_index], tesselator.input_corner[tesselator.medium_corner_index], tesselator.input_corner[tesselator.far_corner_index]);
}
