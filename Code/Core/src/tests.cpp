#include "core.h"
#include "timing.h"

void do_simple_test() {
    tmFunction();

    World world;
    world.create(v3f(100, 100, 100));
    world.add_anchor("Kitchen"_s, v3f(1, 0, 0));
    world.add_anchor("Living Room"_s, v3f(0, 0, 10));
    world.add_boundary("KitchenWall"_s, AABB { v3f(0, 0, 0), v3f(0, 0, 10) });
    world.create_octree();
}

int main() {
    do_simple_test();
    tmFinishFrame(TIMING_OUTPUT_Timeline | TIMING_OUTPUT_Statistics);
    return 0;
}
