LINE_BATCH_COUNT :: 512;

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
    camera: Camera;

    lit_shader: Shader;
    wireframe_shader: Shader;

    cube_buffer: Vertex_Buffer;

    line_buffer: Vertex_Buffer;
    line_vertices: []f32; // Position, Endpoint, Extension
    line_colors: []f32;
    line_count: s64;
}

create_renderer :: (renderer: *Renderer, window: *Window, allocator: *Allocator) {
    renderer.window = window;
    
    //
    // Set up shaders.
    //
    {
        create_shader_from_string(*renderer.lit_shader, lit_shader_code, "lit_shader");
        create_shader_from_string(*renderer.wireframe_shader, wireframe_shader_code, "wireframe_shader");
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
        set_vertex_buffer_name(*renderer.line_buffer, "cube_buffer");
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
    }
}

destroy_renderer :: (renderer: *Renderer, allocator: *Allocator) {
    deallocate_array(allocator, *renderer.line_vertices);
    deallocate_array(allocator, *renderer.line_colors);
    destroy_vertex_buffer(*renderer.cube_buffer);
    destroy_vertex_buffer(*renderer.line_buffer);
    destroy_shader(*renderer.lit_shader);
    destroy_shader(*renderer.wireframe_shader);
}



/* -------------------------------------------------- Camera -------------------------------------------------- */

update_camera :: (renderer: *Renderer) {
    //
    // Move the camera.
    //
    {
        // @Incomplete.
    }

    renderer.camera.aspect     = xx renderer.window.width / xx renderer.window.height;
    renderer.camera.projection = make_perspective_projection_matrix(renderer.camera.fov, renderer.camera.aspect, renderer.camera.near, renderer.camera.far);
    renderer.camera.view       = make_view_matrix(renderer.camera.position, renderer.camera.rotation);
}



/* ---------------------------------------------- Immediate API ---------------------------------------------- */

flush_lines :: (renderer: *Renderer) {
    if renderer.line_count == 0 return;

    set_shader(*renderer.wireframe_shader);
    set_shader_uniform_m4f(*renderer.wireframe_shader, "u_projection", renderer.camera.projection._m[0].data);
    set_shader_uniform_m4f(*renderer.wireframe_shader, "u_view", renderer.camera.view._m[0].data);
    set_shader_uniform_b(*renderer.wireframe_shader, "u_use_normal", false);
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
    
    color_array: [4]f32 = { xx color.r / 255., xx color.g / 255., xx color.b / 255., xx color.a / 255. };
    add_line_endpoint(renderer, 0, p0, p1, +thickness, color_array);
    add_line_endpoint(renderer, 1, p0, p1, -thickness, color_array);
    add_line_endpoint(renderer, 2, p1, p0, -thickness, color_array);

    add_line_endpoint(renderer, 3, p0, p1, -thickness, color_array);
    add_line_endpoint(renderer, 4, p1, p0, +thickness, color_array);
    add_line_endpoint(renderer, 5, p1, p0, -thickness, color_array);

    ++renderer.line_count;
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
    float diffuse = dot(pass_normal, u_light_direction);
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
    if(u_use_normal) {
        // Add the normal values in world space, then calculate the screen space position for that
        vec3 tangent = normalize(cross(in_endpoint - in_position, u_normal));
        gl_Position = u_projection * u_view * vec4(in_position + tangent * in_extension, 1.0);
    } else {
        // Calculate the screen space position of both endpoints, derive the normal from that to always
        // have the line triangles face the camera
        vec4 this_clip_space = to_clip_space(in_position);
        vec2 this_screen_space = to_screen_space(this_clip_space);

        vec4 endpoint_clip_space = to_clip_space(in_endpoint);
        vec2 endpoint_screen_space = to_screen_space(endpoint_clip_space);

        vec2 line_direction = normalize(endpoint_screen_space - this_screen_space);
        vec2 line_normal    = vec2(-line_direction.y, line_direction.x) * sign(this_clip_space.w) * in_extension / 2.0;

        if(u_use_constant_screen_size) {
            line_normal *= this_clip_space.w; // Maybe there is a smarter way of doing this without conditionals? Like multiplying the normal by just 1 if constant_screen_size is not set?
            line_normal.x /= u_aspect; // Adjust the normal to the aspect ratio of the screen
        }

        vec4 clip_offset = vec4(line_normal, 0.0, 0.0);
        gl_Position = this_clip_space + clip_offset;
    }

    pass_color = in_color;
}

@Fragment(140)
in vec4 pass_color;

out vec4 out_color;

void main(void) {
    out_color = pass_color;
}
";
