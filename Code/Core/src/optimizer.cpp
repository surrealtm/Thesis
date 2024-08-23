#include "optimizer.h"
#include "timing.h"

#define OPTIM_DEBUG_PRINT true

struct Triangle_Combiner {
    s8 unshared_t0_index;
    s8 unshared_t1_index;
    vec3 shared_inlined;
    vec3 shared_extruded;
    b8 found_unshared_t0, found_unshared_t1;
    b8 found_shared_inlined, found_shared_extruded;
};

static inline
b8 point_on_edge(vec3 point, vec3 origin, vec3 direction) {
    // https://www.geometrictools.com/Documentation/DistancePointLine.pdf
    real t0 = v3_dot_v3(direction, point - origin);
    real distance = v3_length(point - (origin + t0 * direction)); // @@Speed: Sqrt!
    return distance <= CORE_EPSILON;
}

static inline
b8 points_almost_identical(vec3 p0, vec3 p1) {
    return fabs(p0.x - p1.x) <= CORE_EPSILON && fabs(p0.y - p1.y) <= CORE_EPSILON && fabs(p0.z - p1.z) <= CORE_EPSILON;
}

static inline
b8 check_if_unshared(vec3 p, Triangle &t) {
    if(points_almost_identical(p, t.p0)) return false;
    if(points_almost_identical(p, t.p1)) return false;
    if(points_almost_identical(p, t.p1)) return false;
    return true;
}

static inline
b8 check_if_shared_extruded(vec3 p0, vec3 p1, vec3 unshared_t0, vec3 unshared_t1) {
    if(!points_almost_identical(p0, p1)) return false;    
    return !point_on_edge(p0, unshared_t0, unshared_t1 - unshared_t0);
}

static inline
b8 check_if_shared_extruded(vec3 p0, Triangle &t, vec3 unshared_t0, vec3 unshared_t1) {
    if(check_if_shared_extruded(p0, t.p0, unshared_t0, unshared_t1)) return true;
    if(check_if_shared_extruded(p0, t.p1, unshared_t0, unshared_t1)) return true;
    if(check_if_shared_extruded(p0, t.p2, unshared_t0, unshared_t1)) return true;
    return false;
}

static inline
b8 check_if_shared_inlined(vec3 p0, vec3 p1, vec3 unshared_t0, vec3 unshared_t1) {
    if(!points_almost_identical(p0, p1)) return false;    
    return point_on_edge(p0, unshared_t0, unshared_t1 - unshared_t0);
}

static inline
b8 check_if_shared_inlined(vec3 p0, Triangle &t, vec3 unshared_t0, vec3 unshared_t1) {
    if(check_if_shared_inlined(p0, t.p0, unshared_t0, unshared_t1)) return true;
    if(check_if_shared_inlined(p0, t.p1, unshared_t0, unshared_t1)) return true;
    if(check_if_shared_inlined(p0, t.p2, unshared_t0, unshared_t1)) return true;
    return false;
}

static
b8 maybe_combine_triangles(Triangle &t0, Triangle &t1, Triangle &output) {
    //
    // Two triangles can be combined if together they form one bigger triangle.
    // Two conditions must be met for this:
    //   1. They must share two vertices.
    //   2. The edge between the two unshared vertices must go through one of the shared vertices.
    //
    //      /|  < unshared_t0
    //     / |
    //    /  |
    //   /___|  < shared_extruded , shared_inlined
    //   \   |
    //    \  |
    //     \ |
    //      \|  < unshared_t1
    //
    // Please don't judge me for this code, it was done in a hurry.
    //

#if OPTIM_DEBUG_PRINT
    printf("Optimizing the triangles:\n");
    printf("    %f, %f, %f | %f, %f, %f | %f, %f, %f.\n", t0.p0.x, t0.p0.y, t0.p0.z, t0.p1.x, t0.p1.y, t0.p1.z, t0.p2.x, t0.p2.y, t0.p2.z);
    printf("    %f, %f, %f | %f, %f, %f | %f, %f, %f.\n", t1.p0.x, t1.p0.y, t1.p0.z, t1.p1.x, t1.p1.y, t1.p1.z, t1.p2.x, t1.p2.y, t1.p2.z);
#endif

    Triangle_Combiner combiner;
    combiner.found_unshared_t0     = false;
    combiner.found_unshared_t1     = false;
    combiner.found_shared_inlined  = false;
    combiner.found_shared_extruded = false;

    {
        for(s8 i = 0; i < 3; ++i) {
            if(check_if_unshared(t0[i], t1)) {
                combiner.found_unshared_t0 = true;
                combiner.unshared_t0_index = i;
                break;
            }
        }
        
        if(!combiner.found_unshared_t0) {
            // Early exit for speed
#if OPTIM_DEBUG_PRINT
            printf("    Rejected due to no unshared t0.\n");
#endif
            return false;
        }

        for(s8 i = 0; i < 3; ++i) {
            if(check_if_unshared(t1[i], t0)) {
                combiner.found_unshared_t1 = true;
                combiner.unshared_t1_index = i;
                break;
            }
        }

        if(!combiner.found_unshared_t1) {
            // Early exit for speed
#if OPTIM_DEBUG_PRINT
            printf("    Rejected due to no unshared t1.\n");
#endif
            return false; 
        }

        for(s8 i = 0; i < 3; ++i) {
            if(check_if_shared_extruded(t0[i], t1, t0[combiner.unshared_t0_index], t1[combiner.unshared_t1_index])) {
                combiner.found_shared_extruded = true;
                combiner.shared_extruded = t0[i];
                break;
            }
        }

        if(!combiner.found_shared_extruded) {
            // Early exit for speed
#if OPTIM_DEBUG_PRINT
            printf("    Rejected due to no shared extruded.\n");
#endif
            return false;
        }

        for(s8 i = 0; i < 3; ++i) {
            if(check_if_shared_inlined(t0[i], t1, t0[combiner.unshared_t0_index], t1[combiner.unshared_t1_index])) {
                combiner.found_shared_inlined = true;
                combiner.shared_inlined = t0[i];
                break;
            }
        }
        
        if(!combiner.found_shared_inlined) {
            // Early exit for speed
#if OPTIM_DEBUG_PRINT
            printf("    Rejected due to no shared inlined.\n");
#endif
            return false; 
        }
    }

    // Actually combine the triangles.
    output.p0 = t0[combiner.unshared_t0_index];
    output.p1 = t1[combiner.unshared_t1_index];
    output.p2 = combiner.shared_extruded;

#if OPTIM_DEBUG_PRINT
    printf("    Combined the two triangles.\n");
#endif

    return true;
}

void optimize_mesh(Resizable_Array<Triangle> &mesh) {
    tmFunction(TM_OPTIMIZER_COLOR);
    
    b8 something_changed;
    Triangle output;
    
    do {
        something_changed = false;
        
        for(s64 i = 0; i < mesh.count; ++i) {
            for(s64 j = i + 1; j < mesh.count; ++j) {
                b8 combine = maybe_combine_triangles(mesh[i], mesh[j], output);
                
                if(false && combine) {
                    mesh.remove(j); // J is the bigger index, so remove that one first so that i remains stable...
                    mesh.remove(i);
                    mesh.add(output);
                    something_changed = true;
                }
            }
        }
    } while(something_changed);
}