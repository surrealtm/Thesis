#pragma once

#include "foundation.h"
#include "memutils.h"

constexpr f32 TESSEL_EPSILON = 0.001f; // We have a higher floating point tolerance here since we are talking about actual geometry units, where 1.f is roughly 1 meter semantically...

struct Triangle;

s64 tessellate(Triangle *input, Triangle *clip, Resizable_Array<Triangle> *output, b8 clip_against_plane = false);
