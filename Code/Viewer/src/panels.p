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

        new_options = debug_draw_option_box(viewer, new_options, "BVH", .BVH);
        new_options = debug_draw_option_box(viewer, new_options, "Anchors", .Anchors);
        new_options = debug_draw_option_box(viewer, new_options, "Boundaries", .Boundaries);
        new_options = debug_draw_option_box(viewer, new_options, "Clipping Faces", .Clipping_Faces);
        new_options = debug_draw_option_box(viewer, new_options, "Clipping Wireframes", .Clipping_Wireframes);
        new_options = debug_draw_option_box(viewer, new_options, "Volume Faces", .Volume_Faces);
        new_options = debug_draw_option_box(viewer, new_options, "Volume Wireframes", .Volume_Wireframes);
        new_options = debug_draw_option_box(viewer, new_options, "Labels", .Labels);
        new_options = debug_draw_option_box(viewer, new_options, "Normals", .Normals);
        new_options = debug_draw_option_box(viewer, new_options, "Axis", .Axis_Gizmo);
        new_options = debug_draw_option_box(viewer, new_options, "Root Planes", .Root_Planes);
        new_options = debug_draw_option_box(viewer, new_options, "Flood Fill", .Flood_Fill);

        ui_divider(*viewer.ui, true);
        
        if ui_button(*viewer.ui, "Everything") new_options = .Everything;

        ui_divider(*viewer.ui, false);
        
        if ui_button(*viewer.ui, "Nothing") new_options = .Nothing;

        ui_divider(*viewer.ui, true);

        if ui_button(*viewer.ui, "Export Flags") {
            builder: String_Builder;
            builder.allocator = *viewer.frame_allocator;

            if new_options & .BVH                   append_string(*builder, " | .BVH");
            if new_options & .Anchors               append_string(*builder, " | .Anchors");
            if new_options & .Boundaries            append_string(*builder, " | .Boundaries");
            if new_options & .Clipping_Faces        append_string(*builder, " | .Clipping_Faces");
            if new_options & .Clipping_Wireframes   append_string(*builder, " | .Clipping_Wireframes");
            if new_options & .Volume_Faces          append_string(*builder, " | .Volume_Faces");
            if new_options & .Volume_Wireframes     append_string(*builder, " | .Volume_Wireframes");
            if new_options & .Labels                append_string(*builder, " | .Labels");
            if new_options & .Normals               append_string(*builder, " | .Normals");
            if new_options & .Axis_Gizmo            append_string(*builder, " | .Axis_Gizmo");
            if new_options & .Root_Planes           append_string(*builder, " | .Root_Planes");
            if new_options & .Flood_Fill            append_string(*builder, " | .Flood_Fill");

            complete_string := finish_string_builder(*builder);
            complete_string = substring_view(complete_string, 3, complete_string.count); // Cut out the leading ' | '
            set_clipboard_data(complete_string);
        }
        
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

        viewer_set, viewer_set_unit := get_best_memory_unit(Default_Allocator.stats.working_set);
        ui_label(*viewer.ui, false, "Viewer Heap: %*%", memory_format(viewer_set), memory_unit_suffix(viewer_set_unit));
    }
    ui_pop_window(*viewer.ui);
}
