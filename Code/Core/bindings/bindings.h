#include "timing.h"

#if FOUNDATION_WIN32 && CORE_LIB
# define EXPORT __declspec(dllexport)
#else
# define EXPORT 
#endif

extern "C" {
    /* ----------------------------------------------- Testing ----------------------------------------------- */
    
    EXPORT void core_do_simple_test();
    


    /* -----------------------------------------------Profiling ---------------------------------------------- */

    EXPORT void core_begin_profiling();
    EXPORT void core_stop_profiling();
    EXPORT void core_print_profiling();
    EXPORT Timing_Data core_get_profiling_data();
    EXPORT void core_free_profiling_data(Timing_Data *data);
}
