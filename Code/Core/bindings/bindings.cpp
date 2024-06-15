#include "bindings.h"

#include "../src/core.h"
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
    world->reserve_objects(count, 0);

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
    World_Handle handle = core_do_octree_test(false);
    //World_Handle handle = core_do_center_block_test(false);
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
        world->add_anchor(vec3((real) x, (real) y, (real) z));
        return world->anchors.count - 1;
    }

    s64 core_add_delimiter(World_Handle world_handle, f64 x, f64 y, f64 z, f64 hx, f64 hy, f64 hz, f64 rx, f64 ry, f64 rz) {
        World *world = (World *) world_handle;
        world->add_delimiter(vec3((real) x, (real) y, (real) z), vec3((real) hx, (real) hy, (real) hz), vec3((real) rx, (real) ry, (real) rz));
        return world->delimiters.count - 1;
    }

    void core_add_delimiter_clipping_planes(World_Handle world_handle, s64 delimiter_index, s64 axis) {
        World *world = (World *) world_handle;
        world->add_delimiter_clipping_planes(&world->delimiters[delimiter_index], (Axis) axis);
    }

    void core_calculate_volumes(World_Handle world_handle) {
        World *world = (World *) world_handle;
        world->clip_delimiters(false);
        world->calculate_volumes();
    }


    
