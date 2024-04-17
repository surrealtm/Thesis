PROFILING_BAR_HEIGHT: f64 : 32;
PROFILING_ZOOM_STEP : f32 : 1.2;

Profiling_UI_Data :: struct {
    viewer: *Viewer;
    target_zoom:    f32;
    current_zoom:   f32;
    zoom_index:     f32;
    target_center:  f32;
    current_center: f32;
}

get_best_time_resolution :: (nanoseconds: s64) -> f64, Time_Resolution {
    if nanoseconds <= 0 return 0, .Nanoseconds;

    adjusted_time: f64 = xx nanoseconds;
    
    resolution := Time_Resolution.Nanoseconds;
    while adjusted_time >= 1000 {
        adjusted_time /= 1000;
        --resolution;
    }

    return adjusted_time, resolution;
}

combine_scissors :: (gfx: *GFX, tl1: v2f, br1: v2f, tl2: v2f, br2: v2f) {
    tl := v2f.{ max(tl1.x, tl2.x), max(tl1.y, tl2.y) };
    br := v2f.{ min(br1.x, br2.x), min(br1.y, br2.y) };

    gfx_set_scissors(gfx, tl, br);
}

get_profiling_view_port :: (element: *UI_Element, data: *Profiling_UI_Data, current: bool) -> f64, f64, f64, f64 {
    if current {
        w:  f64 = xx (element.screen_size.x * data.current_zoom);
        h:  f64 = xx (element.screen_size.y);
        x0: f64 = xx (element.screen_position.x + element.screen_size.x / 2 - data.current_center);
        y0: f64 = xx (element.screen_position.y); 
        return w, h, x0, y0;
    } else {
        w:  f64 = xx (element.screen_size.x * data.target_zoom);
        h:  f64 = xx (element.screen_size.y);
        x0: f64 = xx (element.screen_position.x + element.screen_size.x / 2 - data.target_center);
        y0: f64 = xx (element.screen_position.y); 
        return w, h, x0, y0;
    }
}

draw_profiling_marker :: (gfx: *GFX, element: *UI_Element, x: f64, h: f32) {
    marker_color :: GFX_Color.{ 150, 150, 150, 255 };       

    _x: f32 = cast(f32) x;
    if _x >= element.screen_position.x && _x <= element.screen_position.x + element.screen_size.x {
        gfx_draw_line(gfx, .{ _x, element.screen_position.y }, .{ _x, element.screen_position.y + xx h }, 1, marker_color);
    }
}

