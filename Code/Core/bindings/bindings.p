/*
 * This file is a manual translation of the bindings.h file. It creates prometheus source code definitions
 * for all exported code of the Core library, so that it can be used in the Viewer code.
 */

/* ----------------------------------------------- General API ----------------------------------------------- */

World_Handle :: *void;

core_create_world ::  #foreign (x: f64, y: f64, z: f64) -> World_Handle;
core_destroy_world :: #foreign (world: World_Handle);



/* ------------------------------------------------- Testing ------------------------------------------------- */

core_do_world_step :: #foreign (world: World_Handle, step_mode: bool);

core_do_house_test         :: #foreign (step_into: bool) -> World_Handle;
core_do_bvh_test           :: #foreign (step_into: bool) -> World_Handle;
core_do_large_volumes_test :: #foreign (step_into: bool) -> World_Handle;
core_do_cutout_test        :: #foreign (step_into: bool) -> World_Handle;
core_do_circle_test        :: #foreign (step_into: bool) -> World_Handle;
core_do_u_shape_test       :: #foreign (step_into: bool) -> World_Handle;
core_do_center_block_test  :: #foreign (step_into: bool) -> World_Handle;

core_do_jobs_test :: #foreign (step_into: bool) -> World_Handle;



/* ------------------------------------------------ Debug Draw ------------------------------------------------ */

Debug_Draw_Options :: enum {
    Nothing             :: 0x0;
    BVH                 :: 0x1;
    Anchors             :: 0x2;
    Boundaries          :: 0x4;
    Clipping_Faces      :: 0x8;
    Clipping_Wireframes :: 0x10;
    Volume_Faces        :: 0x20;
    Volume_Wireframes   :: 0x40;
    Labels              :: 0x1000;
    Normals             :: 0x2000;
    Axis_Gizmo          :: 0x4000;
    Root_Planes         :: 0x8000;
    Flood_Fill          :: 0x10000;
    Everything          :: 0xffffffff;
}

Debug_Draw_Line :: struct {
    p0, p1: v3f;
    thickness: f32;
    r, g, b: u8;
}

Debug_Draw_Triangle :: struct {
    p0, p1, p2: v3f;
    r, g, b, a: u8;
}

Debug_Draw_Text :: struct {
    position: v3f;
    text: string;
    r, g, b: u8;
}

Debug_Draw_Cuboid :: struct {
    position: v3f;
    size: v3f;
    rotation: quatf;
    r, g, b: u8;
}

Debug_Draw_Sphere :: struct {
    position: v3f;
    radius: f32;
    r, g, b: u8;
}

Debug_Draw_Data :: struct {
    lines: *Debug_Draw_Line;
    line_count: s64;

    triangles: *Debug_Draw_Triangle;
    triangle_count: s64;
    
    texts: *Debug_Draw_Text;
    text_count: s64;

    cuboids: *Debug_Draw_Cuboid;
    cuboid_count: s64;

    spheres: *Debug_Draw_Sphere;
    sphere_count: s64;
}

core_debug_draw_world :: #foreign (world: World_Handle, options: Debug_Draw_Options) -> Debug_Draw_Data;
core_free_debug_draw_data :: #foreign (data: *Debug_Draw_Data);



/* ------------------------------------------------ Profiling ------------------------------------------------ */


Timing_Timeline_Entry :: struct {
    name: string;
    start_in_nanoseconds, end_in_nanoseconds: s64; // Relative to the start of the timing data
    depth: s64; // The vertical depth of the entry, representing the call stack depth
    r, g, b: u8;
}

Timing_Summary_Entry :: struct {
    name: string;
    inclusive_time_in_nanoseconds: s64;
    exclusive_time_in_nanoseconds: s64;
    count: s64;
}

Timing_Data :: struct {
    timelines: **Timing_Timeline_Entry;
    timelines_entry_count: *s64;
    timelines_count: s64;
    
    summary: *Timing_Summary_Entry;
    summary_count: s64;

    total_time_in_nanoseconds: s64;
    total_overhead_time_in_nanoseconds: s64;
    total_overhead_space_in_bytes: s64;
}

core_begin_profiling     :: #foreign ();
core_stop_profiling      :: #foreign ();
core_print_profiling     :: #foreign (include_timeline: bool);
core_get_profiling_data  :: #foreign () -> Timing_Data;
core_free_profiling_data :: #foreign (data: *Timing_Data);


Memory_Allocator_Information :: struct {
    name: string;
    allocation_count: s64;
    deallocation_count: s64;
    working_set_size: s64;
    peak_working_set_size: s64;
}

Memory_Information :: struct {
    os_working_set_size: s64;
    allocators: *Memory_Allocator_Information;
    allocator_count: s64;
}

core_get_memory_information :: #foreign (world_handle: World_Handle) -> Memory_Information;
core_free_memory_information :: #foreign (info: *Memory_Information);

core_enable_memory_tracking :: #foreign ();
core_disable_memory_tracking :: #foreign ();
