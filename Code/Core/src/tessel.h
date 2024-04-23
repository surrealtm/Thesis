#pragma once

#include "foundation.h"
#include "math/math.h"
#include "core.h"

struct Tesselator {
	// The corner points and normal of the input triangle, copied since we will modify
	// the input triangle to avoid (re-) allocations.
	v3f input_corner[3];
	v3f input_normal;

	// Intermediate state keeping.
	v3f intersection_point[2];
	s8 intersection_count;

	b8 intersection_point_is_corner[2];

	s8 near_intersection_point_index;
	s8 center_intersection_point_index;
	s8 near_corner_index;
    s8 medium_corner_index;
	s8 far_corner_index;
    
    f32 barycentric_coefficients[2][3];

	// To know whether to add a new triangle or re-use the input one.
	Triangle *input_triangle;
	Resizable_Array<Triangle> *output_array;
	s8 outputted_triangle_count;
};

void tesselate(Triangle *input, Triangle *clip, Resizable_Array<Triangle> *output);
