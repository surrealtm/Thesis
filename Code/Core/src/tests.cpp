#include "core.h"
#include "timing.h"

void do_simple_test() {
    tmFunction();

    World world;
    world.create(v3f(100, 100, 100));
    world.add_anchor("Kitchen"_s, v3f(10, 1, 1));
    world.add_anchor("Living Room"_s, v3f(10, 1, 10));
    world.add_boundary("KitchenWall"_s, v3f(5, 1, 5), v3f(4, .25, .5));
    world.create_octree();
}

int main() {
    do_simple_test();
    tmFinishFrame(TIMING_OUTPUT_Timeline | TIMING_OUTPUT_Statistics, TIMING_OUTPUT_Sort_By_Inclusive);
    return 0;
}
