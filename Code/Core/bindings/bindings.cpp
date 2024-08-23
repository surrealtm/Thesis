#include "bindings.h"

#include "../src/world.h"
#include "../src/bvh.h" // For the BVH test
#include "../src/random.h"
#include "../src/os_specific.h"
#include "../src/jobs.h"
#include "../src/math/maths.h" // For FPI...

#include <malloc.h>


#if FOUNDATION_DEVELOPER
/* ------------------------------------------------- Helpers ------------------------------------------------- */

static b8 memory_tracking_enabled = false;

static
void create_random_anchors(World *world, s64 count) {
    tmFunction(TM_SYSTEM_COLOR);
    const real width  = world->half_size.x;
    const real height = world->half_size.y;
    const real length = world->half_size.z;

    for(s64 i = 0; i < count; ++i) {
        vec3 position = vec3(get_random_real_uniform(-width, width),
                             get_random_real_uniform(-height, height),
                             get_random_real_uniform(-length, length));
        Anchor *anchor = world->add_anchor(position);
        anchor->dbg_name = "Anchor"_s;
    }
}

static
void create_random_delimiters(World *world, s64 count) {
    tmFunction(TM_SYSTEM_COLOR);
    const real width  = world->half_size.x;
    const real height = world->half_size.y;
    const real length = world->half_size.z;
            
    const real min_size   = static_cast<real>(5);
    const real max_size   = static_cast<real>(15.);
    const real small_size = static_cast<real>(.1);

    for(s64 i = 0; i < count; ++i) {
        u64 small_dimension = default_random_generator.random_u64(0, AXIS_COUNT);
                
        vec3 size;

        switch(small_dimension) {
        case 0: size = vec3(small_size, get_random_real_uniform(min_size, max_size), get_random_real_uniform(min_size, max_size)); break;
        case 1: size = vec3(get_random_real_uniform(min_size, max_size), small_size, get_random_real_uniform(min_size, max_size)); break;
        case 2: size = vec3(get_random_real_uniform(min_size, max_size), get_random_real_uniform(min_size, max_size), small_size); break;
        }

        vec3 position = vec3(get_random_real_uniform(-width  + size.x, width  - size.x),
                             get_random_real_uniform(-height + size.y, height - size.y),
                             get_random_real_uniform(-length + size.z, length - size.z));
        vec3 rotation = vec3(0, 0, 0);

        auto *delimiter = world->add_delimiter("Delimiter"_s, position, size, rotation, 0);
        world->add_delimiter_plane(delimiter, (Axis_Index) small_dimension);
    }
}

static
Memory_Allocator_Information memory_allocator_information(string name, Allocator *allocator) {
    Memory_Allocator_Information information = { 0 };
    information.name = name;
    information.allocation_count      = allocator->stats.allocations;
    information.deallocation_count    = allocator->stats.deallocations;
    information.working_set_size      = allocator->stats.working_set;
    information.peak_working_set_size = allocator->stats.peak_working_set;
    return information;
}

static
void sample_job_procedure(void * /*user_pointer*/) {
    World_Handle handle = core_do_center_block_test();
    core_destroy_world(handle);
}
#endif


