/* ------------------------------------------------- Modules ------------------------------------------------- */

#load "basic.p";
#load "window.p";
#load "ui.p";
#load "gfx/gfx.p";


/* ------------------------------------------------- Bindings ------------------------------------------------- */

#load "../../Core/bindings/bindings.p";


/* ---------------------------------------------- Viewer source ---------------------------------------------- */

#load "profiling_panel.p";
#load "panels.p";
#load "draw.p";

//
// The viewer program obviously requires the Core dll, which is built into the Core subdirectory somewhere.
// To make sure we always have the latest version, copy the file during compilation of the viewer into
// the viewer's output directory.
// I think this is pretty cool.
//
copy_latest_dll :: #no_export () {
    use_debug_dll := compiler_command_line_option_present("debug_dll");

    dll_subpath: string = ---;
    
    if use_debug_dll {
        dll_subpath = "DebugDll\\Core.dll";
    } else {
        dll_subpath = "ReleaseDll\\Core.dll";
    }

    input_file  := sprint(Default_Allocator, "%..\\..\\Core\\x64\\%", compiler_get_output_folder_path(), dll_subpath);
    output_file := sprint(Default_Allocator, "%Core.dll", compiler_get_output_folder_path());
    if !copy_file(output_file, input_file, true) {
        print("Failed to copy DLL from '%' to '%'.\n", input_file, output_file);
    }
}

#run copy_latest_dll();

REQUESTED_FPS: f64 : 60;

Viewer_Test :: struct {
    name: string;
    proc: () -> World_Handle;
}

TESTS: []Viewer_Test : {
        .{ "house",         core_do_house_test },
        .{ "octree",        core_do_octree_test },
        .{ "large_volumes", core_do_large_volumes_test },
        .{ "cutout",        core_do_cutout_test },
};

STARTUP_TEST :: 0; // -1 means no startup test, else it is the index into the TESTS array.
// #assert(STARTUP_TEST >= -1 && STARTUP_TEST < TESTS.COUNT); // @Cleanup: This assert here makes the program not compile... Seems the type checker is broken.

Viewer :: struct {
    // Memory Management.
    frame_arena: Memory_Arena;
    frame_allocator: Allocator;

    // Display stuff.
    window: Window;
    gfx:    GFX;
    ui:     UI;
    renderer: Renderer;

    // UI stuff.
    select_test_panel_state := UI_Window_State.Closed;
    select_test_panel_position := UI_Vector2.{ .5, .2 };
    debug_draw_options_panel_state := UI_Window_State.Closed;
    debug_draw_options_panel_position := UI_Vector2.{ .5, .2 };
    profiling_panel_state := UI_Window_State.Closed;
    profiling_panel_position := UI_Vector2.{ .5, .1 };
    memory_panel_state := UI_Window_State.Closed;
    memory_panel_position := UI_Vector2.{ .5, .1 };
    
    // Core data.
    test_name: string;

    world_handle: World_Handle;
    debug_draw_data: Debug_Draw_Data;    
    profiling_data: Timing_Data;
    memory_information: Memory_Information;
    
    profiling_show_summary: bool;    
    debug_draw_options: Debug_Draw_Options = .Octree | .Anchors | .Boundaries | .Volume_Faces | .Labels;
}


menu_bar :: (viewer: *Viewer) {
    ui_push_width(*viewer.ui, .Pixels, 80, 1);
    ui_push_height(*viewer.ui, .Pixels, 20, 1);
    ui_toggle_button_with_pointer(*viewer.ui, "Test...", xx *viewer.select_test_panel_state);
    ui_toggle_button_with_pointer(*viewer.ui, "Views", xx *viewer.debug_draw_options_panel_state);
    ui_toggle_button_with_pointer(*viewer.ui, "Profiler", xx *viewer.profiling_panel_state);
    ui_toggle_button_with_pointer(*viewer.ui, "Memory", xx *viewer.memory_panel_state);
    ui_pop_height(*viewer.ui);
    ui_pop_width(*viewer.ui);
}

