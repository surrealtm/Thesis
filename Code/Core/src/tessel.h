#pragma once

#include "foundation.h"
#include "memutils.h"
#include "typedefs.h"

enum Tessellation_Result {
    TESSELLATION_No_Intersection,
    TESSELLATION_Intersection_But_No_Triangles, // If all the would-be triangles were rejected
    TESSELLATION_Success, // At least one triangle has been generated (and the input was discarded)
};

typedef b8 (*Triangle_Should_Be_Clipped)(Triangle *generated_triangle, Triangle *clip_triangle, void *user_pointer);

Tessellation_Result tessellate(Triangle *input, Triangle *clip, vec3 clip_normal, Resizable_Array<Triangle> *output, b8 clip_against_plane = false, Triangle_Should_Be_Clipped triangle_should_be_clipped_proc = null, void *triangle_should_be_clipped_user_pointer = null);
