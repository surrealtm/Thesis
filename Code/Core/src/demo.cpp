#include "../bindings/bindings.h"

int main() {
#if FOUNDATION_DEVELOPER
    core_begin_profiling();
    World_Handle world = core_do_house_test();
    core_stop_profiling();
    core_print_profiling(false);
    core_print_memory_information(world);
    core_destroy_world(world);
#endif

    return 0;
}
