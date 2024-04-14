#include "bindings.h"
#include "../src/core.h"

extern "C" {
    /* ----------------------------------------------- Testing ------------------------------------------------ */

    void core_do_simple_test() {
        tmZone("do_simple_test", TM_SYSTEM_COLOR);

        World world;
        world.create(v3f(100, 100, 100));
        world.add_anchor("Kitchen"_s, v3f(10, 1, 1));
        world.add_anchor("Living Room"_s, v3f(10, 1, 10));
        world.add_boundary("KitchenWall"_s, v3f(5, 1, 5), v3f(4, .25, .5));
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