draw_profiling_timeline :: (*void, element: *UI_Element, data: *Profiling_UI_Data) {
    w, h, x0, y0 := get_profiling_view_port(element, data, true);

    viewer := data.viewer;
    gfx := *data.viewer.gfx;

    available_width: f64 = xx w;
    total_time_in_nanoseconds: f64 = xx data.viewer.profiling_data.total_time_in_nanoseconds;
    width_per_time_unit: [Time_Resolution.count]f64 = {
        (available_width / (total_time_in_nanoseconds / 60000000000.)), // Minutes
        (available_width / (total_time_in_nanoseconds / 1000000000.)),  // Seconds
        (available_width / (total_time_in_nanoseconds / 1000000.)),     // Milliseconds
        (available_width / (total_time_in_nanoseconds / 1000.)),        // Microseconds
        (available_width / (total_time_in_nanoseconds / 1)),            // Nanoseconds
    };
    
    //
    // Render the actual timeline.
    //
    low_x_clamp : f64 = xx (element.screen_position.x);
    high_x_clamp: f64 = xx (element.screen_position.x + element.screen_size.x);

    for i := 0; i < viewer.profiling_data.timeline_count; ++i {
        entry  := *viewer.profiling_data.timeline[i];

        start_time, start_time_unit       := get_best_time_resolution(entry.start_in_nanoseconds);
        end_time, end_time_unit           := get_best_time_resolution(entry.end_in_nanoseconds);
        duration_time, duration_time_unit := get_best_time_resolution(entry.end_in_nanoseconds - entry.start_in_nanoseconds);
        
        entry_x0: f64 = round(x0 + start_time * width_per_time_unit[start_time_unit]);
        entry_y0: f64 = round(y0 + (cast(f64) entry.depth + 2) * PROFILING_BAR_HEIGHT);

        entry_x1: f64 = round(x0 + end_time * width_per_time_unit[end_time_unit]);
        entry_y1: f64 = round(y0 + (cast(f64) entry.depth + 3) * PROFILING_BAR_HEIGHT);
        
        if (xx entry_x1 < element.screen_position.x) || (xx entry_x0 > element.screen_position.x + element.screen_size.x) continue;
        
        if duration_time * width_per_time_unit[duration_time_unit] > 2 {
            entry_x0 = clamp(entry_x0, low_x_clamp, high_x_clamp);
            entry_x1 = clamp(entry_x1, low_x_clamp, high_x_clamp);
            
            text   := v2f.{ xx (entry_x0),                xx (entry_y1 + xx gfx.ui_font.descender) };
            center := v2f.{ xx (entry_x0 + entry_x1) / 2, xx (entry_y0 + entry_y1) / 2 };
            size   := v2f.{ xx (entry_x1 - entry_x0),     xx (entry_y1 - entry_y0) };

            bg_color: GFX_Color = .{ entry.r, entry.g, entry.b, 255 };
            fg_color: GFX_Color = .{ 255, 255, 255, 255 };

            string := sprint(*viewer.frame_allocator, "%, %*%", entry.name, duration_time, time_resolution_suffix(duration_time_unit));

            string_size := gfx_calculate_ui_text_size(gfx, string);
            if text.x < element.screen_position.x text.x = element.screen_position.x;

            if text.x + string_size.x + 5 < center.x + size.x / 2    text.x += 5;

            gfx_draw_quad(gfx, center, size, bg_color);
            gfx_draw_outlined_quad(gfx, center, size, 2, .{ bg_color.r + 20, bg_color.g + 20, bg_color.b + 20, 255 });
            gfx_flush_quads(gfx); // So that the text gets rendered on top of this...
            
            combine_scissors(gfx, .{ element.screen_position.x, element.screen_position.y }, .{ element.screen_position.x + element.screen_size.x, element.screen_position.y + element.screen_size.y }, .{ center.x - size.x / 2, center.y - size.y / 2 }, .{ center.x + size.x / 2, center.y + size.y / 2 });
            gfx_draw_text_with_background(gfx, *gfx.ui_font, string, text, .Left, fg_color, bg_color);
            gfx_flush_text(gfx); // Due to scissoring
            gfx_clear_scissors(gfx);
        } else {
            entry_center := round((entry_x0 + entry_x1) / 2);
            entry_x0 = clamp(entry_center - 1, low_x_clamp, high_x_clamp);
            entry_x1 = clamp(entry_center + 1, low_x_clamp, high_x_clamp);

            center := v2f.{ xx (entry_x0 + entry_x1) / 2, xx (entry_y0 + entry_y1) / 2 };
            size   := v2f.{ xx (entry_x1 - entry_x0),     xx (entry_y1 - entry_y0) };

            bg_color: GFX_Color = .{ 84, 95, 135, 255 };
            gfx_draw_quad(gfx, center, size, bg_color);
        }
    }

    //
    // Add more vertical markers at specific time intervals.
    //
    {        
        draw_profiling_marker(gfx, element, x0, element.screen_size.y);
        draw_profiling_marker(gfx, element, x0 + w, element.screen_size.y);

        interval_time, interval_time_unit := get_best_time_resolution(xx viewer.profiling_data.total_time_in_nanoseconds / xx data.current_zoom);
        interval_width := width_per_time_unit[interval_time_unit];

        if interval_time > 100 interval_width *= 10;
        
        screen_x0_offset := xx element.screen_position.x - x0;
        for x := -fmod(screen_x0_offset, interval_width); x <= xx element.screen_size.x; x += interval_width {
            draw_profiling_marker(gfx, element, x + xx element.screen_position.x, 10);
        }
    }

    //
    // Display the total time interval shown in the current zoom.
    //
    {       
        fg_color := GFX_Color.{ 255, 255, 255, 255 };
        
        displayed_time, displayed_time_unit := get_best_time_resolution(xx viewer.profiling_data.total_time_in_nanoseconds / xx data.current_zoom);
        
        interval_string := sprint(*viewer.frame_allocator, "Shown Interval: %*% (%%%)", displayed_time, time_resolution_suffix(displayed_time_unit), cast(s64) roundf(100 / data.current_zoom));
        gfx_draw_text_without_background(gfx, *gfx.ui_font, interval_string, .{ element.screen_position.x + 5, element.screen_position.y + xx gfx.ui_font.line_height }, .Left, fg_color); // The UI element probably does not have an opaque background...

        gfx_flush_text(gfx); // Due to the scissoring
    }

    gfx_flush_quads(gfx); // Finally flush the quads, since they work correctly without scissors.
}

