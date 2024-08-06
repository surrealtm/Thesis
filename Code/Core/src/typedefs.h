#pragma once

#include "foundation.h"
#include "math/v2.h"
#include "math/v3.h"
#include "math/qt.h"
#include "memutils.h"

#define USE_BVH_FOR_RAYCASTS           true
#define USE_MARCHING_CUBES_FOR_VOLUMES false
#define USE_JOB_SYSTEM                 true
#define USE_HASH_TABLE_IN_ASSEMBLER    false
#define USE_ART_IN_ASSEMBLER           true

//
// This algorithm is supposed to work with both single and double floating point precision, so that the usual
// speed / memory - accuracy tradeoff can be done by the user compiling this code. We use double precision
// by default.
// @Robustness: Make sure single precision actually works and doesn't have too many numerical issues...
//
#if CORE_SINGLE_PRECISION
typedef f32 real;
typedef v2<f32> vec2;
typedef v3<f32> vec3;
typedef qt<f32> quat;
constexpr real CORE_EPSILON = 0.001f; // :CORE_EPSILON    We have a higher floating point tolerance here than F32_EPSILON since we are talking about actual geometry units, where 1.f is roughly 1 meter semantically. Having this be smaller just leads to many different issues...
constexpr real CORE_SMALL_EPSILON = 0.00001f; // :CORE_SMALL_EPSILON    For some specific cases, we do want higher precision checks, if we know the range of values we are talking about etc...
#else
typedef f64 real;
typedef v2<f64> vec2;
typedef v3<f64> vec3;
typedef qt<f64> quat;
constexpr real CORE_EPSILON = 0.00001; // :CORE_EPSILON
constexpr real CORE_SMALL_EPSILON = 0.0000001; // :CORE_SMALL_EPSILON
#endif

#define real_abs(value) (((value) > 0) ? (value) : (-value))



/* --------------------------------------------- Telemetry Colors --------------------------------------------- */

#define TM_SYSTEM_COLOR     0
#define TM_WORLD_COLOR      1
#define TM_BVH_COLOR        2
#define TM_TESSEL_COLOR     3
#define TM_FLOODING_COLOR   4
#define TM_MARCHING_COLOR   5
#define TM_ASSEMBLING_COLOR TM_MARCHING_COLOR



/* ----------------------------------------------- 3D Geometry ----------------------------------------------- */
    
enum Axis_Index {
    AXIS_POSITIVE_X = 0,
    AXIS_POSITIVE_Y = 1,
    AXIS_POSITIVE_Z = 2,
    AXIS_NEGATIVE_X = 3,
    AXIS_NEGATIVE_Y = 4,
    AXIS_NEGATIVE_Z = 5,
    AXIS_SIGNED_COUNT = 6,
    
    AXIS_X = AXIS_POSITIVE_X,
    AXIS_Y = AXIS_POSITIVE_Y,
    AXIS_Z = AXIS_POSITIVE_Z,
    AXIS_COUNT = 3,
};

enum Virtual_Extension {
    VIRTUAL_EXTENSION_None       = 0x0,
    VIRTUAL_EXTENSION_Positive_U = 0x1,
    VIRTUAL_EXTENSION_Negative_U = 0x2,
    VIRTUAL_EXTENSION_Positive_V = 0x4,
    VIRTUAL_EXTENSION_Negative_V = 0x8,
    VIRTUAL_EXTENSION_U          = VIRTUAL_EXTENSION_Positive_U | VIRTUAL_EXTENSION_Negative_U,
    VIRTUAL_EXTENSION_V          = VIRTUAL_EXTENSION_Positive_V | VIRTUAL_EXTENSION_Negative_V,
    VIRTUAL_EXTENSION_All        = VIRTUAL_EXTENSION_Positive_U | VIRTUAL_EXTENSION_Negative_U | VIRTUAL_EXTENSION_Positive_V | VIRTUAL_EXTENSION_Negative_V,
};

BITWISE(Virtual_Extension);

struct Triangle {
    vec3 p0, p1, p2;
    
    Triangle() {};
    Triangle(vec3 p0, vec3 p1, vec3 p2);
    
    real approximate_surface_area(); // This avoids a square root for performance, since we only roughly want to know whether the triangle is dead or not.
    b8 is_dead();
    b8 no_point_behind_plane(vec3 plane_origin, vec3 plane_normal);
    b8 no_point_in_front_of_plane(vec3 plane_origin, vec3 plane_normal);
    vec3 center();
};

struct Triangulated_Plane {
    vec3 o; // For intersection distance heuristics
    vec3 n;
    Resizable_Array<Triangle> triangles;

    void create(Allocator *allocator, vec3 c, vec3 n, vec3 left, vec3 right, vec3 top, vec3 bottom);
    void create(Allocator *allocator, vec3 c, vec3 u, vec3 v);
    b8 cast_ray(vec3 ray_origin, vec3 ray_direction, real ray_distance);
};

real axis_sign(Axis_Index index); // 1 for positive, -1 for negative axis.
