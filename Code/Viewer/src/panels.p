select_test_panel :: (viewer: *Viewer) {
    if viewer.select_test_panel_state == .Closed return;

    viewer.select_test_panel_position, viewer.select_test_panel_state = ui_push_window(*viewer.ui, "Select Test", viewer.select_test_panel_position, .Draggable | .Collapsable | .Closeable);
    if viewer.select_test_panel_state == .Open {    
        ui_push_width(*viewer.ui, .Pixels, 256, 1);
        for i := 0; i < TESTS.count; ++i {
            if ui_button(*viewer.ui, TESTS[i].name) {
                run_test(viewer, i);
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
        new_options = debug_draw_option_box(viewer, new_options, "Clipping Plane Faces", .Clipping_Plane_Faces);
        new_options = debug_draw_option_box(viewer, new_options, "Clipping Plane Wireframes", .Clipping_Plane_Wireframes);
        new_options = debug_draw_option_box(viewer, new_options, "Volume Faces", .Volume_Faces);
        new_options = debug_draw_option_box(viewer, new_options, "Volume Wireframes", .Volume_Wireframes);
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

memory_panel :: (viewer: *Viewer) {
    if viewer.memory_panel_state == .Closed return;
    
    name := sprint(*viewer.frame_allocator, "Memory Usage : %", viewer.test_name);
    viewer.memory_panel_position, viewer.memory_panel_state = ui_push_window(*viewer.ui, name, viewer.profiling_panel_position, .Draggable | .Collapsable | .Closeable);
    if viewer.memory_panel_state != .Collapsed {
        width: f32 : 750;

        ui_push_width(*viewer.ui, .Pixels, width, 1);
        
        {
            ui_push_fixed_container(*viewer.ui, .Horizontal);
            ui_set_width(*viewer.ui, .Pixels, 5, 1);
            ui_spacer(*viewer.ui);
            ui_set_width(*viewer.ui, .Percentage_Of_Parent, .2, .8);
            ui_label(*viewer.ui, false, "Name");
            ui_set_width(*viewer.ui, .Percentage_Of_Parent, .2, .8);
            ui_label(*viewer.ui, false, "Allocations");
            ui_set_width(*viewer.ui, .Percentage_Of_Parent, .2, .8);
            ui_label(*viewer.ui, false, "Deallocations");
            ui_set_width(*viewer.ui, .Percentage_Of_Parent, .2, .8);
            ui_label(*viewer.ui, false, "Working Set");
            ui_set_width(*viewer.ui, .Percentage_Of_Parent, .2, .8);
            ui_label(*viewer.ui, false, "Peak Working Set");
            ui_pop_container(*viewer.ui);
        }
        
        ui_divider(*viewer.ui, true);
        
        for i := 0; i < viewer.memory_information.allocator_count; ++i {
            allocator := *viewer.memory_information.allocators[i];
            working_set, working_set_unit := get_best_memory_unit(allocator.working_set_size);
            peak_working_set, peak_working_set_unit := get_best_memory_unit(allocator.peak_working_set_size);
            
            ui_push_fixed_container(*viewer.ui, .Horizontal);
            ui_set_width(*viewer.ui, .Pixels, 5, 1);
            ui_spacer(*viewer.ui);
            ui_set_width(*viewer.ui, .Percentage_Of_Parent, .2, .8);
            ui_label(*viewer.ui, false, allocator.name);
            ui_set_width(*viewer.ui, .Percentage_Of_Parent, .2, .8);
            ui_label(*viewer.ui, false, "%", allocator.allocation_count);
            ui_set_width(*viewer.ui, .Percentage_Of_Parent, .2, .8);
            ui_label(*viewer.ui, false, "%", allocator.deallocation_count);
            ui_set_width(*viewer.ui, .Percentage_Of_Parent, .2, .8);
            ui_label(*viewer.ui, false, "%*%", memory_format(working_set), memory_unit_suffix(working_set_unit));
            ui_set_width(*viewer.ui, .Percentage_Of_Parent, .2, .8);
            ui_label(*viewer.ui, false, "%*%", memory_format(peak_working_set), memory_unit_suffix(peak_working_set_unit));
            ui_pop_container(*viewer.ui);
        }

        ui_divider(*viewer.ui, true);
        ui_divider(*viewer.ui, true);

        ui_pop_width(*viewer.ui);
        
        working_set, working_set_unit := get_best_memory_unit(viewer.memory_information.os_working_set_size);
        ui_label(*viewer.ui, false, "OS working set: %*%", memory_format(working_set), memory_unit_suffix(working_set_unit));
    }
    ui_pop_window(*viewer.ui);
}