#if FOUNDATION_DEVELOPER
    /* ----------------------------------------------- Testing ------------------------------------------------ */

    void core_do_world_step(World_Handle world_handle, b8 step_mode) {
        World *world = (World *) world_handle;
        world->clip_delimiters(step_mode);
    }

    World_Handle core_do_house_test(b8 step_into) {
        tmZone("do_house_test", TM_SYSTEM_COLOR);

        World *world = (World *) core_create_world(100, 10, 100);
        world->add_anchor("Kitchen"_s, vec3(-5, -3, -5));
        world->add_anchor("Living Room"_s, vec3(5, -3, -5));
        world->add_anchor("Hallway"_s, vec3(-5, -3, 8.5));
        world->add_anchor("Garden"_s, vec3(0, -3, -30));
        
        auto hallway_wall = world->add_delimiter("HallwayWall"_s, vec3(-2, -3, +6), vec3(7.5, .25, .5), vec3(0, 0, 0));
        world->add_delimiter_clipping_planes(hallway_wall, AXIS_Z, VIRTUAL_EXTENSION_Positive_U | VIRTUAL_EXTENSION_Positive_V | VIRTUAL_EXTENSION_Negative_V);

        auto kitchen_wall0 = world->add_delimiter("KitchenWall0"_s, vec3(0, -3, -7), vec3(.5, .25, 2.5), vec3(0, 0, 0));
        world->add_delimiter_clipping_planes(kitchen_wall0, AXIS_X, VIRTUAL_EXTENSION_Positive_U | VIRTUAL_EXTENSION_Negative_U | VIRTUAL_EXTENSION_Positive_V);
        
        auto kitchen_wall1 = world->add_delimiter("KitchenWall1"_s, vec3(-7, -3, 0), vec3(2.5, .25, .5), vec3(0, 0, 0));
        world->add_delimiter_clipping_planes(kitchen_wall1, AXIS_Z, VIRTUAL_EXTENSION_Positive_U | VIRTUAL_EXTENSION_Positive_V | VIRTUAL_EXTENSION_Negative_V);
        
        auto outer_wall_north = world->add_delimiter("OuterWallNorth"_s, vec3(0, -3, -10), vec3(10, .25, .5), vec3(0, 0, 0));
        world->add_delimiter_clipping_planes(outer_wall_north, AXIS_Z);

        auto outer_wall_south = world->add_delimiter("OuterWallSouth"_s, vec3(0, -3, +10), vec3(10, .25, .5), vec3(0, 0, 0));
        world->add_delimiter_clipping_planes(outer_wall_south, AXIS_Z);

        auto outer_wall_east = world->add_delimiter("OuterWallEast"_s, vec3(+10, -3, 0), vec3(.5, .25, 10), vec3(0, 0, 0));
        world->add_delimiter_clipping_planes(outer_wall_east, AXIS_X);

        auto outer_wall_west = world->add_delimiter("OuterWallWest"_s, vec3(-10, -3, 0), vec3(.5, .25, 10), vec3(0, 0, 0));
        world->add_delimiter_clipping_planes(outer_wall_west, AXIS_X);
        
        world->create_octree();
        world->clip_delimiters(step_into);
        world->calculate_volumes();

        return (World_Handle) world;
    }

    World_Handle core_do_octree_test(b8 step_into) {
        tmZone("do_octree_test", TM_SYSTEM_COLOR);
        
        World *world = (World *) core_create_world(100, 40, 100);
        
        {
            tmZone("create_random_delimiters", TM_SYSTEM_COLOR);

            const s64 count = 10000;

            const real width  = world->half_size.x;
            const real height = world->half_size.y;
            const real length = world->half_size.z;
            
            const real min_size = 0.01f;
            const real max_size = 1.f;

            world->reserve_objects(0, count);

            for(s64 i = 0; i < count; ++i) {
                vec3 size     = vec3(get_random_real_uniform(min_size, max_size), get_random_real_uniform(min_size, max_size), get_random_real_uniform(min_size, max_size));
                vec3 position = vec3(get_random_real_uniform(-width  + size.x, width  - size.x),
                                   get_random_real_uniform(-height + size.y, height - size.y),
                                   get_random_real_uniform(-length + size.z, length - size.z));
                vec3 rotation = vec3(0, 0, 0);
                world->add_delimiter("Delimiter"_s, position, size, rotation);
            }
        }
        
        world->create_octree();
        return world;
    }

    World_Handle core_do_large_volumes_test(b8 step_into) {
        tmZone("do_large_volumes_test", TM_SYSTEM_COLOR);
        
        World *world = (World *) core_create_world(100, 40, 100);
        
        {
            tmZone("create_random_delimiters", TM_SYSTEM_COLOR);

            const s64 count = 1000;

            const real width  = world->half_size.x;
            const real height = world->half_size.y;
            const real length = world->half_size.z;
            
            const real min_size   = static_cast<real>(5);
            const real max_size   = static_cast<real>(15.);
            const real small_size = static_cast<real>(.1);

            world->reserve_objects(0, count);

            for(s64 i = 0; i < count; ++i) {
                int small_dimension = get_random_u32(0, 3);
                
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

                auto *delimiter = world->add_delimiter("Delimiter"_s, position, size, rotation);
                world->add_delimiter_clipping_planes(delimiter, (Axis) small_dimension);
            }
        }

        {
            tmZone("create_random_anchors", TM_SYSTEM_COLOR);
            create_random_anchors(world, 100);
        }

        world->create_octree();
        world->clip_delimiters(step_into);
        world->calculate_volumes();
        return world;
    }
    
    World_Handle core_do_cutout_test(b8 step_into) {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_create_world(50, 10, 50);

        Delimiter *b0 = world->add_delimiter("Delimiter"_s, vec3(0, 0, -5), vec3(5, .5, .5), vec3(0, 0, 0));
        world->add_delimiter_clipping_planes(b0, AXIS_Z);
        
        Delimiter *b1 = world->add_delimiter("Delimiter"_s, vec3(0, 0, +5), vec3(5, .5, .5), vec3(0, 0, 0));
        world->add_delimiter_clipping_planes(b1, AXIS_Z);

        Delimiter *b2 = world->add_delimiter("Delimiter"_s, vec3(-5, 0, 0), vec3(.5, .5, 5), vec3(0, 0, 0));
        world->add_delimiter_clipping_planes(b2, AXIS_X);
        
        Delimiter *b3 = world->add_delimiter("Delimiter"_s, vec3(+5, 0, 0), vec3(.5, .5, 5), vec3(0, 0, 0));
        world->add_delimiter_clipping_planes(b3, AXIS_X);

        world->add_anchor("Inside"_s, vec3(0, 0, 0));
        world->add_anchor("Outside"_s, vec3(0, 0, -10));

        world->create_octree();
        world->clip_delimiters(step_into);
        world->calculate_volumes();
        return world;
    }

    World_Handle core_do_circle_test(b8 step_into) {
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

            Delimiter *b = world->add_delimiter("Delimiter"_s, position, size, rotation);
            world->add_delimiter_clipping_planes(b, AXIS_Z);
        }

        world->add_anchor("Inside"_s, vec3(0, 0, 0));
        world->add_anchor("Outside"_s, vec3(0, 0, -40));

        world->create_octree();
        world->clip_delimiters(step_into);
        world->calculate_volumes();
        return world;
    }

    World_Handle core_do_u_shape_test(b8 step_into) {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_create_world(50, 10, 50);

        Delimiter *b0 = world->add_delimiter("Delimiter North"_s, vec3(0, 0, -10), vec3(10, .5, .5), vec3(0));
        world->add_delimiter_clipping_planes(b0, AXIS_Z);
        
        Delimiter *b1 = world->add_delimiter("Delimiter West"_s, vec3(10, 0, 0), vec3(.5, .5, 10), vec3(0));
        world->add_delimiter_clipping_planes(b1, AXIS_X);

        Delimiter *b2 = world->add_delimiter("Delimiter East"_s, vec3(-10, 0, 0), vec3(.5, .5, 10), vec3(0));
        world->add_delimiter_clipping_planes(b2, AXIS_X);
        
        world->add_anchor("Inside"_s, vec3(0, 0, 0));
        world->add_anchor("Outside"_s, vec3(0, 0, -20));
        
        world->create_octree();
        world->clip_delimiters(step_into);
        world->calculate_volumes();
        return world;
    }

    World_Handle core_do_center_block_test(b8 step_into) {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_create_world(50, 10, 50);

        Delimiter *delimiter = world->add_delimiter("Center Block"_s, vec3(0, 0, 0), vec3(5, 5, 5), vec3(0.125, 0, 0.125));
        world->add_delimiter_clipping_planes(delimiter, AXIS_X);
        world->add_delimiter_clipping_planes(delimiter, AXIS_Z);

        world->add_anchor("Outside"_s, vec3(0, 0, -10));
        
        world->create_octree();
        world->clip_delimiters(step_into);
        world->calculate_volumes();
        return world;
    }

    World_Handle core_do_jobs_test(b8 step_into) {   
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
    


    /* ---------------------------------------------- Debug Draw ---------------------------------------------- */

    Debug_Draw_Data core_debug_draw_world(World_Handle world, Debug_Draw_Options options) {
        return debug_draw_world((World *) world, options);
    }

    void core_free_debug_draw_data(Debug_Draw_Data *data) {
        free_debug_draw_data(data);
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
    tmSetColor(TM_SYSTEM_COLOR, 209, 202, 197);
    tmSetColor(TM_WORLD_COLOR,   95, 230,  46);
    tmSetColor(TM_OCTREE_COLOR,  46, 184, 230);
    tmSetColor(TM_ANCHOR_COLOR, 215,  15, 219);
    return true;
}
#endif
