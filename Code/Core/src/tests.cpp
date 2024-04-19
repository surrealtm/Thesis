#include "../bindings/bindings.h"

int main() {
    tmBegin();
    World_Handle world = core_do_octree_test();
    tmFinish();

    core_print_profiling(false);
    core_print_memory_information(world);

    return 0;
}
