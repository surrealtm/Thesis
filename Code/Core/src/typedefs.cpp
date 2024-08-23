#include "typedefs.h"

#include "timing.h"
#include "math/intersect.h"

/* ----------------------------------------------- 3D Geometry ----------------------------------------------- */

Triangle::Triangle(vec3 p0, vec3 p1, vec3 p2) : p0(p0), p1(p1), p2(p2) {
}

real Triangle::approximate_surface_area() {
    // https://math.stackexchange.com/questions/128991/how-to-calculate-the-area-of-a-3d-triangle
    vec3 h = v3_cross_v3(this->p1 - this->p0, this->p2 - this->p0);
    return v3_length2(h) / 2;
}

b8 Triangle::is_dead() {
    return this->approximate_surface_area() <= CORE_EPSILON;
}

b8 Triangle::no_point_behind_plane(vec3 plane_origin, vec3 plane_normal) {
    real d0 = v3_dot_v3(this->p0 - plane_origin, plane_normal);
    real d1 = v3_dot_v3(this->p1 - plane_origin, plane_normal);
    real d2 = v3_dot_v3(this->p2 - plane_origin, plane_normal);
    return d0 >= -CORE_EPSILON && d1 >= -CORE_EPSILON && d2 >= -CORE_EPSILON;
}

b8 Triangle::no_point_in_front_of_plane(vec3 plane_origin, vec3 plane_normal) {
    real d0 = v3_dot_v3(this->p0 - plane_origin, plane_normal);
    real d1 = v3_dot_v3(this->p1 - plane_origin, plane_normal);
    real d2 = v3_dot_v3(this->p2 - plane_origin, plane_normal);
    return d0 <= CORE_EPSILON && d1 <= CORE_EPSILON && d2 <= CORE_EPSILON;
}

vec3 Triangle::center() {
    return (this->p0 + this->p1 + this->p2) / 3.0;
}

vec3 Triangle::operator[](s8 index) {
    assert(index >= 0 && index <= 2);
    
    switch(index) {
    case 0: return this->p0;
    case 1: return this->p1;
    default: return this->p2;    
    }
}



void Triangulated_Plane::create(Allocator *allocator, vec3 c, vec3 n, vec3 left, vec3 right, vec3 top, vec3 bottom) {
    vec3 p0 = c + left  + top;
    vec3 p1 = c + left  + bottom;
    vec3 p2 = c + right + top;
    vec3 p3 = c + right + bottom;

    this->o = c;
    this->n = n;
    this->triangles.allocator = allocator;
    this->triangles.add({ p0, p3, p1 });
    this->triangles.add({ p0, p2, p3 });
}

void Triangulated_Plane::create(Allocator *allocator, vec3 c, vec3 u, vec3 v) {
    this->create(allocator, c, v3_normalize(v3_cross_v3(u, v)), -u, u, -v, v);
}

b8 Triangulated_Plane::cast_ray(vec3 ray_origin, vec3 ray_direction, real max_ray_distance) {
    b8 hit_something = false;

    for(Triangle &triangle : this->triangles) {
        auto triangle_result = ray_double_sided_triangle_intersection(ray_origin, ray_direction, triangle.p0, triangle.p1, triangle.p2);

        if(triangle_result.intersection && triangle_result.distance >= 0.f && triangle_result.distance <= max_ray_distance) { // distance isn't normalized!
            hit_something = true;
            goto early_exit;
        }
    }

 early_exit:
    return hit_something;
}



real axis_sign(Axis_Index index) {
    return index < AXIS_COUNT ? 1 : -1;
}
