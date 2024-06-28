#pragma once

#include "foundation.h"
#include "memutils.h"
#include "typedefs.h"

typedef b8 (*Triangle_Should_Be_Clipped)(Triangle *generated_triangle, Triangle *clip_triangle, void *user_pointer);

s64 tessellate(Triangle *input, Triangle *clip, vec3 clip_normal, Resizable_Array<Triangle> *output, b8 clip_against_plane = false, Triangle_Should_Be_Clipped triangle_should_be_clipped_proc = null, void *triangle_should_be_clipped_user_pointer = null);
