LINE_BATCH_COUNT     :: 512;
TRIANGLE_BATCH_COUNT :: 512;
SAMPLES :: 8;

Camera :: struct {
    fov:    f32 = 70;
    aspect: f32;
    near:   f32 = 0.01;
    far:    f32 = 500;

    position: v3f;
    rotation: v3f;
    forward:  v3f;
    
    projection: m4f;
    view: m4f;
}

Internal_Triangle :: struct {
    dot_to_camera: f32;
    p0, p1, p2: v3f;
    r, g, b, a: f32;
}

Renderer :: struct {
    window: *Window;
    gfx: *GFX;
    frame_allocator: *Allocator;
    
    camera: Camera;
    light_direction: v3f;

    hud_font: Font;
    
    lit_shader:       Shader;
    wireframe_shader: Shader;
    triangle_shader:  Shader;
    
    frame_buffer: Frame_Buffer; // Used for multisampling the 3D rendering.
    
    cube_buffer: Vertex_Buffer;
    sphere_buffer: Vertex_Buffer;

    triangle_buffer: Vertex_Buffer;
    triangle_vertices: []f32; // Position, Color
    triangle_count: s64;
    
    line_buffer: Vertex_Buffer;
    line_vertices: []f32; // Position, Endpoint, Extension
    line_colors: []f32;
    line_count: s64;


    sorted_triangles: []Internal_Triangle;
}