one_viewer_frame :: (viewer: *Viewer) {
    //
    // Prepare the frame.
    //
    frame_start := get_hardware_time();

    update_window(*viewer.window);

    background_color :: GFX_Color.{ 32, 80, 128, 255 };
    
    gfx_prepare_frame(*viewer.gfx, background_color);
    gfx_prepare_ui(*viewer.gfx, *viewer.ui);

    //
    // 3D Drawing.
    //
    draw_debug_draw_data(*viewer.renderer, *viewer.debug_draw_data, background_color);

    //
    // UI.
    //
    menu_bar(viewer);
    profiling_panel(viewer);
    memory_panel(viewer);
    select_test_panel(viewer);
    debug_draw_options_panel(viewer);
    
    //
    // Finish the frame.
    //
    gfx_finish_ui(*viewer.gfx, *viewer.ui);
    gfx_finish_frame(*viewer.gfx);

    reset_allocator(*viewer.frame_allocator);

    frame_end := get_hardware_time();
    window_ensure_frame_time(frame_start, frame_end, REQUESTED_FPS);
}

run_test :: (viewer: *Viewer, test_index: s64) {
    destroy_test_data(viewer);

    core_begin_profiling();
    viewer.world_handle = TESTS[test_index].proc();
    core_stop_profiling();
    
    viewer.debug_draw_data = core_debug_draw_world(viewer.world_handle, viewer.debug_draw_options);
    viewer.profiling_data  = core_get_profiling_data();
    viewer.memory_information = core_get_memory_information(viewer.world_handle);
    viewer.test_name       = TESTS[test_index].name;
    
    set_window_name(*viewer.window, sprint(*viewer.frame_allocator, "Viewer: %", viewer.test_name));
}

rebuild_debug_draw_data :: (viewer: *Viewer) {
    if viewer.world_handle == null return;

    core_free_debug_draw_data(*viewer.debug_draw_data);
    viewer.debug_draw_data = core_debug_draw_world(viewer.world_handle, viewer.debug_draw_options);    
}

destroy_test_data :: (viewer: *Viewer) {
    if viewer.world_handle core_destroy_world(viewer.world_handle);
    core_free_profiling_data(*viewer.profiling_data);
    core_free_debug_draw_data(*viewer.debug_draw_data);
}

main :: () -> s32 {
    enable_high_resolution_time();

    viewer: Viewer;
    create_memory_arena(*viewer.frame_arena, 4 * MEGABYTES);
    viewer.frame_allocator = memory_arena_allocator(*viewer.frame_arena);
    
    create_window(*viewer.window, "Viewer", WINDOW_DONT_CARE, WINDOW_DONT_CARE, WINDOW_DONT_CARE, WINDOW_DONT_CARE, .Default);
    show_window(*viewer.window);

    create_gfx(*viewer.gfx, *viewer.window, Default_Allocator);
    gfx_create_ui(*viewer.gfx, *viewer.ui, UI_Watermelon_Theme);
    create_renderer(*viewer.renderer, *viewer.window, *viewer.gfx, Default_Allocator, *viewer.frame_allocator);

    if STARTUP_TEST != -1 run_test(*viewer, STARTUP_TEST);
    
    while !viewer.window.should_close {
        one_viewer_frame(*viewer);
    }
        
    destroy_test_data(*viewer);
    
    gfx_destroy_ui(*viewer.gfx, *viewer.ui);
    destroy_renderer(*viewer.renderer, Default_Allocator);
    destroy_gfx(*viewer.gfx);
    destroy_window(*viewer.window);
    destroy_memory_arena(*viewer.frame_arena);
    return 0;
}

/*
 * The command to compile and run this application is:
 * prometheus Viewer/src/viewer.p -o:Viewer/run_tree/viewer.exe -l:Core/x64/ReleaseDll/Core.lib -run
 *
 */
