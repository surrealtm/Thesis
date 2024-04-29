#pragma once

#include "foundation.h"
#include "math/math.h"
#include "core.h"

struct Tessellator {
	// If this is true, then clip against the plane of the triangle, not just the triangle's
	// surface area.
	b8 clip_against_plane;

	// The corner points and normal of the input triangle, copied since we will modify
	// the input triangle to avoid (re-) allocations.
	v3f input_corner[3];

	// Intermediate state keeping.
	v3f intersection_point[2];
	s8 intersection_count;

    s8 far_intersection_point_index;
    s8 near_intersection_point_index;

    s8 extension_corner_index;
    s8 first_other_corner_index;
    s8 second_other_corner_index;
    
    f32 barycentric_coefficients[2][3]; // The barycentric coefficients of the intersection points in relation to the input corners.
    
	// To know whether to add a new triangle or re-use the input one.
	Triangle *input_triangle;
	Resizable_Array<Triangle> *output_array;
	s64 generated_triangle_count;
};

s64 tessellate(Triangle *input, Triangle *clip, Resizable_Array<Triangle> *output, bool clip_against_plane = false);
