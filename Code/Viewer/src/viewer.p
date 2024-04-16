/* ------------------------------------------------- Modules ------------------------------------------------- */

#load "basic.p";
#load "window.p";
#load "ui.p";
#load "gfx/gfx.p";


/* ------------------------------------------------- Bindings ------------------------------------------------- */

#load "../../Core/bindings/bindings.p";


/* ---------------------------------------------- Viewer source ---------------------------------------------- */

#load "profiling_panel.p";
#load "draw.p";

//
// The viewer program obviously requires the Core dll, which is built into the Core subdirectory somewhere.
// To make sure we always have the latest version, copy the file during compilation of the viewer into
// the viewer's output directory.
// I think this is pretty cool.
//
copy_latest_dll :: #no_export () {
    input_file  := sprint(Default_Allocator, "%..\\..\\Core\\x64\\ReleaseDll\\Core.dll", compiler_get_output_folder_path());
    output_file := sprint(Default_Allocator, "%Core.dll", compiler_get_output_folder_path());
    if !copy_file(output_file, input_file, true) {
        print("Failed to copy DLL from '%' to '%'.\n", input_file, output_file);
    }
}

#run copy_latest_dll();

REQUESTED_FPS: f64 : 60;

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
    profiling_panel_state := UI_Window_State.Closed;
    profiling_panel_position := UI_Vector2.{ .5, .1 };

    // Core data.
    test_string: string;
    debug_draw_data: Debug_Draw_Data;    
    profiling_data: Timing_Data;
    profiling_show_summary: bool;    
}


menu_bar :: (viewer: *Viewer) {
    ui_push_width(*viewer.ui, .Pixels, 80, 1);
    ui_toggle_button_with_pointer(*viewer.ui, "Profiler", xx *viewer.profiling_panel_state);
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
    profiling_ui(viewer);

    //
    // Finish the frame.
    //
    gfx_finish_ui(*viewer.gfx, *viewer.ui);
    gfx_finish_frame(*viewer.gfx);

    reset_allocator(*viewer.frame_allocator);

    frame_end := get_hardware_time();
    window_ensure_frame_time(frame_start, frame_end, REQUESTED_FPS);
}

viewer_run_test :: (viewer: *Viewer, test_proc: () -> World_Handle, test_string: string) {
    viewer_destroy_test_data(viewer);

    core_begin_profiling();
    world := test_proc();
    core_stop_profiling();

    draw_options: Debug_Draw_Options : .Octree | .Anchors | .Boundaries;
    
    viewer.profiling_data  = core_get_profiling_data();
    viewer.debug_draw_data = core_debug_draw_world(world, draw_options);
    viewer.test_string     = test_string;

    set_window_name(*viewer.window, sprint(*viewer.frame_allocator, "Viewer: %", viewer.test_string));
}

viewer_destroy_test_data :: (viewer: *Viewer) {
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
    dump_gl_errors("GFX");

    create_renderer(*viewer.renderer, *viewer.window, *viewer.gfx, Default_Allocator);
    dump_gl_errors("Renderer");
    
    gfx_create_ui(*viewer.gfx, *viewer.ui, UI_Watermelon_Theme);

    dump_gl_errors("Setup");
    
    viewer_run_test(*viewer, core_do_octree_test, "octree_test");
//    viewer_run_test(*viewer, core_do_simple_test, "simple_test");
    
    while !viewer.window.should_close {
        one_viewer_frame(*viewer);
    }
        
    viewer_destroy_test_data(*viewer);
    
    gfx_destroy_ui(*viewer.gfx, *viewer.ui);
    destroy_renderer(*viewer.renderer, Default_Allocator);
    destroy_gfx(*viewer.gfx);
    destroy_window(*viewer.window);
    return 0;
}

/*
 * The command to compile and run this application is:
 * prometheus Viewer/src/viewer.p -o:Viewer/run_tree/viewer.exe -l:Core/x64/ReleaseDll/Core.lib -run
 *
 */
