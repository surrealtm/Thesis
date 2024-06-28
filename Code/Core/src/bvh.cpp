#include "bvh.h"

void BVH::create(Allocator *allocator, vec3 center, vec3 half_size) {
    this->allocator = allocator;
    this->center    = center;
    this->half_size = half_size;
    this->triangles.allocator = allocator;
}

