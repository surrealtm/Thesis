#pragma once

#include "foundation.h"
#include "memutils.h"

struct Triangle;

s64 tessellate(Triangle *input, Triangle *clip, Resizable_Array<Triangle> *output, b8 clip_against_plane = false);