create_renderer :: (renderer: *Renderer, window: *Window, gfx: *GFX, global_allocator: *Allocator, frame_allocator: *Allocator) {
    renderer.window = window;
    renderer.gfx    = gfx;
    renderer.light_direction = v3f_normalize(.{ 1, 2, 3 });
    renderer.frame_allocator = frame_allocator;
    create_font_from_file(*renderer.hud_font, "C:/Windows/Fonts/segoeui.ttf", 20, false, create_gl_texture_2d, null);
    
    //
    // Set up shaders.
    //
    {
        create_shader_from_string(*renderer.lit_shader, lit_shader_code, "lit_shader");
        create_shader_from_string(*renderer.wireframe_shader, wireframe_shader_code, "wireframe_shader");
        create_shader_from_string(*renderer.triangle_shader, triangle_shader_code, "triangle_shader");
        dump_gl_errors("Shaders");
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
        add_vertex_data(*renderer.cube_buffer, cube_normals,  cube_normals.count,  3);
        set_vertex_buffer_name(*renderer.cube_buffer, "cube_buffer");
        dump_gl_errors("Cube Buffer");
    }

    //
    // Set up the sphere buffer.
    //
    {
        vertices, normals := get_sphere_vertices(0.5, 32);

        create_vertex_buffer(*renderer.sphere_buffer);
        add_vertex_data(*renderer.sphere_buffer, vertices, vertices.count, 3);
        add_vertex_data(*renderer.sphere_buffer, normals,  normals.count,  3);
        set_vertex_buffer_name(*renderer.sphere_buffer, "sphere_buffer");
        dump_gl_errors("Sphere Buffer");
        
        deallocate_array(Default_Allocator, *normals);
        deallocate_array(Default_Allocator, *vertices);
    }

    //
    // Set up the triangle buffer.
    //
    {
        create_vertex_buffer(*renderer.triangle_buffer);
        add_vertex_attribute_buffer(*renderer.triangle_buffer, null, TRIANGLE_BATCH_COUNT * 3 * 7, 7);
        allocate_vertex_attribute(*renderer.triangle_buffer, 3, 7, 0); // Position
        allocate_vertex_attribute(*renderer.triangle_buffer, 4, 7, 3); // Color
        set_vertex_buffer_name(*renderer.triangle_buffer, "triangle_buffer");
        dump_gl_errors("Triangle Buffer");

        renderer.triangle_vertices = allocate_array(global_allocator, TRIANGLE_BATCH_COUNT * 3 * 7, f32);
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

        renderer.line_vertices = allocate_array(global_allocator, LINE_BATCH_COUNT * 6 * 7, f32);
        renderer.line_colors = allocate_array(global_allocator, LINE_BATCH_COUNT * 6 * 4, f32);
        dump_gl_errors("Line Buffer");
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
    deallocate_array(allocator, *renderer.triangle_vertices);
    destroy_vertex_buffer(*renderer.cube_buffer);
    destroy_vertex_buffer(*renderer.sphere_buffer);
    destroy_vertex_buffer(*renderer.triangle_buffer);
    destroy_vertex_buffer(*renderer.line_buffer);
    destroy_shader(*renderer.lit_shader);
    destroy_shader(*renderer.wireframe_shader);
    destroy_shader(*renderer.triangle_shader);
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
        speed := 15 * renderer.window.frame_time;
        velocity := v3f.{ 0, 0, 0 };
        
        if renderer.window.key_held[.Shift] speed *= 2;
        
        if renderer.window.key_held[.W] {
            velocity.x += sinf(yaw);
            velocity.z -= cosf(yaw);
        }

        if renderer.window.key_held[.S] {
            velocity.x -= sinf(yaw);
            velocity.z += cosf(yaw);
        }

        if renderer.window.key_held[.A] {
            velocity.x -= cosf(yaw);
            velocity.z -= sinf(yaw);
        }

        if renderer.window.key_held[.D] {
            velocity.x += cosf(yaw);
            velocity.z += sinf(yaw);
        }

        if renderer.window.key_held[.X] velocity.y += 1;

        if renderer.window.key_held[.Y] velocity.y -= 1;

        if v3f_length_squared(velocity) > F32_EPSILON {
            velocity = v3f_mul_f(v3f_normalize(velocity), speed);
            renderer.camera.position.x += velocity.x;
            renderer.camera.position.y += velocity.y;
            renderer.camera.position.z += velocity.z;
        }
        
        if renderer.window.button_held[.Middle] {
            renderer.camera.rotation.x += xx renderer.window.raw_mouse_delta_y * 0.0002;
            renderer.camera.rotation.y += xx renderer.window.raw_mouse_delta_x * 0.0002;
        }
    }

    //
    // Update the camera matrices.
    //

    renderer.camera.aspect     = xx renderer.window.width / xx renderer.window.height;
    renderer.camera.projection = make_perspective_projection_matrix(renderer.camera.fov, renderer.camera.aspect, renderer.camera.near, renderer.camera.far);
    renderer.camera.view       = make_view_matrix(renderer.camera.position, renderer.camera.rotation);

    renderer.camera.forward = .{ -renderer.camera.view._m[0][2], -renderer.camera.view._m[1][2], -renderer.camera.view._m[2][2] };
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

flush_triangles :: (renderer: *Renderer) {
    if renderer.triangle_count == 0 return;

    set_cull_test(.Disabled);
    set_depth_test(.Default);
    set_blending(.Default);
    
    set_shader(*renderer.triangle_shader);
    set_shader_uniform_m4f(*renderer.triangle_shader, "u_projection", renderer.camera.projection._m[0].data);
    set_shader_uniform_m4f(*renderer.triangle_shader, "u_view", renderer.camera.view._m[0].data);

    set_vertex_buffer(*renderer.triangle_buffer);
    update_vertex_data(*renderer.triangle_buffer, 0, renderer.triangle_vertices, renderer.triangle_count * 3 * 7);
    draw_vertex_buffer(*renderer.triangle_buffer);

    renderer.triangle_count = 0;
}

add_triangle_corner :: (renderer: *Renderer, index: s64, point: v3f, color: [4]f32) {
    idx := renderer.triangle_count * 3 * 7 + index * 7;
    
    renderer.triangle_vertices[idx + 0] = point.x;
    renderer.triangle_vertices[idx + 1] = point.y;
    renderer.triangle_vertices[idx + 2] = point.z;
    renderer.triangle_vertices[idx + 3] = color[0];
    renderer.triangle_vertices[idx + 4] = color[1];
    renderer.triangle_vertices[idx + 5] = color[2];
    renderer.triangle_vertices[idx + 6] = color[3];
}

draw_triangle :: (renderer: *Renderer, p0: v3f, p1: v3f, p2: v3f, color: GFX_Color) {
    if renderer.triangle_count == TRIANGLE_BATCH_COUNT flush_triangles(renderer);

    triangle: Internal_Triangle = .{ ---, p0, p1, p2, xx color.r / 255., xx color.g / 255., xx color.b / 255., xx color.a / 255. };
    draw_internal_triangle(renderer, *triangle);
}

draw_internal_triangle :: (renderer: *Renderer, triangle: *Internal_Triangle) {
    if renderer.triangle_count == TRIANGLE_BATCH_COUNT flush_triangles(renderer);

    color_array: [4]f32 = { triangle.r, triangle.g, triangle.b, triangle.a };
    
    add_triangle_corner(renderer, 0, triangle.p0, color_array);
    add_triangle_corner(renderer, 1, triangle.p1, color_array);
    add_triangle_corner(renderer, 2, triangle.p2, color_array);
    
    ++renderer.triangle_count;
}

draw_sphere :: (renderer: *Renderer, center: v3f, size: v3f, color: GFX_Color) {
    transformation := make_transformation_matrix_with_v3f_rotation(center, .{ 0, 0, 0 }, size);

    set_cull_test(.Backfaces);
    set_depth_test(.Default);
    
    set_shader(*renderer.lit_shader);
    set_shader_uniform_m4f(*renderer.lit_shader, "u_projection", renderer.camera.projection._m[0].data);
    set_shader_uniform_m4f(*renderer.lit_shader, "u_view", renderer.camera.view._m[0].data);
    set_shader_uniform_m4f(*renderer.lit_shader, "u_transformation", transformation._m[0].data);
    set_shader_uniform_v3f(*renderer.lit_shader, "u_light_direction", renderer.light_direction.x, renderer.light_direction.y, renderer.light_direction.z);
    set_shader_uniform_color(*renderer.lit_shader, "u_color", color.r, color.g, color.b, color.a);

    set_vertex_buffer(*renderer.sphere_buffer);
    draw_vertex_buffer(*renderer.sphere_buffer);
}

draw_cuboid :: (renderer: *Renderer, center: v3f, size: v3f, rotation: v3f, color: GFX_Color) {
    transformation := make_transformation_matrix_with_v3f_rotation(center, rotation, size);

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

    width := query_text_width(*renderer.hud_font, text) + 10;

    bg_color :: GFX_Color.{ 50, 50, 50, 200 };
    
    gfx_draw_quad(renderer.gfx, .{ screen_position.x, screen_position.y + xx renderer.hud_font.descender }, .{ xx width, xx renderer.hud_font.line_height }, bg_color);
    gfx_flush_quads(renderer.gfx);

    gfx_draw_text_without_background(renderer.gfx, *renderer.hud_font, text, screen_position, .Centered, color);
    gfx_flush_text(renderer.gfx);
}



/* --------------------------------------------- Higher Level API --------------------------------------------- */

draw_debug_draw_data :: (renderer: *Renderer, data: *Debug_Draw_Data, background_color: GFX_Color) {
    update_camera(renderer);

    prepare_3d(renderer, background_color);

    /* Render all solid objects. */
    for i := 0; i < data.cuboid_count; ++i {
        draw_cuboid(renderer, data.cuboids[i].position, data.cuboids[i].size, data.cuboids[i].rotation, .{ data.cuboids[i].r, data.cuboids[i].g, data.cuboids[i].b, 255 });
    }

    for i := 0; i < data.sphere_count; ++i {
        draw_sphere(renderer, data.spheres[i].position, .{ data.spheres[i].radius, data.spheres[i].radius, data.spheres[i].radius }, .{ data.spheres[i].r, data.spheres[i].g, data.spheres[i].b, 255 });
    }

    /* Render all lines. */
    for i := 0; i < data.line_count; ++i {
        draw_line(renderer, data.lines[i].p0, data.lines[i].p1, data.lines[i].thickness, .{ data.lines[i].r, data.lines[i].g, data.lines[i].b, 255 });
    }
    flush_lines(renderer);

    /* Render the (potentially) transparent triangles last. Since these triangles can be transparent, we need
       to sort them so that the one furthest away is rendered first, and the nearest is rendered last. */
    renderer.sorted_triangles = allocate_array(renderer.frame_allocator, data.triangle_count, Internal_Triangle);
    
    for i := 0; i < data.triangle_count; ++i {
        triangle := *data.triangles[i];

        triangle_center := v3f.{ (triangle.p0.x + triangle.p1.x + triangle.p2.x) / 3, (triangle.p0.y + triangle.p1.y + triangle.p2.y) / 3, (triangle.p0.z + triangle.p1.z + triangle.p2.z) / 3 };
        distance_to_camera := v3f_length_squared(.{ renderer.camera.position.x - triangle_center.x, renderer.camera.position.y - triangle_center.y, renderer.camera.position.z - triangle_center.z });

        renderer.sorted_triangles[i] = .{ distance_to_camera, triangle.p0, triangle.p1, triangle.p2, xx triangle.r / 255., xx triangle.g / 255., xx triangle.b / 255., xx triangle.a / 255. };
    }

    sort(renderer.sorted_triangles, compare_internal_triangle);

    for i := 0; i < renderer.sorted_triangles.count; ++i {
        draw_internal_triangle(renderer, *renderer.sorted_triangles[i]);
    }
    flush_triangles(renderer);

    /* Resolve multisampled hdr. */
    finish_3d(renderer);

    /* Draw all hud text. */
    for i := 0; i < data.text_count; ++i {
        draw_3d_hud_text(renderer, data.texts[i].position, data.texts[i].text, .{ data.texts[i].r, data.texts[i].g, data.texts[i].b, 255 });
    }
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

triangle_shader_code :: "
@Vertex(330 core)
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;

out vec4 pass_color;

uniform mat4 u_projection;
uniform mat4 u_view;

void main(void) {
    gl_Position = u_projection * u_view * vec4(in_position, 1.0);
    pass_color = in_color;
}

@Fragment(140)
in vec4 pass_color;

out vec4 out_color;

void main(void) {
    out_color = pass_color;
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

get_sphere_vertices :: (radius: f32, segments: s64) -> []f32, []f32 {
    vertices, normals: []f32;
    vertices = allocate_array(Default_Allocator, segments * segments * 6 * 3, f32);
    normals  = allocate_array(Default_Allocator, segments * segments * 6 * 3, f32);

    idx := 0;
    angle_per_segment := FPI / cast(f32) segments;
    
    for y0i := 0; y0i < segments; ++y0i {
        y1i := y0i + 1;
        
        y0 := sinf(FPI / 2 - xx y0i * angle_per_segment) * radius;
        y1 := sinf(FPI / 2 - xx y1i * angle_per_segment) * radius;

        r0 := cosf(FPI / 2 - xx y0i * angle_per_segment) * radius;
        r1 := cosf(FPI / 2 - xx y1i * angle_per_segment) * radius;

        for x0i := 0; x0i < segments; ++x0i {
            x1i := (x0i + 1) % segments;

            theta0 := cast(f32) x0i / cast(f32) segments * 2 * FPI;
            theta1 := cast(f32) x1i / cast(f32) segments * 2 * FPI;

            p0 := v2f.{ cosf(theta0), sinf(theta0) };
            p1 := v2f.{ cosf(theta1), sinf(theta1) };

            p00 := v3f.{ cosf(theta0) * r0, y0, sinf(theta0) * r0 };
            p01 := v3f.{ cosf(theta0) * r1, y1, sinf(theta0) * r1 };
            p10 := v3f.{ cosf(theta1) * r0, y0, sinf(theta1) * r0 };
            p11 := v3f.{ cosf(theta1) * r1, y1, sinf(theta1) * r1 };

            n00 := v3f_normalize(p00);
            n01 := v3f_normalize(p01);
            n10 := v3f_normalize(p10);
            n11 := v3f_normalize(p11);

            vertices[idx + 0] = p10.x;
            vertices[idx + 1] = p10.y;
            vertices[idx + 2] = p10.z;

            vertices[idx + 3] = p01.x;
            vertices[idx + 4] = p01.y;
            vertices[idx + 5] = p01.z;

            vertices[idx + 6] = p00.x;
            vertices[idx + 7] = p00.y;
            vertices[idx + 8] = p00.z;

            vertices[idx + 9]  = p10.x;
            vertices[idx + 10] = p10.y;
            vertices[idx + 11] = p10.z;

            vertices[idx + 12] = p11.x;
            vertices[idx + 13] = p11.y;
            vertices[idx + 14] = p11.z;

            vertices[idx + 15] = p01.x;
            vertices[idx + 16] = p01.y;
            vertices[idx + 17] = p01.z;

            normals[idx + 0] = n10.x;
            normals[idx + 1] = n10.y;
            normals[idx + 2] = n10.z;

            normals[idx + 3] = n01.x;
            normals[idx + 4] = n01.y;
            normals[idx + 5] = n01.z;

            normals[idx + 6] = n00.x;
            normals[idx + 7] = n00.y;
            normals[idx + 8] = n00.z;

            normals[idx + 9]  = n10.x;
            normals[idx + 10] = n10.y;
            normals[idx + 11] = n10.z;

            normals[idx + 12] = n11.x;
            normals[idx + 13] = n11.y;
            normals[idx + 14] = n11.z;

            normals[idx + 15] = n01.x;
            normals[idx + 16] = n01.y;
            normals[idx + 17] = n01.z;

            idx += 18;
        }
    }
    
    return vertices, normals;
}

compare_internal_triangle :: (lhs: *Internal_Triangle, rhs: *Internal_Triangle) -> int {
    if lhs.dot_to_camera < rhs.dot_to_camera return 1;
    if lhs.dot_to_camera > rhs.dot_to_camera return -1;
    return 0;
}

ray_double_sided_plane_intersection :: (ray_origin: v3f, ray_direction: v3f, plane_point: v3f, plane_normal: v3f) -> bool, f32 {
    denom := abs(v3f_dot_v3f(plane_normal, ray_direction));
    if denom > F32_EPSILON {
        p0l0 := v3f.{ plane_point.x - ray_origin.x, plane_point.y - ray_origin.y, plane_point.z - ray_origin.z };
        distance := abs(v3f_dot_v3f(p0l0, plane_normal) / denom);
        return true, distance;
    }

    return false, MIN_F32;

}
