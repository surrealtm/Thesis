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

BUILD_CORE :: false || #run compiler_command_line_option_present("build_core");
USE_DEBUG_DLL :: false || #run compiler_command_line_option_present("debug_dll");

#if USE_DEBUG_DLL {
    #library "Core\\x64\\DebugDll\\Core.lib";
} #else {
    #library "Core\\x64\\ReleaseDll\\Core.lib";
}

compile_core :: #no_export() {
    if USE_DEBUG_DLL {
        system("MSBuild.exe Core\\Core.sln /property:Configuration=DebugDll");
    } else {
        system("MSBuild.exe Core\\Core.sln /property:Configuration=ReleaseDll");
    }
}

//
// The viewer program obviously requires the Core dll, which is built into the Core subdirectory somewhere.
// To make sure we always have the latest version, copy the file during compilation of the viewer into
// the viewer's output directory.
// I think this is pretty cool.
//
copy_latest_dll :: #no_export () {
    dll_subpath: string = ---;
    
    if USE_DEBUG_DLL {
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

#run {
//    if BUILD_CORE compile_core(); // @Cleanup: This breaks the compiler...
    copy_latest_dll();
}

REQUESTED_FPS: f64 : 60;

Viewer_Test :: struct {
    name: string;
    proc: (step_into: bool) -> World_Handle;
}

TESTS: []Viewer_Test : {
        .{ "house",         core_do_house_test },
        .{ "octree",        core_do_octree_test },
        .{ "large_volumes", core_do_large_volumes_test },
        .{ "cutout",        core_do_cutout_test },
        .{ "circle",        core_do_circle_test },
        .{ "u_shape",       core_do_u_shape_test },
        .{ "center_block",  core_do_center_block_test },
        .{ "jobs",          core_do_jobs_test },
};

STARTUP_TEST :: -1; // -1 means no startup test, else it is the index into the TESTS array.
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
    stepping_mode:   bool = false;
    memory_tracking: bool = false;
    
    world_handle: World_Handle;
    debug_draw_data: Debug_Draw_Data;    
    profiling_data: Timing_Data;
    memory_information: Memory_Information;
    
    profiling_show_summary: bool;    
    debug_draw_options: Debug_Draw_Options = .Octree | .Anchors | .Boundaries | .Clipping_Wireframes | .Volume_Faces | .Volume_Wireframes/* | .Flood_Fill*/;
}


menu_bar :: (viewer: *Viewer) {
    ui_push_width(*viewer.ui, .Pixels, 80, 1);
    ui_push_height(*viewer.ui, .Pixels, 20, 1);

    ui_toggle_button_with_pointer(*viewer.ui, "Test...", xx *viewer.select_test_panel_state);
    ui_toggle_button_with_pointer(*viewer.ui, "Views", xx *viewer.debug_draw_options_panel_state);
    ui_toggle_button_with_pointer(*viewer.ui, "Profiler", xx *viewer.profiling_panel_state);
    ui_toggle_button_with_pointer(*viewer.ui, "Memory", xx *viewer.memory_panel_state);
    if viewer.world_handle && ui_button(*viewer.ui, "Destroy") destroy_test_data(viewer);
    
    ui_set_width(*viewer.ui, .Percentage_Of_Parent, 1, 0);
    ui_spacer(*viewer.ui);

    if viewer.stepping_mode && viewer.world_handle && (ui_button(*viewer.ui, "Step") || viewer.window.key_repeated[.Space]) {
        core_do_world_step(viewer.world_handle, viewer.stepping_mode);
        rebuild_debug_draw_data(viewer);
    }
    
    ui_toggle_button_with_pointer(*viewer.ui, "Step Mode", *viewer.stepping_mode);

    ui_set_width(*viewer.ui, .Label_Size, 10, 1);
    if ui_toggle_button_with_pointer(*viewer.ui, "Track Memory", *viewer.memory_tracking) {
        if viewer.memory_tracking {
            core_enable_memory_tracking();
        } else {
            core_disable_memory_tracking();
        }
    }
    
    ui_pop_height(*viewer.ui);
    ui_pop_width(*viewer.ui);

    if viewer.window.key_pressed[.Escape] viewer.select_test_panel_state = !viewer.select_test_panel_state;
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
    // Create the UI.
    //
    menu_bar(viewer);
    profiling_panel(viewer);
    memory_panel(viewer);
    select_test_panel(viewer);
    debug_draw_options_panel(viewer);
    

    //
    // Draw the UI.
    //
    gfx_finish_ui(*viewer.gfx, *viewer.ui);

    {
        frame_info := sprint(*viewer.frame_allocator, "FPS: %, Time: %ms", cast(s64) (1 / viewer.window.frame_time), cast(s64) (viewer.window.frame_time * 1000));
        gfx_draw_text_without_background(*viewer.gfx, *viewer.gfx.ui_font, frame_info, .{ xx viewer.window.width - 5, xx viewer.window.height - 5 }, .Right, .{ 255, 255, 255, 255 });
        gfx_flush_text(*viewer.gfx);
    }

    //
    // Finish the frame.
    //    
    gfx_finish_frame(*viewer.gfx);

    reset_allocator(*viewer.frame_allocator);

    frame_end := get_hardware_time();
    window_ensure_frame_time(frame_start, frame_end, REQUESTED_FPS);
}

run_test :: (viewer: *Viewer, test_index: s64) {
    destroy_test_data(viewer);

    core_begin_profiling();
    viewer.world_handle = TESTS[test_index].proc(viewer.stepping_mode);
    core_stop_profiling();

    if viewer.world_handle != null {
        viewer.debug_draw_data    = core_debug_draw_world(viewer.world_handle, viewer.debug_draw_options);
        viewer.memory_information = core_get_memory_information(viewer.world_handle);
    }
    
    viewer.profiling_data  = core_get_profiling_data();
    viewer.test_name       = TESTS[test_index].name;
    
    set_window_name(*viewer.window, sprint(*viewer.frame_allocator, "Viewer: %", viewer.test_name));
}

rebuild_debug_draw_data :: (viewer: *Viewer) {
    if viewer.world_handle == null return;

    core_free_debug_draw_data(*viewer.debug_draw_data);
    viewer.debug_draw_data = core_debug_draw_world(viewer.world_handle, viewer.debug_draw_options);    
}

destroy_test_data :: (viewer: *Viewer) {
    core_destroy_world(viewer.world_handle);
    core_free_profiling_data(*viewer.profiling_data);
    core_free_debug_draw_data(*viewer.debug_draw_data);
    core_free_memory_information(*viewer.memory_information);
    viewer.world_handle = null;
}

main :: () -> s32 {
    enable_high_resolution_time();

    viewer: Viewer;
    create_memory_arena(*viewer.frame_arena, 4 * MEGABYTES);
    viewer.frame_allocator = memory_arena_allocator(*viewer.frame_arena);
    
    create_window(*viewer.window, "Viewer", WINDOW_DONT_CARE, WINDOW_DONT_CARE, WINDOW_DONT_CARE, WINDOW_DONT_CARE, .Default);
    show_window(*viewer.window);

    create_gfx(*viewer.gfx, *viewer.window, Default_Allocator);
    gfx_create_ui(*viewer.gfx, *viewer.ui, UI_Watermelon_Theme, 16);
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
 * prometheus Viewer/src/viewer.p -o:Viewer/run_tree/viewer.exe -run
 *
 */
