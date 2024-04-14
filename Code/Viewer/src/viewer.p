#load "basic.p";
#load "window.p";
#load "ui.p";
#load "gfx/gfx.p";
#load "../../Core/bindings/bindings.p";

#load "profiling_panel.p";

//
// The viewer program obviously requires the Core dll, which is built into the Core subdirectory somewhere.
// To make sure we always have the latest version, copy the file during compilation of the viewer into
// the viewer's output directory.
// I think this is pretty cool.
//
import_latest_dll :: #no_export () {
    input_file  := sprint(Default_Allocator, "%..\\..\\Core\\x64\\ReleaseDll\\Core.dll", compiler_get_output_folder_path());
    output_file := sprint(Default_Allocator, "%Core.dll", compiler_get_output_folder_path());
    if !copy_file(output_file, input_file, true) {
        print("Failed to copy DLL from '%' to '%'.\n", input_file, output_file);
    }
}


Viewer :: struct {
    window: Window;
    gfx: GFX;
    font: Font;

    profiling_data: Timing_Data;
}


main :: () -> s32 {
    viewer: Viewer;

    create_window(*viewer.window, "Viewer", WINDOW_DONT_CARE, WINDOW_DONT_CARE, WINDOW_DONT_CARE, WINDOW_DONT_CARE, .Default);
    show_window(*viewer.window);

    create_gfx(*viewer.gfx, *viewer.window, Default_Allocator);
    gfx_create_font_from_file(*viewer.font, "C:/Windows/Fonts/times.ttf", 25, true);
    
    core_begin_profiling();
    core_do_simple_test();
    core_stop_profiling();
    viewer.profiling_data = core_get_profiling_data();
    
    while !viewer.window.should_close {
        update_window(*viewer.window);

        gfx_prepare_frame(*viewer.gfx, .{ 100, 100, 100, 255 });
        draw_profiling(*viewer);
        gfx_finish_frame(*viewer.gfx);
    }
        
    core_free_profiling_data(*viewer.profiling_data);

    gfx_destroy_font(*viewer.font);
    destroy_gfx(*viewer.gfx);
    destroy_window(*viewer.window);
    return 0;
}

#run import_latest_dll();

/*
 * The command to compile this application is:
 * prometheus Viewer/src/viewer.p -o:Viewer/run_tree/viewer.exe -l:Core/x64/ReleaseDll/Core.lib
 */
