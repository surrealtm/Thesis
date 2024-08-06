#include "../bindings/bindings.h"
#include "serialized_setup.cpp"
#include "os_specific.h"
#include "memutils.h"
#include "art.h"
#include "random.h"

typedef s8 T;

static
b8 contains(T *array, s64 array_count, T value) {
    for(s64 i = 0; i < array_count; ++i) if(array[i] == value) return true;
    return false;
}

int main() {
    tmBegin();
    
    Hardware_Time start = os_get_hardware_time();
    setup_world(); // Serialized from unity!
    Hardware_Time end = os_get_hardware_time();

    printf("Test case took %.2fs, %.2fmb.\n", os_convert_hardware_time(end - start, Seconds), convert_to_memory_unit(os_get_working_set_size(), Megabytes));
    
    tmFinish();

#if FOUNDATION_TELEMETRY
    tmPrintToConsole(TIMING_OUTPUT_Timeline);
#endif

    return 0;

    /*
    s64 value_count = ONE_MEGABYTE;
    T *values = (T *) Default_Allocator->allocate(value_count * sizeof(T));

    for(s64 i = 0; i < value_count; ++i) {
        values[i] = (T) default_random_generator.random_s64();
    }

    T *anti_values = (T *) Default_Allocator->allocate(value_count * sizeof(T));

    for(s64 i = 0; i < value_count; ++i) {
        anti_values[i] = (T) default_random_generator.random_s64();
    }
    
    Adaptive_Radix_Tree<T> tree;
    tree.create();

    for(s64 i = 0; i < value_count; ++i) {
        tree.add(values[i]);
    }

    for(s64 i = 0; i < value_count; ++i) {
        assert(tree.query(values[i]) == true);
    }

    for(s64 i = 0; i < value_count; ++i) {
        assert(tree.query(anti_values[i]) == contains(values, value_count, anti_values[i]));
    }
    
    tree.destroy();

    Default_Allocator->deallocate(values);
    Default_Allocator->deallocate(anti_values);
    
#if FOUNDATION_DEVELOPER
    if(Default_Allocator->stats.allocations > Default_Allocator->stats.deallocations) printf("Memory Leak!!!!\n");
#endif

    return 0;
    */
}


/* Perf Statistics for the Unity Scene:
      > Always in Release, with Sparse Telemetry and Allocator Stats

   2.8.24, 11:55     Test case took 8.01s, 2.56mb.   Profiling Overhead:  13.00us
   6.8.24, 14:35     Test case took 7.19s, 2.57mb.   Profiling Overhead:  14.60us
   6.8.24, 14:51     Test case took 4.42s, 5.70mb.   Profiling Overhead: 696.60us
   6.8.24, 16:51     Test case took 0.62s, 5.71mb.   Profiling Overhead: 788.60us
*/
