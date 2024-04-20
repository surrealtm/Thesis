#include "../bindings/bindings.h"
#include "../src/jobs.h"

int main() {
    World_Handle world = core_do_jobs_test(false);
    core_destroy_world(world);
    return 0;
}