update_profiling_timeline :: (input: UI_Input, element: *UI_Element, data: *Profiling_UI_Data) {   
    interacted_with_widget := (element.interactions & .Hovered) || (element.interactions & .Dragged);
    previous_zoom := data.current_zoom;
    
    if interacted_with_widget && data.viewer.window.button_held[.Right] {
        data.target_center -= xx data.viewer.window.mouse_delta_x;
    }
    
    if interacted_with_widget && input.mouse_wheel_turns != 0 {
        pw, ph, px0, py0 := get_profiling_view_port(element, data, false);

        data.zoom_index  = clamp(data.zoom_index + input.mouse_wheel_turns, 0, 50);
        data.target_zoom = powf(PROFILING_ZOOM_STEP, data.zoom_index);

        nw, nh, nx0, ny0   := get_profiling_view_port(element, data, false);
        cursor_position    := clamp((xx data.viewer.window.mouse_x - px0), 0, pw);
        cursor_distance    := cursor_position / pw;
        data.target_center += xx (cursor_distance * (nw - pw));
    }

    data.current_zoom   += (data.target_zoom   - data.current_zoom)   * data.viewer.window.frame_time * 10;
    data.current_center += (data.target_center - data.current_center) * data.viewer.window.frame_time * 10;

    if interacted_with_widget && data.viewer.window.button_pressed[.Left] reset_profiling_data(data, element.screen_size.x);
}

reset_profiling_data :: (data: *Profiling_UI_Data, element_width: f32) {
    data.zoom_index     = 0;
    data.target_zoom    = powf(PROFILING_ZOOM_STEP, data.zoom_index);
    data.current_zoom   = data.target_zoom;
    data.target_center  = element_width / 2;
    data.current_center = data.target_center;
}

profiling_mode_string :: (show_summary: bool) -> string {
    if show_summary return "Summary";
    return "Timeline";
}