extern "C" {
    /* --------------------------------------------- General API --------------------------------------------- */
    
    World_Handle core_create_world(f64 x, f64 y, f64 z) {
        World *world = (World *) Default_Allocator->allocate(sizeof(World));
        *world = World(); // Default-initialize the world.
        world->create(vec3((real) x, (real) y, (real) z));

#if FOUNDATION_DEVELOPER
        if(memory_tracking_enabled) install_allocator_console_logger(world->allocator, "World");
#endif

        return (World_Handle) world;
    }

    void core_destroy_world(World_Handle world_handle) {
        if(world_handle == null) return;

        World *world = (World *) world_handle;
        world->destroy();
        Default_Allocator->deallocate(world);
    }

    s64 core_add_anchor(World_Handle world_handle, f64 x, f64 y, f64 z) {
        World *world = (World *) world_handle;
        Anchor *anchor = world->add_anchor(vec3((real) x, (real) y, (real) z));
        return anchor->id;
    }

    s64 core_add_delimiter(World_Handle world_handle, f64 x, f64 y, f64 z, f64 hx, f64 hy, f64 hz, f64 rx, f64 ry, f64 rz, f64 rw, u8 level) {
        World *world = (World *) world_handle;
        Delimiter *delimiter = world->add_delimiter(vec3((real) x, (real) y, (real) z), vec3((real) hx, (real) hy, (real) hz), quat((real) rx, (real) ry, (real) rz, (real) rw), level);
        return delimiter->id;
    }

    void core_add_delimiter_plane(World_Handle world_handle, s64 delimiter_index, Axis_Index axis, b8 centered, Virtual_Extension extension) {
        World *world = (World *) world_handle;
        world->add_delimiter_plane(&world->delimiters[delimiter_index], axis, centered, extension);
    }

    void core_calculate_volumes(World_Handle world_handle, f64 cell_world_space_size) {
        World *world = (World *) world_handle;
        world->calculate_volumes((real) cell_world_space_size);
    }

    s64 core_query_point(World_Handle world_handle, f64 x, f64 y, f64 z) {
        World *world = (World *) world_handle;
        Anchor *anchor = world->query(vec3((real) x, (real) y, (real) z));
        return anchor != null ? anchor->id : -1;
    }
    

    
    /* ---------------------------------------------- Debug Draw ---------------------------------------------- */

    Debug_Draw_Data core_debug_draw_world(World_Handle world, Debug_Draw_Options options) {
        return debug_draw_world((World *) world, options);
    }

    void core_free_debug_draw_data(Debug_Draw_Data *data) {
        free_debug_draw_data(data);
    }
    
    

#if FOUNDATION_DEVELOPER
    /* ----------------------------------------------- Testing ------------------------------------------------ */

    World_Handle core_do_house_test() {
        tmZone("do_house_test", TM_SYSTEM_COLOR);

        // nocheckin

        World *world = (World *) core_create_world(100, 10, 100);
        //world->add_anchor("Living Room"_s, vec3(5, -3, -5));
        //world->add_anchor("Kitchen"_s, vec3(-5, -3, -5));
        //world->add_anchor("Hallway"_s, vec3(-5, -3, 8.5));
        //world->add_anchor("Garden"_s, vec3(0, -3, -30));
        
        auto hallway_wall = world->add_delimiter("HallwayWall"_s, vec3(-2, -3, +6), vec3(7.5, .25, .5), vec3(0, 0, 0), 1);
        world->add_delimiter_plane(hallway_wall, AXIS_Z, true, VIRTUAL_EXTENSION_Positive_U | VIRTUAL_EXTENSION_Positive_V | VIRTUAL_EXTENSION_Negative_V);

        /*
        auto kitchen_wall0 = world->add_delimiter("KitchenWall0"_s, vec3(0, -3, -7), vec3(.5, .25, 2.5), vec3(0, 0, 0), 1);
        world->add_delimiter_plane(kitchen_wall0, AXIS_X, true, VIRTUAL_EXTENSION_Positive_U | VIRTUAL_EXTENSION_Negative_U | VIRTUAL_EXTENSION_Positive_V);
        
        auto kitchen_wall1 = world->add_delimiter("KitchenWall1"_s, vec3(-7, -3, 0), vec3(2.5, .25, .5), vec3(0, 0, 0), 1);
        world->add_delimiter_plane(kitchen_wall1, AXIS_Z, true, VIRTUAL_EXTENSION_Positive_U | VIRTUAL_EXTENSION_Positive_V | VIRTUAL_EXTENSION_Negative_V);
        
        auto outer_wall_north = world->add_delimiter("OuterWallNorth"_s, vec3(0, -3, -10), vec3(10, .25, .5), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(outer_wall_north, AXIS_Z);

        auto outer_wall_south = world->add_delimiter("OuterWallSouth"_s, vec3(0, -3, +10), vec3(10, .25, .5), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(outer_wall_south, AXIS_Z);

        auto outer_wall_east = world->add_delimiter("OuterWallEast"_s, vec3(+10, -3, 0), vec3(.5, .25, 10), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(outer_wall_east, AXIS_X);

        auto outer_wall_west = world->add_delimiter("OuterWallWest"_s, vec3(-10, -3, 0), vec3(.5, .25, 10), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(outer_wall_west, AXIS_X);
        */
        
        world->calculate_volumes();

        return (World_Handle) world;
    }

    World_Handle core_do_bvh_test() {
        tmZone("do_bvh_test", TM_SYSTEM_COLOR);
        
        Resizable_Array<Triangle> debug_mesh = build_sample_triangle_mesh(Default_Allocator);

        World *world = (World *) core_create_world(100, 40, 100);
        world->create_bvh_from_triangles(debug_mesh);
        return world;
    }

    World_Handle core_do_large_volumes_test() {
        tmZone("do_large_volumes_test", TM_SYSTEM_COLOR);
        
        World *world = (World *) core_create_world(100, 40, 100);

        const s64 delimiter_count = 10;
        const s64 anchor_count    = 10;
        world->reserve_objects(anchor_count, delimiter_count);
        create_random_delimiters(world, delimiter_count);
        create_random_anchors(world, anchor_count);

        world->calculate_volumes();

        return world;
    }
    
    World_Handle core_do_cutout_test() {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_create_world(50, 10, 50);

        Delimiter *b0 = world->add_delimiter("Delimiter"_s, vec3(0, 0, -5), vec3(5, .5, .5), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(b0, AXIS_Z);
        
        Delimiter *b1 = world->add_delimiter("Delimiter"_s, vec3(0, 0, +5), vec3(5, .5, .5), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(b1, AXIS_Z);

        Delimiter *b2 = world->add_delimiter("Delimiter"_s, vec3(-5, 0, 0), vec3(.5, .5, 5), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(b2, AXIS_X);
        
        Delimiter *b3 = world->add_delimiter("Delimiter"_s, vec3(+5, 0, 0), vec3(.5, .5, 5), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(b3, AXIS_X);

        world->add_anchor("Inside"_s, vec3(0, 0, 0));
        world->add_anchor("Outside"_s, vec3(0, 0, -10));

        world->calculate_volumes();

        return world;
    }

    World_Handle core_do_circle_test() {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_create_world(50, 1, 50);

        const s64 steps           = 12;
        const real radius         = 10;
        const real circumference  = static_cast<real>(2 * PI * radius);
        const real space_per_step = static_cast<real>(0.5);

        for(s64 i = 0; i < steps; ++i) {
            real theta    = (i * static_cast<real>(TAU)) / static_cast<real>(steps);
            vec3 position = vec3(static_cast<real>(sin(theta)) * radius, 0, static_cast<real>(cos(theta)) * radius);
            vec3 rotation = vec3(0, i / (real) steps, 0);
            vec3 size     = vec3(circumference / steps / 2 * (1. - space_per_step), .5, .5);

            Delimiter *b = world->add_delimiter("Delimiter"_s, position, size, rotation, 0);
            world->add_both_delimiter_planes(b, AXIS_Z);
        }

        world->add_anchor("Inside"_s, vec3(0, 0, 0));
        //world->add_anchor("Outside"_s, vec3(0, 0, -40));

        world->calculate_volumes();

        return world;
    }

    World_Handle core_do_u_shape_test() {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_create_world(50, 10, 50);

        Delimiter *b0 = world->add_delimiter("Delimiter North"_s, vec3(0, 0, -10), vec3(10, .5, .5), vec3(0), 0);
        world->add_both_delimiter_planes(b0, AXIS_Z);
        
        Delimiter *b1 = world->add_delimiter("Delimiter West"_s, vec3(10, 0, 0), vec3(.5, .5, 10), vec3(0), 0);
        world->add_both_delimiter_planes(b1, AXIS_X);

        Delimiter *b2 = world->add_delimiter("Delimiter East"_s, vec3(-10, 0, 0), vec3(.5, .5, 10), vec3(0), 0);
        world->add_both_delimiter_planes(b2, AXIS_X);
        
        world->add_anchor("Inside"_s, vec3(0, 0, 0));
        world->add_anchor("Outside"_s, vec3(0, 0, -20));
        
        world->calculate_volumes();

        return world;
    }

    World_Handle core_do_center_block_test() {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_create_world(50, 10, 50);

        Delimiter *delimiter = world->add_delimiter("Center Block"_s, vec3(0, 0, 0), vec3(5, 5, 5), vec3(0.125, 0, 0.125), 0);
        world->add_both_delimiter_planes(delimiter, AXIS_X);
        world->add_both_delimiter_planes(delimiter, AXIS_Z);

        world->add_anchor("Outside"_s, vec3(0, 0, -10));
        
        world->calculate_volumes();

        return world;
    }

    World_Handle core_do_gallery_test() {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_create_world(20, 10, 20);

        f64 outer = 12;
        f64 inner = 5;
        
        auto outer_wall_north = world->add_delimiter("OuterWallNorth"_s, vec3(0, -3, -outer), vec3(outer, .25, .5), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(outer_wall_north, AXIS_Z);

        auto outer_wall_south = world->add_delimiter("OuterWallSouth"_s, vec3(0, -3, +outer), vec3(outer, .25, .5), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(outer_wall_south, AXIS_Z);

        auto outer_wall_east = world->add_delimiter("OuterWallEast"_s, vec3(+outer, -3, 0), vec3(.5, .25, outer), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(outer_wall_east, AXIS_X);

        auto outer_wall_west = world->add_delimiter("OuterWallWest"_s, vec3(-outer, -3, 0), vec3(.5, .25, outer), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(outer_wall_west, AXIS_X);


        auto inner_wall_north = world->add_delimiter("InnerWallNorth"_s, vec3(0, -3, -inner), vec3(inner, .25, .5), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(inner_wall_north, AXIS_Z);

        auto inner_wall_south = world->add_delimiter("InnerWallSouth"_s, vec3(0, -3, +inner), vec3(inner, .25, .5), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(inner_wall_south, AXIS_Z);

        auto inner_wall_east = world->add_delimiter("InnerWallEast"_s, vec3(+inner, -3, 0), vec3(.5, .25, inner), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(inner_wall_east, AXIS_X);

        auto inner_wall_west = world->add_delimiter("InnerWallWest"_s, vec3(-inner, -3, 0), vec3(.5, .25, inner), vec3(0, 0, 0), 0);
        world->add_both_delimiter_planes(inner_wall_west, AXIS_X);

        
        world->add_anchor("Walkway"_s, vec3(0, 0, -9));
        
        world->calculate_volumes();

        return world;
    }

    World_Handle core_do_louvre_test() {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_create_world(20, 10, 20);
        
        f64 inner = 5.0;
        f64 angle = 0.1;

        const b8 only_centered = false;
        
        auto wall_north = world->add_delimiter("WallNorth"_s, vec3(0, 0, -inner), vec3(inner, .25, .5), vec3(+angle, 0, 0), 1);
        auto wall_south = world->add_delimiter("WallSouth"_s, vec3(0, 0, +inner), vec3(inner, .25, .5), vec3(-angle, 0, 0), 1);
        auto wall_east  = world->add_delimiter("WallEast"_s,  vec3(+inner, 0, 0), vec3(.5, .25, inner), vec3(0, 0, +angle), 1);       
        auto wall_west  = world->add_delimiter("WallWest"_s,  vec3(-inner, 0, 0), vec3(.5, .25, inner), vec3(0, 0, -angle), 1);

        if(only_centered) {
            world->add_delimiter_plane(wall_north, AXIS_Z, true);
            world->add_delimiter_plane(wall_south, AXIS_Z, true);
            world->add_delimiter_plane(wall_east,  AXIS_X, true);
            world->add_delimiter_plane(wall_west,  AXIS_X, true);
        } else {
            world->add_both_delimiter_planes(wall_north, AXIS_Z);
            world->add_both_delimiter_planes(wall_south, AXIS_Z);
            world->add_both_delimiter_planes(wall_east,  AXIS_X);
            world->add_both_delimiter_planes(wall_west,  AXIS_X);
        }

        auto bottom = world->add_delimiter("Bottom"_s, vec3(0, -1, 0), vec3(inner, .25, inner), vec3(0, 0, 0), 0);
        world->add_delimiter_plane(bottom, AXIS_Y);
        
        world->add_anchor("room"_s, vec3(0, 1, 0));

        world->calculate_volumes(1.f);

        return world;        
    }
    
    World_Handle core_do_jobs_test() {   
        tmFunction(TM_DEFAULT_COLOR);
        Job_System jobs;
        create_job_system(&jobs, 10);

        for(s64 i = 0; i < 15; ++i) {
            Job_Declaration decl;
            decl.procedure_pointer = sample_job_procedure;
            spawn_job(&jobs, decl);
        }

        destroy_job_system(&jobs, JOB_SYSTEM_Wait_On_All_Jobs);
        return null;
    }
    
    

    /* ---------------------------------------------- Profiling ---------------------------------------------- */

    void core_begin_profiling() {
        tmBegin();
    }

    void core_stop_profiling() {
        tmFinish();
    }

    void core_print_profiling(b8 include_timeline) {
        Timing_Output_Mode output = TIMING_OUTPUT_Summary;
        if(include_timeline) output |= TIMING_OUTPUT_Timeline;

        tmPrintToConsole(output, TIMING_OUTPUT_Sort_By_Exclusive);
    }

    Timing_Data core_get_profiling_data() {
        Timing_Data data = tmData(TIMING_OUTPUT_Sort_By_Exclusive);
        return data;
    }

    void core_free_profiling_data(Timing_Data *data) {
        tmFreeData(data);
        _tmDestroy(); // This is called at termination of the viewer, so make sure we don't leak any bytes even if telemetry has been used since the last core_get_profiling_data call.
    }


    Memory_Information core_get_memory_information(World_Handle world_handle) {
        World *world = (World *) world_handle;
        
        Memory_Information info = { 0 };
        info.os_working_set_size = os_get_working_set_size();
        info.allocator_count = 2;
        info.allocators = (Memory_Allocator_Information *) malloc(sizeof(Memory_Allocator_Information) * info.allocator_count); // Avoid this allocation to count towards the heap...
        info.allocators[0] = memory_allocator_information("Heap"_s, Default_Allocator);
        info.allocators[1] = memory_allocator_information("World"_s, world->allocator);
        return info;
    }

    void core_print_memory_information(World_Handle world_handle) {
        Memory_Information information = core_get_memory_information(world_handle);
        
        
        printf("\n\n\n");
        s32 half_length = (s32) (125 - cstring_length(" MEMORY INFORMATION ") + 1) / 2;
        
        {
            for(s32 i = 0; i < half_length; ++i) printf("-");
            printf(" MEMORY INFORMATION ");
            for(s32 i = 0; i < half_length; ++i) printf("-");
            printf("\n");
        }

        {
            for(s64 i = 0; i < information.allocator_count; ++i) {
                Memory_Allocator_Information *alloc = &information.allocators[i];
                
                f32 working_set, peak_working_set;
                Memory_Unit working_set_unit, peak_working_set_unit;

                working_set_unit = get_best_memory_unit(alloc->working_set_size, &working_set);
                peak_working_set_unit = get_best_memory_unit(alloc->peak_working_set_size, &peak_working_set);

                printf("  %.*s:%-*s %8" PRId64 " Allocations, %8" PRId64 " Deallocations,     %04.3f%s Working Set,     %04.3f%s Peak Working Set.\n", (u32) alloc->name.count, alloc->name.data, (u32) max(0, 10 - alloc->name.count), "", alloc->allocation_count, alloc->deallocation_count, working_set, memory_unit_suffix(working_set_unit), peak_working_set, memory_unit_suffix(peak_working_set_unit));
            }


            f32 os_working_set;
            Memory_Unit os_working_set_unit = get_best_memory_unit(information.os_working_set_size, &os_working_set);
            printf("\n");
            printf("OS Working Set: %08f%s\n", os_working_set, memory_unit_suffix(os_working_set_unit));
        }

        {
            for(s32 i = 0; i < half_length; ++i) printf("-");
            printf(" MEMORY INFORMATION ");
            for(s32 i = 0; i < half_length; ++i) printf("-");
            printf("\n");
        }

        core_free_memory_information(&information);
    }

    void core_free_memory_information(Memory_Information *info) {
        free(info->allocators);
        info->allocators = null;
        info->allocator_count = 0;
        info->os_working_set_size = 0;
    }


    void core_enable_memory_tracking() {
        install_allocator_console_logger(Default_Allocator, "Heap");
        memory_tracking_enabled = true;
    }

    void core_disable_memory_tracking() {
        clear_allocator_logger(Default_Allocator);
        memory_tracking_enabled = false;
    }
#endif
}


#if FOUNDATION_WIN32 && FOUNDATION_DEVELOPER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

BOOL WINAPI DllMain(HINSTANCE hinstance, DWORD reason, LPVOID reserved) {
    //
    // Set up some things regarding the core library.
    //
    tmSetColor(TM_SYSTEM_COLOR,    209, 202, 197);
    tmSetColor(TM_WORLD_COLOR,      95, 230,  46);
    tmSetColor(TM_BVH_COLOR,        46, 184, 230);
    tmSetColor(TM_TESSEL_COLOR,    255,   0, 224);
    tmSetColor(TM_FLOODING_COLOR,   93,  75, 255);
    tmSetColor(TM_MARCHING_COLOR,  255,  75,  75);
    tmSetColor(TM_OPTIMIZER_COLOR,   0, 255, 207);
    
    return true;
}
#endif
