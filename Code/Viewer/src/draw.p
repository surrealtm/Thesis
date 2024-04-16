LINE_BATCH_COUNT :: 512;
SAMPLES :: 8;

Camera :: struct {
    fov:    f32 = 70;
    aspect: f32;
    near:   f32 = 0.01;
    far:    f32 = 100;

    position: v3f;
    rotation: v3f;

    projection: m4f;
    view: m4f;
}

Renderer :: struct {
    window: *Window;
    gfx: *GFX;

    camera: Camera;
    light_direction: v3f;

    hud_font: Font;
    
    lit_shader: Shader;
    wireframe_shader: Shader;

    frame_buffer: Frame_Buffer; // Used for multisampling the 3D rendering.
    
    cube_buffer: Vertex_Buffer;

    line_buffer: Vertex_Buffer;
    line_vertices: []f32; // Position, Endpoint, Extension
    line_colors: []f32;
    line_count: s64;
}

create_renderer :: (renderer: *Renderer, window: *Window, gfx: *GFX, allocator: *Allocator) {
    renderer.window = window;
    renderer.gfx    = gfx;
    renderer.light_direction = v3f_normalize(.{ 3, 2, 1 });
    create_font_from_file(*renderer.hud_font, "C:/Windows/Fonts/segoeui.ttf", 20, false, create_gl_texture_2d, null);
    
    //
    // Set up shaders.
    //
    {
        create_shader_from_string(*renderer.lit_shader, lit_shader_code, "lit_shader");
        create_shader_from_string(*renderer.wireframe_shader, wireframe_shader_code, "wireframe_shader");
        dump_gl_errors("Shader");
    }
        
    //
    // Set up the cube buffer.
    //
    {
        cube_vertices: []f32 = {
            -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, -1.0, 1.0, 1.0, // Left Side  
            -1.0, -1.0, -1.0, -1.0, 1.0, 1.0, -1.0, 1.0, -1.0, // Left Side
            1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 1.0, -1.0, // Back Side
            1.0, 1.0, -1.0, 1.0, -1.0, -1.0, -1.0, -1.0, -1.0, // Back Side
            1.0, -1.0, 1.0, -1.0, -1.0, -1.0, 1.0, -1.0, -1.0, // Bottom Side
            1.0, -1.0, 1.0, -1.0, -1.0, 1.0, -1.0, -1.0, -1.0, // Bottom Side
            -1.0, 1.0, 1.0, -1.0, -1.0, 1.0, 1.0, -1.0, 1.0,   // Front Side
            1.0, 1.0, 1.0, -1.0, 1.0, 1.0, 1.0, -1.0, 1.0,     // Front Side
            1.0, 1.0, 1.0, 1.0, -1.0, -1.0, 1.0, 1.0, -1.0,    // Right Side
            1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, -1.0, 1.0,    // Right Side
            1.0, 1.0, 1.0, 1.0, 1.0, -1.0, -1.0, 1.0, -1.0,    // Top Side
            1.0, 1.0, 1.0, -1.0, 1.0, -1.0, -1.0, 1.0, 1.0,    // Top Side
        };

        cube_normals: []f32 = {
            -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, // Left Side
            -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, // Left Side
            0.0, 0.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, // Back Side
            0.0, 0.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, // Back Side
            0.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, 0.0, // Bottom Side
            0.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, 0.0, // Bottom Side
            0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0,    // Front Side
            0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0,    // Front Side
            1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0,    // Right Side
            1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0,    // Right Side
            0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0,    // Top Side
            0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0,    // Top Side
        };

        create_vertex_buffer(*renderer.cube_buffer);
        add_vertex_data(*renderer.cube_buffer, cube_vertices, cube_vertices.count, 3);
        add_vertex_data(*renderer.cube_buffer, cube_normals, cube_normals.count, 3);
        set_vertex_buffer_name(*renderer.cube_buffer, "cube_buffer");
        dump_gl_errors("Cube Vertex");
    }

    //
    // Set up the line buffer.
    //
    {        
        create_vertex_buffer(*renderer.line_buffer);
        add_vertex_attribute_buffer(*renderer.line_buffer, null, LINE_BATCH_COUNT * 6 * 7, 7);
        allocate_vertex_attribute(*renderer.line_buffer, 3, 7, 0); // Position
        allocate_vertex_attribute(*renderer.line_buffer, 3, 7, 3); // Endpoint
        allocate_vertex_attribute(*renderer.line_buffer, 1, 7, 6); // Extension
        allocate_vertex_data(*renderer.line_buffer, LINE_BATCH_COUNT * 6 * 4, 4);
        set_vertex_buffer_name(*renderer.line_buffer, "line_buffer");

        renderer.line_vertices = allocate_array(allocator, LINE_BATCH_COUNT * 6 * 7, f32);
        renderer.line_colors = allocate_array(allocator, LINE_BATCH_COUNT * 6 * 4, f32);
        dump_gl_errors("Line Vertex");
    }

    //
    // Set up the framebuffer.
    //
    {
        create_frame_buffers(renderer, false);
        dump_gl_errors("Framebuffer");
    }
}

