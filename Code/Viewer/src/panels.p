select_test_panel :: (viewer: *Viewer) {
    if viewer.select_test_panel_state == .Closed return;

    viewer.select_test_panel_position, viewer.select_test_panel_state = ui_push_window(*viewer.ui, "Select Test", viewer.select_test_panel_position, .Draggable | .Collapsable | .Closeable);
    if viewer.select_test_panel_state == .Open {    
        ui_push_width(*viewer.ui, .Pixels, 256, 1);
        for i := 0; i < TESTS.count; ++i {
            if ui_button(*viewer.ui, TESTS[i].name) {
                run_test(viewer, TESTS[i].proc, TESTS[i].name);
                viewer.select_test_panel_state = .Closed;
            }
        }
        ui_pop_width(*viewer.ui);
    }
        
    ui_pop_window(*viewer.ui);
}

debug_draw_option_box :: (viewer: *Viewer, options: Debug_Draw_Options, name: string, flag: Debug_Draw_Options) -> Debug_Draw_Options{
    flag_is_set: bool = (options & flag) != 0;
    
    if ui_check_box(*viewer.ui, name, *flag_is_set){
        if flag_is_set {
            options |= flag;
        } else {
            options ^= flag;
        }
    }

    return options;
}

debug_draw_options_panel :: (viewer: *Viewer) {
    if viewer.debug_draw_options_panel_state == .Closed return;

    viewer.debug_draw_options_panel_position, viewer.debug_draw_options_panel_state = ui_push_window(*viewer.ui, "Debug Views", viewer.debug_draw_options_panel_position, .Draggable | .Collapsable | .Closeable);
    if viewer.debug_draw_options_panel_state == .Open {
        previous_options := viewer.debug_draw_options;
        new_options := previous_options;

        ui_push_width(*viewer.ui, .Pixels, 256, 1);

        new_options = debug_draw_option_box(viewer, new_options, "Octree", .Octree);
        new_options = debug_draw_option_box(viewer, new_options, "Anchors", .Anchors);
        new_options = debug_draw_option_box(viewer, new_options, "Boundaries", .Boundaries);
        new_options = debug_draw_option_box(viewer, new_options, "Labels", .Labels);

        ui_divider(*viewer.ui, true);
        
        if ui_button(*viewer.ui, "Everything") new_options = .Everything;

        ui_divider(*viewer.ui, false);
        
        if ui_button(*viewer.ui, "Nothing") new_options = .Nothing;
        
        ui_pop_width(*viewer.ui);

        if new_options != previous_options {
            viewer.debug_draw_options = new_options;
            rebuild_debug_draw_data(viewer);
        }
    }

    ui_pop_window(*viewer.ui);
}
