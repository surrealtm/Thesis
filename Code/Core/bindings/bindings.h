#pragma once

#include "timing.h"
#include "../src/dbgdraw.h"

#if FOUNDATION_WIN32 && CORE_LIB
# define EXPORT __declspec(dllexport)
#else
# define EXPORT 
#endif


extern "C" {
    typedef void* World_Handle;

    typedef struct Memory_Allocator_Information {
        string name;
        s64 allocation_count;
        s64 deallocation_count;
        s64 working_set_size; // In bytes
        s64 peak_working_set_size; // In bytes
    } Memory_Allocator_Information;
    
    typedef struct Memory_Information {
        s64 os_working_set_size; // In bytes
        Memory_Allocator_Information *allocators;
        s64 allocator_count;
    } Memory_Information;


    
    /* --------------------------------------------- General API --------------------------------------------- */

    EXPORT World_Handle core_allocate_world();
    EXPORT void core_destroy_world(World_Handle world);

    

    /* ----------------------------------------------- Testing ----------------------------------------------- */
    
    EXPORT World_Handle core_do_house_test();
    EXPORT World_Handle core_do_octree_test();
    EXPORT World_Handle core_do_large_volumes_test();

    

    /* ---------------------------------------------- Debug Draw ---------------------------------------------- */
    
    EXPORT Debug_Draw_Data core_debug_draw_world(World_Handle world, Debug_Draw_Options options);
    EXPORT void core_free_debug_draw_data(Debug_Draw_Data *data);
    


    /* -----------------------------------------------Profiling ---------------------------------------------- */

    EXPORT void core_begin_profiling();
    EXPORT void core_stop_profiling();
    EXPORT void core_print_profiling();
    EXPORT Timing_Data core_get_profiling_data();
    EXPORT void core_free_profiling_data(Timing_Data *data);

    EXPORT Memory_Information core_get_memory_information(World_Handle world_handle);
    EXPORT void core_free_memory_information(Memory_Information *info);
}