destroy_renderer :: (renderer: *Renderer, allocator: *Allocator) {
    deallocate_array(allocator, *renderer.line_vertices);
    deallocate_array(allocator, *renderer.line_colors);
    destroy_vertex_buffer(*renderer.cube_buffer);
    destroy_vertex_buffer(*renderer.line_buffer);
    destroy_shader(*renderer.lit_shader);
    destroy_shader(*renderer.wireframe_shader);
    destroy_frame_buffer(*renderer.frame_buffer);
    destroy_font(*renderer.hud_font, destroy_gl_texture_2d, null);
}

create_frame_buffers :: (renderer: *Renderer, destroy: bool) {
    if destroy {
        destroy_frame_buffer(*renderer.frame_buffer);
    }

    create_frame_buffer(*renderer.frame_buffer);
    create_color_texture_attachment(*renderer.frame_buffer, renderer.window.width, renderer.window.height, .RGB, SAMPLES);
    create_depth_buffer_attachment(*renderer.frame_buffer, renderer.window.width, renderer.window.height, SAMPLES);
    validate_frame_buffer(*renderer.frame_buffer);
}



/* -------------------------------------------------- Camera -------------------------------------------------- */

update_camera :: (renderer: *Renderer) {
    //
    // Move the camera.
    //
    {
        yaw := turns_to_radians(renderer.camera.rotation.y);
        speed := 100 * renderer.window.frame_time;

        if renderer.window.key_held[.Shift] speed /= 2;
        
        if renderer.window.key_held[.W] {
            renderer.camera.position.x += sinf(yaw) * speed;
            renderer.camera.position.z -= cosf(yaw) * speed;
        }

        if renderer.window.key_held[.S] {
            renderer.camera.position.x -= sinf(yaw) * speed;
            renderer.camera.position.z += cosf(yaw) * speed;
        }

        if renderer.window.key_held[.A] {
            renderer.camera.position.x -= cosf(yaw) * speed;
            renderer.camera.position.z -= sinf(yaw) * speed;
        }

        if renderer.window.key_held[.D] {
            renderer.camera.position.x += cosf(yaw) * speed;
            renderer.camera.position.z += sinf(yaw) * speed;
        }

        if renderer.window.key_held[.X] renderer.camera.position.y += speed;

        if renderer.window.key_held[.Y] renderer.camera.position.y -= speed;
        
        if renderer.window.button_held[.Middle] {
            renderer.camera.rotation.x += xx renderer.window.raw_mouse_delta_y * 0.0002;
            renderer.camera.rotation.y += xx renderer.window.raw_mouse_delta_x * 0.0002;
        }
    }

    renderer.camera.aspect     = xx renderer.window.width / xx renderer.window.height;
    renderer.camera.projection = make_perspective_projection_matrix(renderer.camera.fov, renderer.camera.aspect, renderer.camera.near, renderer.camera.far);
    renderer.camera.view       = make_view_matrix(renderer.camera.position, renderer.camera.rotation);
}



/* ---------------------------------------------- Immediate API ---------------------------------------------- */

