#pragma once

#include "math/v2.h"
#include "math/v3.h"
#include "math/qt.h"

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
