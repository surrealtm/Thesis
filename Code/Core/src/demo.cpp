#include "../bindings/bindings.h"
#include "serialized_setup.cpp"
#include "os_specific.h"
#include "memutils.h"

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
}


/* Perf Statistics for the Unity Scene:
      > Always in Release, with Sparse Telemetry and Allocator Stats

   8.2.24, 11:55     Test case took 8.01s, 2.56mb.   Profiling Overhead: 13.00us
*/
