#include "../bindings/bindings.h"
#include "../src/jobs.h"
#include "../src/hash_table.h"
#include "../src/string_type.h"

#include "../src/core.h"
#include "../src/tessel.h"

int main() {
    /*
    core_begin_profiling();
    World_Handle world = core_do_house_test(false);
    core_stop_profiling();
    core_print_profiling(false);
    core_print_memory_information(world);
    core_destroy_world(world);
    return 0;
    */

    Triangle delimiter = Triangle(vec3(100, -100, 10.5), vec3(18.186533479473205, -18.186533479473212, 10.500000000000000), vec3(-100, -100, 10.5), vec3(0, 0, 1));
    Triangle clip      = Triangle(vec3(59.093266739736585, -100, 81.352540378443877), vec3(-40.906733260263373, -100, -91.852540378443877), vec3(-40.906733260263373, 100, -91.852540378443877), vec3(1, 0, 0));

    Resizable_Array<Triangle> output;
    output.add(delimiter);

    tessellate(&output[0], &clip, &output);

    return 0;
}
