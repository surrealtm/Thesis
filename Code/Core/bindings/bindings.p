/*
 * This file is a manual translation of the bindings.h file. It creates prometheus source code definitions
 * for all exported code of the Core library, so that it can be used in the Viewer code.
 */

Timing_Timeline_Entry :: struct {
    name: cstring;
    relative_start, relative_end: f64; // Relative to the entire time span, meaning in the interval [0,1]
    time_in_seconds: f64;
    depth: s64; // The vertical depth of the entry, representing the call stack depth
    r, g, b: u8;
};

Timing_Summary_Entry :: struct {
    name: cstring;
    inclusive_time_in_seconds: f64;
    exclusive_time_in_seconds: f64;
    count: s64;
};

Timing_Data :: struct {
    timeline: *Timing_Timeline_Entry;
    timeline_count: s64;
    summary: *Timing_Summary_Entry;
    summary_count: s64;

    total_time_in_seconds: f64; // To make it easier to port
};


/* ------------------------------------------------- Testing ------------------------------------------------- */

core_do_simple_test :: #foreign ();
core_do_octree_test :: #foreign ();



/* ------------------------------------------------ Profiling ------------------------------------------------ */

core_begin_profiling     :: #foreign ();
core_stop_profiling      :: #foreign ();
core_print_profiling     :: #foreign ();
core_get_profiling_data  :: #foreign () -> Timing_Data;
core_free_profiling_data :: #foreign (data: *Timing_Data);