profiling_panel :: (viewer: *Viewer) {
    if viewer.profiling_panel_state == .Closed return;

    name := sprint(*viewer.frame_allocator, "Profiling : %", viewer.test_name);
    viewer.profiling_panel_position, viewer.profiling_panel_state = ui_push_window(*viewer.ui, name, viewer.profiling_panel_position, .Draggable | .Collapsable | .Closeable);
    if viewer.profiling_panel_state == .Open {
        width : f32 : 1000;
        height: f32 : 500;

        reset_timeline := false;
        
        //
        // View settings for the profiling panel.
        //
        {
            ui_set_width(*viewer.ui, .Pixels, width, 1);
            ui_push_fixed_container(*viewer.ui, .Horizontal);
            ui_set_width(*viewer.ui, .Label_Size, 10, 1);
            ui_label(*viewer.ui, false, "Mode");

            ui_push_width(*viewer.ui, .Pixels, 128, 1);
            if ui_push_dropdown(*viewer.ui, profiling_mode_string(viewer.profiling_show_summary)) {
                if ui_button(*viewer.ui, profiling_mode_string(false)) viewer.profiling_show_summary = false;
                if ui_button(*viewer.ui, profiling_mode_string(true)) viewer.profiling_show_summary = true;
            }
            ui_pop_dropdown(*viewer.ui);
            ui_pop_width(*viewer.ui);

            ui_set_width(*viewer.ui, .Percentage_Of_Parent, 1, 0);
            ui_spacer(*viewer.ui);

            if ui_button(*viewer.ui, "Reset") {
                reset_timeline = true;
            }
            
            ui_pop_container(*viewer.ui);
        }

        ui_push_width(*viewer.ui, .Pixels, width, 1);
        ui_divider(*viewer.ui, true);
        ui_pop_width(*viewer.ui);
        
        //
        // Do the actual informational widget.
        //
        {
           
            if viewer.profiling_show_summary {
                ui_set_width(*viewer.ui, .Pixels, width, 1);
                ui_push_fixed_container(*viewer.ui, .Horizontal);
                ui_set_width(*viewer.ui, .Pixels, 5, 1);
                ui_spacer(*viewer.ui);
                ui_set_width(*viewer.ui, .Percentage_Of_Parent, .5, .8);
                ui_label(*viewer.ui, false, "Name");
                ui_set_width(*viewer.ui, .Percentage_Of_Parent, .2, .8);
                ui_label(*viewer.ui, false, "Inclusive");
                ui_set_width(*viewer.ui, .Percentage_Of_Parent, .2, .8);
                ui_label(*viewer.ui, false, "Exclusive");
                ui_set_width(*viewer.ui, .Percentage_Of_Parent, .1, .8);
                ui_label(*viewer.ui, false, "Count");
                ui_pop_container(*viewer.ui);

                ui_set_width(*viewer.ui, .Pixels, width, 1);
                ui_set_height(*viewer.ui, .Pixels, height, 1);
                ui_push_scroll_view(*viewer.ui, "__entry_container", .Vertical);
                for i := 0; i < viewer.profiling_data.summary_count; ++i {
                    entry := *viewer.profiling_data.summary[i];
                    ui_set_width(*viewer.ui, .Pixels, width, 1);
                    ui_set_height(*viewer.ui, .Pixels, 32, 1);
                    ui_push_fixed_container(*viewer.ui, .Horizontal);

                    inclusive_time, inclusive_unit := get_best_time_resolution(entry.inclusive_time_in_nanoseconds);
                    exclusive_time, exclusive_unit := get_best_time_resolution(entry.exclusive_time_in_nanoseconds);
                    
                    ui_set_width(*viewer.ui, .Pixels, 5, 1);
                    ui_spacer(*viewer.ui);
                    ui_set_width(*viewer.ui, .Percentage_Of_Parent, .5, .8);
                    ui_label(*viewer.ui, false, "%", entry.name);
                    ui_set_width(*viewer.ui, .Percentage_Of_Parent, .2, .8);
                    ui_label(*viewer.ui, false, "%*%", inclusive_time, time_resolution_suffix(inclusive_unit));
                    ui_set_width(*viewer.ui, .Percentage_Of_Parent, .2, .8);
                    ui_label(*viewer.ui, false, "%*%", exclusive_time, time_resolution_suffix(exclusive_unit));
                    ui_set_width(*viewer.ui, .Percentage_Of_Parent, .1, .8);
                    ui_label(*viewer.ui, false, "%", entry.count);

                    ui_pop_container(*viewer.ui);
                }
                ui_pop_scroll_view(*viewer.ui);
            } else {
                ui_set_width(*viewer.ui, .Pixels, width, 1);
                ui_set_height(*viewer.ui, .Pixels, height, 1);

                data: *Profiling_UI_Data;
                created_this_frame: bool;
                data, created_this_frame = ui_custom_widget(*viewer.ui, "__profiling_view", update_profiling_timeline, draw_profiling_timeline, size_of(Profiling_UI_Data));

                if created_this_frame || reset_timeline {
                    data.viewer = viewer;
                    reset_profiling_data(data, width);
                }
            }
        }
    }
    
    ui_pop_window(*viewer.ui);
}
