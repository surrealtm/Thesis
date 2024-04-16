/*
 * This file is a manual translation of the bindings.h file. It creates prometheus source code definitions
 * for all exported code of the Core library, so that it can be used in the Viewer code.
 */

/* ----------------------------------------------- General API ----------------------------------------------- */

World_Handle :: *void;

core_destroy_world :: #foreign (world: World_Handle);



/* ------------------------------------------------- Testing ------------------------------------------------- */

core_do_simple_test :: #foreign () -> World_Handle;
core_do_octree_test :: #foreign () -> World_Handle;



/* ------------------------------------------------ Debug Draw ------------------------------------------------ */

Debug_Draw_Options :: enum {
    Nothing    :: 0x0;
    Octree     :: 0x1;
    Anchors    :: 0x2;
    Boundaries :: 0x4;
    Labels     :: 0x1000;
    Everything :: 0xffffffff;
}

Debug_Draw_Line :: struct {
    p0, p1: v3f;
    thickness: f32;
    r, g, b: u8;
}

Debug_Draw_Text :: struct {
    position: v3f;
    text: string;
    r, g, b: u8;
}

Debug_Draw_Cuboid :: struct {
    position: v3f;
    size: v3f;
    r, g, b: u8;
}

Debug_Draw_Data :: struct {
    lines: *Debug_Draw_Line;
    line_count: s64;

    texts: *Debug_Draw_Text;
    text_count: s64;

    cuboids: *Debug_Draw_Cuboid;
    cuboid_count: s64;
}

core_debug_draw_world :: #foreign (world: World_Handle, options: Debug_Draw_Options) -> Debug_Draw_Data;
core_free_debug_draw_data :: #foreign (data: *Debug_Draw_Data);



/* ------------------------------------------------ Profiling ------------------------------------------------ */


Timing_Timeline_Entry :: struct {
    name: string;
    start_in_nanoseconds, end_in_nanoseconds: s64; // Relative to the start of the timing data
    depth: s64; // The vertical depth of the entry, representing the call stack depth
    r, g, b: u8;
};

Timing_Summary_Entry :: struct {
    name: string;
    inclusive_time_in_nanoseconds: s64;
    exclusive_time_in_nanoseconds: s64;
    count: s64;
};

Timing_Data :: struct {
    timeline: *Timing_Timeline_Entry;
    timeline_count: s64;
    summary: *Timing_Summary_Entry;
    summary_count: s64;

    total_time_in_nanoseconds: s64;
};

core_begin_profiling     :: #foreign ();
core_stop_profiling      :: #foreign ();
core_print_profiling     :: #foreign ();
core_get_profiling_data  :: #foreign () -> Timing_Data;
core_free_profiling_data :: #foreign (data: *Timing_Data);
