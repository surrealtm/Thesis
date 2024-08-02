#include "../bindings/bindings.h"
#include "serialized_setup.cpp"
#include "os_specific.h"
#include "memutils.h"

int main() {
    Hardware_Time start = os_get_hardware_time();
    setup_world(); // Serialized from unity!
    Hardware_Time end = os_get_hardware_time();

    printf("Test case took %.2f, %.2fmb.\n", os_convert_hardware_time(end - start, Seconds), convert_to_memory_unit(os_get_working_set_size(), Megabytes));
    
    return 0;
}
