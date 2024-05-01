#include "../bindings/bindings.h"
#include "../src/jobs.h"
#include "../src/hash_table.h"
#include "../src/strings.h"

int main() {
    World_Handle world = core_do_large_volumes_test(false);
    core_print_memory_information(world);
    core_destroy_world(world);
    return 0;
}
