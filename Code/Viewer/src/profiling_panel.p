PROFILING_BAR_HEIGHT: f32 : 32;

Profiling_UI_Data :: struct {
    viewer: *Viewer;
    zoom:   v2f;
    center: v2f;
}

get_best_time_resolution :: (time_in_seconds: f32) -> f32, Time_Resolution {
    if time_in_seconds <= 0 return 0, .Nanoseconds;

    adjusted_time := time_in_seconds;
    
    resolution := Time_Resolution.Seconds;
    while adjusted_time < 1 {
        adjusted_time *= 1000;
        ++resolution;
    }

    return adjusted_time, resolution;
}

combine_scissors :: (gfx: *GFX, tl1: v2f, br1: v2f, tl2: v2f, br2: v2f) {
    tl := v2f.{ max(tl1.x, tl2.x), max(tl1.y, tl2.y) };
    br := v2f.{ min(br1.x, br2.x), min(br1.y, br2.y) };

    gfx_set_scissors(gfx, tl, br);
}

get_profiling_view_port :: (element: *UI_Element, data: *Profiling_UI_Data) -> f32, f32, f32, f32 {
    w:  f32 = element.screen_size.x * data.zoom.x;
    h:  f32 = element.screen_size.y * data.zoom.y;
    x0: f32 = element.screen_position.x + element.screen_size.x / 2 - w * data.center.x;
    y0: f32 = element.screen_position.y + element.screen_size.y / 2 - h * data.center.y;
    
    return w, h, x0, y0;
}

draw_profiling :: (*void, element: *UI_Element, data: *Profiling_UI_Data) {
    w, h, x0, y0 := get_profiling_view_port(element, data);

    viewer := data.viewer;
    gfx := *data.viewer.gfx;
    
    //
    // Render the actual timeline.
    //
    for i := 0; i < viewer.profiling_data.timeline_count; ++i {
        entry  := *viewer.profiling_data.timeline[i];
        
        text   := v2f.{ roundf((xx entry.relative_start * w) + x0), roundf(y0 + xx (entry.depth + 2) * PROFILING_BAR_HEIGHT + xx gfx.ui_font.descender) };
        center := v2f.{ roundf((xx entry.relative_end + xx entry.relative_start) / 2 * w + x0), roundf(y0 + (xx entry.depth + 1.5) * PROFILING_BAR_HEIGHT) };
        size   := v2f.{ roundf((xx entry.relative_end - xx entry.relative_start) * w), PROFILING_BAR_HEIGHT - 1 };

        if size.x == 0 || (center.x + size.x / 2 < element.screen_position.x) || (center.x - size.x / 2 > element.screen_position.x + element.screen_size.x) continue;
        
        bg_color: GFX_Color = .{ entry.r, entry.g, entry.b, 255 };
        fg_color: GFX_Color = .{ 255, 255, 255, 255 };

        combine_scissors(gfx, .{ element.screen_position.x, element.screen_position.y }, .{ element.screen_position.x + element.screen_size.x, element.screen_position.y + element.screen_size.y }, .{ center.x - size.x / 2, center.y - size.y / 2 }, .{ center.x + size.x / 2, center.y + size.y / 2 });

        displayed_time, displayed_time_unit := get_best_time_resolution(xx ((entry.relative_end - entry.relative_start ) * viewer.profiling_data.total_time_in_seconds));
        string := sprint(*viewer.frame_allocator, "%, %*%", entry.name, displayed_time, time_resolution_suffix(displayed_time_unit));

        string_size := gfx_calculate_ui_text_size(gfx, string);
        if string_size.x + 5 < size.x text.x += 5;

        gfx_draw_quad(gfx, center, size, bg_color);
        gfx_draw_outlined_quad(gfx, center, size, 2, .{ bg_color.r + 20, bg_color.g + 20, bg_color.b + 20, 255 });

        gfx_draw_text_with_background(gfx, *gfx.ui_font, string, text, .Left, fg_color, bg_color);
        gfx_flush_text(gfx); // Due to scissoring
    }

    gfx_set_scissors(gfx, .{ element.screen_position.x, element.screen_position.y }, .{ element.screen_position.x + element.screen_size.x, element.screen_position.y + element.screen_size.y });
    
    //
    // Mark the start and end of profiling visually.
    //
    {
        marker_color :: GFX_Color.{ 150, 150, 150, 255 };
        gfx_draw_line(gfx, .{ x0 + .5, element.screen_position.y }, .{ x0 + .5, element.screen_position.y + element.screen_size.y }, 1, marker_color); // Mark the start of profiling
        gfx_draw_line(gfx, .{ x0 + w, element.screen_position.y }, .{ x0 +  w, element.screen_position.y + element.screen_size.y }, 1, marker_color); // Mark the end of profiling
    }

    //
    // Add more vertical markers at specific time intervals.
    //
    {       
        fg_color := GFX_Color.{ 255, 255, 255, 255 };
        
        displayed_time, displayed_time_unit := get_best_time_resolution(xx viewer.profiling_data.total_time_in_seconds / data.zoom.x);
        
        interval_string := sprint(*viewer.frame_allocator, "Shown Interval: %*% (%%%)", displayed_time, time_resolution_suffix(displayed_time_unit), (100 / data.zoom.x));
        gfx_draw_text_without_background(gfx, *gfx.ui_font, interval_string, .{ element.screen_position.x + 5, element.screen_position.y + xx gfx.ui_font.line_height }, .Left, fg_color); // The UI element probably does not have an opaque background...

        gfx_flush_text(gfx); // Due to the scissoring
    }
}

update_profiling :: (input: UI_Input, element: *UI_Element, data: *Profiling_UI_Data) {
    if !(element.interactions & .Hovered) && !(element.interactions & .Dragged) return;
    
    w, h, x0, y0 := get_profiling_view_port(element, data);
        
    if data.viewer.window.button_held[.Right] {
        data.center.x -= xx data.viewer.window.mouse_delta_x / w;
    }

    previous_zoom := data.zoom.x;
    
    if input.mouse_wheel_turns != 0 {
        // @Cleanup: Improve this.
        data.zoom.x = clamp(data.zoom.x + input.mouse_wheel_turns, 1, 1000);        
    }

    if abs(previous_zoom - data.zoom.x) > F32_EPSILON {
        pixel_cursor    := input.mouse_position.x - x0;
        relative_offset := (pixel_cursor / w - data.center.x) / data.zoom.x;
        data.center.x   += relative_offset * sign(input.mouse_wheel_turns);
    }

    if data.viewer.window.button_pressed[.Left] {
        data.zoom   = .{  1,  1 };
        data.center = .{ .5, .5 };
    }
}

profiling_mode_string :: (show_summary: bool) -> string {
    if show_summary return "Summary";
    return "Timeline";
}

profiling_ui :: (viewer: *Viewer) {
    if viewer.profiling_panel_state == .Closed return;

    name := sprint(*viewer.frame_allocator, "Profiling : %", viewer.profiling_string);
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

                    inclusive_time, inclusive_unit := get_best_time_resolution(xx entry.inclusive_time_in_seconds);
                    exclusive_time, exclusive_unit := get_best_time_resolution(xx entry.exclusive_time_in_seconds);
                    
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
                data, created_this_frame = ui_custom_widget(*viewer.ui, "__profiling_view", update_profiling, draw_profiling, size_of(Profiling_UI_Data));

                if created_this_frame || reset_timeline {
                    data.viewer = viewer;
                    data.zoom   = .{  1,  1 };
                    data.center = .{ .5, .5 };
                }
            }
        }
    }
    
    ui_pop_window(*viewer.ui);
}
