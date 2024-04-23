#include "bindings.h"
#include "../src/core.h"
#include "../src/random.h"
#include "../src/os_specific.h"
#include "../src/jobs.h"

#include <malloc.h>

static
void create_random_anchors(World *world, s64 count) {
    world->reserve_objects(count, 0);

    const f32 width  = world->half_size.x;
    const f32 height = world->half_size.y;
    const f32 length = world->half_size.z;

    for(s64 i = 0; i < count; ++i) {
        v3f position = v3f(get_random_f32_uniform(-width, width),
                    get_random_f32_uniform(-height, height),
                    get_random_f32_uniform(-length, length));
        world->add_anchor("Anchor"_s, position);
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
void sample_job_procedure(void *user_pointer) {
    World_Handle handle = core_do_octree_test(false);
    //World_Handle handle = core_do_center_block_test(false);
    core_destroy_world(handle);
}

extern "C" {
    /* --------------------------------------------- General API --------------------------------------------- */

    World_Handle core_allocate_world() {
        World *world = (World *) Default_Allocator->allocate(sizeof(World));
        *world = World(); // Default-initialize the world.
        return (World_Handle) world;
    }

    void core_destroy_world(World_Handle world_handle) {
        if(world_handle == null) return;

        World *world = (World *) world_handle;
        world->destroy();
        Default_Allocator->deallocate(world);
    }

    
    
    /* ----------------------------------------------- Testing ------------------------------------------------ */

    void core_do_world_step(World_Handle world_handle, Debug_Draw_Data *draw_data, Debug_Draw_Options draw_options) {
        World *world = (World *) world_handle;
        world->calculate_volumes_step(true);

        if(draw_data) {
            core_free_debug_draw_data(draw_data);
            *draw_data = core_debug_draw_world(world_handle, draw_options);
        }
    }


    World_Handle core_do_house_test(b8 step_into) {
        tmZone("do_house_test", TM_SYSTEM_COLOR);

        World *world = (World *) core_allocate_world();
        world->create(v3f(100, 10, 100));
        world->add_anchor("Kitchen"_s, v3f(-5, -3, -5));
        world->add_anchor("Living Room"_s, v3f(5, -3, -5));
        world->add_anchor("Hallway"_s, v3f(-5, -3, 8.5));
        world->add_anchor("Garden"_s, v3f(0, -3, -30));
        
        auto hallway_wall = world->add_boundary("HallwayWall"_s, v3f(-2, -3, +6), v3f(8, .25, .5), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(hallway_wall, AXIS_Z);

        auto kitchen_wall0 = world->add_boundary("KitchenWall0"_s, v3f(0, -3, -7), v3f(.5, .25, 3), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(kitchen_wall0, AXIS_X);
        
        auto kitchen_wall1 = world->add_boundary("KitchenWall1"_s, v3f(-7, -3, 0), v3f(3, .25, .5), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(kitchen_wall1, AXIS_Z);
        
        auto outer_wall_north = world->add_boundary("OuterWallNorth"_s, v3f(0, -3, -10), v3f(10, .25, .5), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(outer_wall_north, AXIS_Z);

        auto outer_wall_south = world->add_boundary("OuterWallSouth"_s, v3f(0, -3, +10), v3f(10, .25, .5), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(outer_wall_south, AXIS_Z);

        auto outer_wall_east = world->add_boundary("OuterWallEast"_s, v3f(+10, -3, 0), v3f(.5, .25, 10), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(outer_wall_east, AXIS_X);

        auto outer_wall_west = world->add_boundary("OuterWallWest"_s, v3f(-10, -3, 0), v3f(.5, .25, 10), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(outer_wall_west, AXIS_X);

        world->create_octree();
        world->calculate_volumes(step_into);
        
        return (World_Handle) world;
    }

    World_Handle core_do_octree_test(b8 step_into) {
        tmZone("do_octree_test", TM_SYSTEM_COLOR);
        
        World *world = (World *) core_allocate_world();
        world->create(v3f(100, 40, 100));
        
        {
            tmZone("create_random_boundaries", TM_SYSTEM_COLOR);

            const s64 count = 10000;

            const f32 width  = world->half_size.x;
            const f32 height = world->half_size.y;
            const f32 length = world->half_size.z;
            
            const f32 min_size = 0.01f;
            const f32 max_size = 1.f;

            world->reserve_objects(0, count);

            for(s64 i = 0; i < count; ++i) {
                v3f size     = v3f(get_random_f32_uniform(min_size, max_size), get_random_f32_uniform(min_size, max_size), get_random_f32_uniform(min_size, max_size));
                v3f position = v3f(get_random_f32_uniform(-width  + size.x, width  - size.x),
                                    get_random_f32_uniform(-height + size.y, height - size.y),
                                    get_random_f32_uniform(-length + size.z, length - size.z));
                v3f rotation = v3f(0, 0, 0);
                world->add_boundary("Boundary"_s, position, size, rotation);
            }
        }
        
        world->create_octree();
        return world;
    }

    World_Handle core_do_large_volumes_test(b8 step_into) {
        tmZone("do_large_volumes_test", TM_SYSTEM_COLOR);
        
        World *world = (World *) core_allocate_world();
        world->create(v3f(100, 40, 100));
        
        {
            tmZone("create_random_boundaries", TM_SYSTEM_COLOR);

            const s64 count = 1000;

            const f32 width  = world->half_size.x;
            const f32 height = world->half_size.y;
            const f32 length = world->half_size.z;
            
            const f32 min_size = 5.f;
            const f32 max_size = 15.f;

            world->reserve_objects(0, count);

            for(s64 i = 0; i < count; ++i) {
                int small_dimension = get_random_u32(0, 3);
                
                v3f size;

                switch(small_dimension) {
                case 0: size = v3f(.1f, get_random_f32_uniform(min_size, max_size), get_random_f32_uniform(min_size, max_size)); break;
                case 1: size = v3f(get_random_f32_uniform(min_size, max_size), .1f, get_random_f32_uniform(min_size, max_size)); break;
                case 2: size = v3f(get_random_f32_uniform(min_size, max_size), get_random_f32_uniform(min_size, max_size), .1f); break;
                }

                v3f position = v3f(get_random_f32_uniform(-width  + size.x, width  - size.x),
                                    get_random_f32_uniform(-height + size.y, height - size.y),
                                    get_random_f32_uniform(-length + size.z, length - size.z));
                v3f rotation = v3f(0, 0, 0);

                auto *boundary = world->add_boundary("Boundary"_s, position, size, rotation);
                world->add_boundary_clipping_planes(boundary, (Axis) small_dimension);
            }
        }

        {
            tmZone("create_random_anchors", TM_SYSTEM_COLOR);
            create_random_anchors(world, 100);
        }

        world->create_octree();
        world->calculate_volumes(step_into);
        return world;
    }
    
    World_Handle core_do_cutout_test(b8 step_into) {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_allocate_world();
        world->create(v3f(50, 10, 50));

        Boundary *b0 = world->add_boundary("Boundary"_s, v3f(0, 0, -5), v3f(5, .5, .5), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(b0, AXIS_Z);
        
        Boundary *b1 = world->add_boundary("Boundary"_s, v3f(0, 0, +5), v3f(5, .5, .5), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(b1, AXIS_Z);

        Boundary *b2 = world->add_boundary("Boundary"_s, v3f(-5, 0, 0), v3f(.5, .5, 5), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(b2, AXIS_X);
        
        Boundary *b3 = world->add_boundary("Boundary"_s, v3f(+5, 0, 0), v3f(.5, .5, 5), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(b3, AXIS_X);

        world->add_anchor("Inside"_s, v3f(0, 0, 0));
        world->add_anchor("Outside"_s, v3f(0, 0, -10));

        world->create_octree();
        world->calculate_volumes(step_into);
        return world;
    }

    World_Handle core_do_circle_test(b8 step_into) {
        tmFunction(TM_SYSTEM_COLOR);

        // nocheckin
        World *world = (World *) core_allocate_world();
        world->create(v3f(50, .5, 50));

        Boundary *b0 = world->add_boundary("Boundary"_s, v3f(-10, 0, 0), v3f(.5, .5, 10), v3f(0));
        world->add_boundary_clipping_planes(b0, AXIS_X);

        Boundary *b1 = world->add_boundary("Boundary"_s, v3f(0, 0, -10), v3f(.5, .5, 10), v3f(0, +.125, 0));
        world->add_boundary_clipping_planes(b1, AXIS_X);

        Boundary *b2 = world->add_boundary("Boundary"_s, v3f(0, 0, +10), v3f(.5, .5, 10), v3f(0, -.125, 0));
        world->add_boundary_clipping_planes(b2, AXIS_X);

        world->add_anchor("Anchor"_s, v3f(0, 0, 0));

        world->create_octree();
        world->calculate_volumes(step_into);
        return world;

        /*
        World *world = (World *) core_allocate_world();
        world->create(v3f(50, 1, 50));

        const s64 steps = 5;
        const f32 radius = 10;
        const f32 circumference = 2 * FPI * radius;
        const f32 space_per_step = 0.5f;
        
        for(s64 i = 0; i < steps; ++i) {
            f32 theta    = i / (f32) steps * 2 * FPI;
            v3f position = v3f(sinf(theta) * radius, 0, cosf(theta) * radius);
            v3f rotation = v3f(0, i / (f32) steps, 0);
            v3f size     = v3f(circumference / steps / 2 * (1.f - space_per_step), .5, .5);

            Boundary *b = world->add_boundary("Boundary"_s, position, size, rotation);
            world->add_boundary_clipping_planes(b, AXIS_Z);
        }

        world->add_anchor("Inside"_s, v3f(0, 0, 0));
        world->add_anchor("Outside"_s, v3f(0, 0, -40));

        world->create_octree();
        world->calculate_volumes(step_into);
        return world;
        */
    }

    World_Handle core_do_u_shape_test(b8 step_into) {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_allocate_world();
        world->create(v3f(50, 10, 50));

        Boundary *b0 = world->add_boundary("Boundary"_s, v3f(0, 0, -10), v3f(10, .5, .5), v3f(0));
        world->add_boundary_clipping_planes(b0, AXIS_Z);

        Boundary *b1 = world->add_boundary("Boundary"_s, v3f(10, 0, 0), v3f(.5, .5, 10), v3f(0));
        world->add_boundary_clipping_planes(b1, AXIS_X);
        
        Boundary *b2 = world->add_boundary("Boundary"_s, v3f(-10, 0, 0), v3f(.5, .5, 10), v3f(0));
        world->add_boundary_clipping_planes(b2, AXIS_X);
        
        world->add_anchor("Inside"_s, v3f(0, 0, 0));
        world->add_anchor("Outside"_s, v3f(0, 0, -20));
        
        world->create_octree();
        world->calculate_volumes(step_into);
        return world;
    }
    
    World_Handle core_do_center_block_test(b8 step_into) {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_allocate_world();
        world->create(v3f(50, 10, 50));

        //Boundary *boundary = world->add_boundary("Boundary"_s, v3f(0, 0, 0), v3f(5, 5, 5), v3f(0.125, 0, 0.125));
        Boundary *boundary = world->add_boundary("Center Block"_s, v3f(0, 0, 0), v3f(5, 5, 5), v3f(0, 0, 0));
        //world->add_boundary_clipping_planes(boundary, AXIS_X);
        world->add_boundary_clipping_planes(boundary, AXIS_Z);

        world->add_anchor("Outside"_s, v3f(0, 0, -10));
        
        world->create_octree();
        world->calculate_volumes(step_into);
        return world;
    }


    World_Handle core_do_clipping_test(b8 step_into) {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_allocate_world();
        world->create(v3f(10, 5, 10));

        Boundary *boundary = world->add_boundary("Ground"_s, v3f(0, 0, 0), v3f(1, 1, 1), v3f(-0.0625, 0, 0));
        world->add_centered_boundary_clipping_plane(boundary, AXIS_X);

        Anchor *anchor = world->add_anchor("Anchor"_s, v3f(1, 1, 1));
        anchor->volume_triangles.add({ v3f(-4,  0, -4), v3f( 4,  0, -4), v3f( 4,  0,  4) });
        anchor->volume_triangles.add({ v3f(-4,  0, -4), v3f(-4,  0,  4), v3f( 4,  0,  4) });
        anchor->volume_triangles.add({ v3f(-4,  4, -4), v3f( 4,  4, -4), v3f( 4, -4, -4) });
        anchor->volume_triangles.add({ v3f(-4,  4, -4), v3f(-4, -4, -4), v3f( 4, -4, -4) });
        anchor->volume_triangles.add({ v3f(-4,  4, -4), v3f(-4,  4,  4), v3f(-4, -4,  4) });
        anchor->volume_triangles.add({ v3f(-4,  4, -4), v3f(-4, -4, -4), v3f(-4, -4,  4) });

        world->create_octree();
        world->calculate_volumes(step_into);

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

                printf("  %.*s:%-*s %8" PRId64 " Allocations, %8" PRId64 " Deallocations,     %08f%s Working Set,     %08f%s Peak Working Set.\n", (u32) alloc->name.count, alloc->name.data, (u32) max(0, 10 - alloc->name.count), "", alloc->allocation_count, alloc->deallocation_count, working_set, memory_unit_suffix(working_set_unit), peak_working_set, memory_unit_suffix(peak_working_set_unit));
            }


            f32 os_working_set;
            Memory_Unit os_working_set_unit = get_best_memory_unit(information.os_working_set_size, &os_working_set);
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
}


#if FOUNDATION_WIN32
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
