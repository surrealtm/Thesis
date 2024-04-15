#load "basic.p";
#load "window.p";
#load "ui.p";
#load "gfx/gfx.p";
#load "../../Core/bindings/bindings.p";

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
    frame_arena: Memory_Arena;
    frame_allocator: Allocator;

    window: Window;
    gfx:    GFX;
    ui:     UI;
    renderer: Renderer;
    
    profiling_data: Timing_Data;
    profiling_string: string;
    profiling_show_summary: bool;
    
    profiling_panel_state := UI_Window_State.Closed;
    profiling_panel_position := UI_Vector2.{ .5, .1 };
}


menu_bar :: (viewer: *Viewer) {
    ui_push_width(*viewer.ui, .Pixels, 80, 1);
    ui_toggle_button_with_pointer(*viewer.ui, "Profiling", xx *viewer.profiling_panel_state);
    ui_pop_width(*viewer.ui);
}

one_viewer_frame :: (viewer: *Viewer) {
    //
    // Prepare the frame.
    //
    frame_start := get_hardware_time();

    update_window(*viewer.window);
    
    gfx_prepare_frame(*viewer.gfx, .{ 50, 50, 50, 255 });
    gfx_prepare_ui(*viewer.gfx, *viewer.ui);

    //
    // Immediate mode 3D drawing.
    //
    {
        update_camera(*viewer.renderer);
        draw_line(*viewer.renderer, .{ 0, 0, -10 }, .{ 0, 10, -10 }, 1, .{ 255, 255, 255, 255 });
        flush_lines(*viewer.renderer);
    }

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

profile_octree_test :: (viewer: *Viewer) {
    core_begin_profiling();
    core_do_octree_test();
    core_stop_profiling();
    viewer.profiling_data = core_get_profiling_data();
    viewer.profiling_string = "octree_test";
}

profile_simple_test :: (viewer: *Viewer) {
    core_begin_profiling();
    core_do_simple_test();
    core_stop_profiling();
    viewer.profiling_data = core_get_profiling_data();
    viewer.profiling_string = "simple_test";
}

main :: () -> s32 {
    enable_high_resolution_time();

    viewer: Viewer;
    create_memory_arena(*viewer.frame_arena, 4 * MEGABYTES);
    viewer.frame_allocator = memory_arena_allocator(*viewer.frame_arena);
    
    create_window(*viewer.window, "Viewer", WINDOW_DONT_CARE, WINDOW_DONT_CARE, WINDOW_DONT_CARE, WINDOW_DONT_CARE, .Default);
    show_window(*viewer.window);

    create_gfx(*viewer.gfx, *viewer.window, Default_Allocator);
    create_renderer(*viewer.renderer, *viewer.window, Default_Allocator);
    
    gfx_create_ui(*viewer.gfx, *viewer.ui, UI_Dark_Theme);

    profile_octree_test(*viewer);
    //profile_simple_test(*viewer);
    
    while !viewer.window.should_close {
        one_viewer_frame(*viewer);
    }
        
    core_free_profiling_data(*viewer.profiling_data);

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