prepare_3d :: (renderer: *Renderer, color: GFX_Color) {
    if renderer.window.resized {
        create_frame_buffers(renderer, true);
    }

    set_frame_buffer(*renderer.frame_buffer);
    glClearColor(xx color.r / 255., xx color.g / 255., xx color.b / 255., xx color.a / 255.);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

finish_3d :: (renderer: *Renderer) {
    set_depth_test(.Disabled);
    set_cull_test(.Disabled);
    set_blending(.Default);
    
    blit_frame_buffer_to_default(*renderer.frame_buffer, renderer.window.width, renderer.window.height);
    set_default_frame_buffer(renderer.window.width, renderer.window.height);
}

flush_lines :: (renderer: *Renderer) {
    if renderer.line_count == 0 return;

    set_cull_test(.Disabled);
    set_depth_test(.Less_Equals);
    
    set_shader(*renderer.wireframe_shader);
    set_shader_uniform_m4f(*renderer.wireframe_shader, "u_projection", renderer.camera.projection._m[0].data);
    set_shader_uniform_m4f(*renderer.wireframe_shader, "u_view", renderer.camera.view._m[0].data);
    set_shader_uniform_b(*renderer.wireframe_shader, "u_use_constant_screen_size", false);
    set_shader_uniform_f(*renderer.wireframe_shader, "u_aspect", renderer.camera.aspect);
    
    set_vertex_buffer(*renderer.line_buffer);
    update_vertex_data(*renderer.line_buffer, 0, renderer.line_vertices, renderer.line_count * 6 * 7);
    update_vertex_data(*renderer.line_buffer, 1, renderer.line_colors, renderer.line_count * 6 * 4);
    draw_vertex_buffer(*renderer.line_buffer);

    renderer.line_count = 0;
}

add_line_endpoint :: (renderer: *Renderer, i: s64, p: v3f, e: v3f, ex: f32, color: [4]f32) {
    {
        idx := renderer.line_count * 6 * 7 + i * 7;
        renderer.line_vertices[idx + 0] = p.x;
        renderer.line_vertices[idx + 1] = p.y;
        renderer.line_vertices[idx + 2] = p.z;
        renderer.line_vertices[idx + 3] = e.x;
        renderer.line_vertices[idx + 4] = e.y;
        renderer.line_vertices[idx + 5] = e.z;
        renderer.line_vertices[idx + 6] = ex;
    }

    {
        idx := renderer.line_count * 6 * 4 + i * 4;
        renderer.line_colors[idx + 0] = color[0];
        renderer.line_colors[idx + 1] = color[1];
        renderer.line_colors[idx + 2] = color[2];
        renderer.line_colors[idx + 3] = color[3];
    }
}

draw_line :: (renderer: *Renderer, p0: v3f, p1: v3f, thickness: f32, color: GFX_Color) {
    if renderer.line_count == LINE_BATCH_COUNT flush_lines(renderer);
   
    // The lines are extruded in the vertex shader to ensure proper depth values and
    // screen scaling of the triangles.
    // -1      p0 ------------  p1    1
    //         |       /        |
    //  1      p0 ------------  p1   -1
    // 
    // Since the direction vector always points towards the other end, the normal gets flipped across the two
    // points, meaning we need different directions for the two points.

    color_array: [4]f32 = { xx color.r / 255., xx color.g / 255., xx color.b / 255., xx color.a / 255. };

    add_line_endpoint(renderer, 0, p0, p1, -thickness, color_array);
    add_line_endpoint(renderer, 1, p1, p0, +thickness, color_array);
    add_line_endpoint(renderer, 2, p0, p1, +thickness, color_array);

    add_line_endpoint(renderer, 3, p0, p1, +thickness, color_array);
    add_line_endpoint(renderer, 4, p1, p0, +thickness, color_array);
    add_line_endpoint(renderer, 5, p1, p0, -thickness, color_array);

    ++renderer.line_count;
}

draw_cuboid :: (renderer: *Renderer, center: v3f, size: v3f, color: GFX_Color) {
    transformation := make_transformation_matrix_with_v3f_rotation(center, .{ 0, 0, 0 }, size);

    set_cull_test(.Backfaces);
    set_depth_test(.Default);
    
    set_shader(*renderer.lit_shader);
    set_shader_uniform_m4f(*renderer.lit_shader, "u_projection", renderer.camera.projection._m[0].data);
    set_shader_uniform_m4f(*renderer.lit_shader, "u_view", renderer.camera.view._m[0].data);
    set_shader_uniform_m4f(*renderer.lit_shader, "u_transformation", transformation._m[0].data);
    set_shader_uniform_v3f(*renderer.lit_shader, "u_light_direction", renderer.light_direction.x, renderer.light_direction.y, renderer.light_direction.z);
    set_shader_uniform_color(*renderer.lit_shader, "u_color", color.r, color.g, color.b, color.a);

    set_vertex_buffer(*renderer.cube_buffer);
    draw_vertex_buffer(*renderer.cube_buffer);
}

draw_3d_hud_text :: (renderer: *Renderer, center: v3f, text: string, color: GFX_Color) {
    screen_position, on_screen := world_space_to_screen_space(renderer.camera.projection, renderer.camera.view, center, .{ xx renderer.window.width, xx renderer.window.height });

    if !on_screen return;

    width := query_text_width(*renderer.hud_font, text);

    bg_color :: GFX_Color.{ 50, 50, 50, 200 };
    
    gfx_draw_quad(renderer.gfx, .{ screen_position.x, screen_position.y + xx renderer.hud_font.descender }, .{ xx width, xx renderer.hud_font.line_height }, bg_color);
    gfx_flush_quads(renderer.gfx);

    gfx_draw_text_without_background(renderer.gfx, *renderer.hud_font, text, screen_position, .Centered, color);
    gfx_flush_text(renderer.gfx);
}



/* ----------------------------------------------- Shader Code ----------------------------------------------- */

lit_shader_code :: "
@Vertex(330 core)
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

out vec3 pass_normal;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_transformation;

void main(void) {
    gl_Position = u_projection * u_view * u_transformation * vec4(in_position, 1.0);
    pass_normal = in_normal;
}

@Fragment(330 core)
in vec3 pass_normal;

out vec4 out_color;

uniform vec4 u_color;
uniform vec3 u_light_direction;

void main(void) {
    float diffuse = max(dot(pass_normal, u_light_direction), 0.2);
    out_color = vec4(u_color.rgb * diffuse, u_color.a);
}
";

wireframe_shader_code :: "
@Vertex(330 core)

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_endpoint;
layout(location = 2) in float in_extension;
layout(location = 3) in vec4 in_color;

out vec4 pass_color;

uniform mat4  u_projection;
uniform mat4  u_view;
uniform vec3  u_normal;
uniform float u_aspect;
uniform bool  u_use_constant_screen_size;
uniform bool  u_use_normal;

vec4 to_clip_space(vec3 world_space) {
    vec4 clip = u_projection * u_view * vec4(world_space, 1.0);
    return clip;
}

vec2 to_screen_space(vec4 clip_space) {
    return clip_space.xy / clip_space.w * vec2(u_aspect, 1.0);
}

void main(void) {
    // Calculate the screen space position of both endpoints, derive the normal from that to always
    // have the line triangles face the camera
    vec4 this_clip_space = to_clip_space(in_position);
    vec2 this_screen_space = to_screen_space(this_clip_space);

    vec4 endpoint_clip_space = to_clip_space(in_endpoint);
    vec2 endpoint_screen_space = to_screen_space(endpoint_clip_space);

    vec2 line_direction = normalize(endpoint_screen_space - this_screen_space);
    vec2 line_normal    = vec2(-line_direction.y, line_direction.x) * in_extension / 2.0;

    if(u_use_constant_screen_size) {
        line_normal *= this_clip_space.w; // Maybe there is a smarter way of doing this without conditionals? Like multiplying the normal by just 1 if constant_screen_size is not set?
        line_normal.x /= u_aspect; // Adjust the normal to the aspect ratio of the screen
    }

    vec4 clip_offset = vec4(line_normal, 0.0, 0.0);
    gl_Position = this_clip_space + clip_offset;

    pass_color = in_color;
}

@Fragment(140)
in vec4 pass_color;

out vec4 out_color;

void main(void) {
    out_color = pass_color;
}
";