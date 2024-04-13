#load "basic.p";
#load "window.p";
#load "../../Core/bindings/bindings.p";

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

main :: () -> s32 {
    do_simple_test();
    return 0;
}

#run import_latest_dll();

/*
 * The command to compile this application is:
 * prometheus Viewer/src/viewer.p -o:Viewer/run_tree/viewer.exe -l:Core/x64/ReleaseDll/Core.lib
 */
