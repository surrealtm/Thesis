#include "bindings.h"
#include "../src/core.h"
#include "../src/random.h"

extern "C" {
    /* ----------------------------------------------- Testing ------------------------------------------------ */

    void core_do_simple_test() {
        tmZone("do_simple_test", TM_SYSTEM_COLOR);

        World world;
        world.create(v3f(100, 10, 100));
        world.add_anchor("Kitchen"_s, v3f(10, 1, 1));
        world.add_anchor("Living Room"_s, v3f(10, 1, 10));
        world.add_boundary("KitchenWall"_s, v3f(5, 1, 5), v3f(4, .25, .5));
        world.create_octree();
    }

    void core_do_octree_test() {
        tmZone("do_octree_test", TM_SYSTEM_COLOR);
        
        const f32 width    = 100;
        const f32 height   = 10;
        const f32 length   = 100;
        const f32 max_size = 5;

        World world;
        world.create(v3f(width, height, length));

        {
            tmZone("create_random_objects", TM_SYSTEM_COLOR);

            for(s64 i = 0; i < 10000; ++i) {
                v3f size     = v3f(get_random_f32_uniform(0.1f, max_size), get_random_f32_uniform(0.1f, max_size), get_random_f32_uniform(0.1f, max_size));
                v3f position = v3f(get_random_f32_uniform(-width  + size.x, width  - size.x),
                                   get_random_f32_uniform(-height + size.y, height - size.y),
                                   get_random_f32_uniform(-length + size.z, length - size.z));
                world.add_boundary("Boundary"_s, position, size);
            }
        }
        
        world.create_octree();
    }
    


    /* ---------------------------------------------- Profiling ---------------------------------------------- */

    void core_begin_profiling() {
        tmBegin();
    }

    void core_stop_profiling() {
        tmFinish();
    }

    void core_print_profiling() {
        tmPrintToConsole(TIMING_OUTPUT_Summary | TIMING_OUTPUT_Timeline, TIMING_OUTPUT_Sort_By_Exclusive);
    }

    Timing_Data core_get_profiling_data() {
        Timing_Data data = tmData(TIMING_OUTPUT_Sort_By_Inclusive);
        return data;
    }

    void core_free_profiling_data(Timing_Data *data) {
        tmFreeData(data);
    }
}


#if FOUNDATION_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


BOOL WINAPI DllMain(HINSTANCE hinstance, DWORD reason, LPVOID reserved) {
    //
    // Set up some things regarding the core library.
    //
    tmSetColor(TM_OCTREE_COLOR,  46, 184, 230);
    tmSetColor(TM_WORLD_COLOR,   95, 230,  46);
    tmSetColor(TM_SYSTEM_COLOR, 209, 202, 197);
    return true;
}
#endif
