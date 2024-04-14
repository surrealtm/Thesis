draw_profiling :: (viewer: *Viewer) {
    total_width: f32 = xx viewer.window.width;
    left_x:      f32 = 0;
    upper_y:     f32 = 200;
    bar_height:  f32 = 32;

    for i := 0; i < viewer.profiling_data.timeline_count; ++i {
        entry  := *viewer.profiling_data.timeline[i];
        
        text   := v2f.{ roundf((xx entry.relative_start * total_width) + left_x), roundf(upper_y + xx (entry.depth + 1) * bar_height + xx viewer.font.descender) };
        center := v2f.{ roundf((xx entry.relative_end + xx entry.relative_start) / 2 * total_width + left_x), roundf(upper_y + (xx entry.depth + 0.5) * bar_height) };
        size   := v2f.{ roundf((xx entry.relative_end - xx entry.relative_start) * total_width), bar_height - 1 };

        bg_color: GFX_Color = .{ 255, 0, entry.depth * 16, 255 };
        fg_color: GFX_Color = .{ 255, 255, 255, 255 };
        
        gfx_draw_quad(*viewer.gfx, center, size, bg_color);
        gfx_set_scissors(*viewer.gfx, .{ center.x - size.x / 2, center.y - size.y / 2 }, .{ center.x + size.x / 2, center.y + size.y / 2 });
        gfx_draw_text_with_background(*viewer.gfx, *viewer.font, cstring_view(entry.name), text, .Left, fg_color, bg_color);
        gfx_flush_text(*viewer.gfx); // Due to scissoring
        gfx_clear_scissors(*viewer.gfx);
    }
}
