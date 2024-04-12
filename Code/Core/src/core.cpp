#include "core.h"
#include "string.h"
#include "os_specific.h"
#include "memutils.h"

int main() {
    //
    // Set the default working directory to be fixed so that we can rely on relative paths.
    //
    string module_path = os_get_executable_directory();
    os_set_working_directory(module_path);
    deallocate_string(Default_Allocator, &module_path);

    //
    // We're done.
    //
    return 0;
}
