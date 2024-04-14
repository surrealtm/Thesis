#include "core.h"
#include "timing.h"

void do_simple_test() {
    {
        tmZone("do_simple_test", 0);

        World world;
        world.create(v3f(100, 100, 100));
        world.add_anchor("Kitchen"_s, v3f(10, 1, 1));
        world.add_anchor("Living Room"_s, v3f(10, 1, 10));
        world.add_boundary("KitchenWall"_s, v3f(5, 1, 5), v3f(4, .25, .5));
        world.create_octree();
    }

    tmFinish();
}

int main() {
    do_simple_test();
    return 0;
}
