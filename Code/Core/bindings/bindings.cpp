#include "bindings.h"

#include "../src/core.h"
#include "../src/random.h"
#include "../src/os_specific.h"
#include "../src/jobs.h"
#include "../src/math/math.h" // For FPI...

#include <malloc.h>


#if FOUNDATION_DEVELOPER
static b8 memory_tracking_enabled = false;


/* --------------------------------------------- Memory Tracking --------------------------------------------- */

static
void allocation_callback(Allocator *allocator, const void *allocator_name, void *data, u64 size) {
    printf("[Allocation] %s : %" PRIu64 "b, 0x%016" PRIx64 "\n", (char *) allocator_name, size, (u64) data);
}

static
void deallocation_callback(Allocator *allocator, const void *allocator_name, void *data, u64 size) {
    printf("[Deallocation] %s : %" PRIu64 "b, 0x%016" PRIx64 "\n", (char *) allocator_name, size, (u64) data);    
}

static
void reallocation_callback(Allocator *allocator, const void *allocator_name, void *old_data, u64 old_size, void *new_data, u64 new_size) {
    printf("[Reallocation] %s : %" PRIu64 "b, 0x%016" PRIx64 " -> %" PRIu64 "b, 0x%016" PRIx64 "\n", (char *) allocator_name, old_size, (u64) old_data, new_size, (u64) new_data);    
}

static
void clear_callback(Allocator *allocator, const void *allocator_name) {
    printf("[Clear] %s\n", (char *) allocator_name);
}

static
void set_allocation_callbacks(Allocator *allocator, const char *name) {
    allocator->callbacks.allocation_callback   = allocation_callback;
    allocator->callbacks.deallocation_callback = deallocation_callback;
    allocator->callbacks.reallocation_callback = reallocation_callback;
    allocator->callbacks.clear_callback        = clear_callback;
    allocator->callbacks.user_pointer          = name;
}

static
void clear_allocation_callbacks(Allocator *allocator) {
    allocator->callbacks.allocation_callback   = null;
    allocator->callbacks.deallocation_callback = null;
    allocator->callbacks.reallocation_callback = null;
    allocator->callbacks.clear_callback        = null;
    allocator->callbacks.user_pointer          = null;
}



/* ------------------------------------------------- Helpers ------------------------------------------------- */

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
        if(memory_tracking_enabled) set_allocation_callbacks(world->allocator, "World");
#endif

        return (World_Handle) world;
    }

    void core_destroy_world(World_Handle world_handle) {
        if(world_handle == null) return;

        World *world = (World *) world_handle;
        world->destroy();
        Default_Allocator->deallocate(world);
    }

    
#if FOUNDATION_DEVELOPER
    /* ----------------------------------------------- Testing ------------------------------------------------ */

    void core_do_world_step(World_Handle world_handle, b8 step_mode) {
        World *world = (World *) world_handle;
        world->clip_boundaries(step_mode);
    }

    World_Handle core_do_house_test(b8 step_into) {
        tmZone("do_house_test", TM_SYSTEM_COLOR);

        World *world = (World *) core_create_world(100, 10, 100);
        world->add_anchor("Kitchen"_s, vec3(-5, -3, -5));
        world->add_anchor("Living Room"_s, vec3(5, -3, -5));
        world->add_anchor("Hallway"_s, vec3(-5, -3, 8.5));
        world->add_anchor("Garden"_s, vec3(0, -3, -30));
        
        auto hallway_wall = world->add_boundary("HallwayWall"_s, vec3(-2, -3, +6), vec3(8, .25, .5), vec3(0, 0, 0));
        world->add_boundary_clipping_planes(hallway_wall, AXIS_Z);

        auto kitchen_wall0 = world->add_boundary("KitchenWall0"_s, vec3(0, -3, -7), vec3(.5, .25, 3), vec3(0, 0, 0));
        world->add_boundary_clipping_planes(kitchen_wall0, AXIS_X);
        
        auto kitchen_wall1 = world->add_boundary("KitchenWall1"_s, vec3(-7, -3, 0), vec3(3, .25, .5), vec3(0, 0, 0));
        world->add_boundary_clipping_planes(kitchen_wall1, AXIS_Z);
        
        auto outer_wall_north = world->add_boundary("OuterWallNorth"_s, vec3(0, -3, -10), vec3(10, .25, .5), vec3(0, 0, 0));
        world->add_boundary_clipping_planes(outer_wall_north, AXIS_Z);

        auto outer_wall_south = world->add_boundary("OuterWallSouth"_s, vec3(0, -3, +10), vec3(10, .25, .5), vec3(0, 0, 0));
        world->add_boundary_clipping_planes(outer_wall_south, AXIS_Z);

        auto outer_wall_east = world->add_boundary("OuterWallEast"_s, vec3(+10, -3, 0), vec3(.5, .25, 10), vec3(0, 0, 0));
        world->add_boundary_clipping_planes(outer_wall_east, AXIS_X);

        auto outer_wall_west = world->add_boundary("OuterWallWest"_s, vec3(-10, -3, 0), vec3(.5, .25, 10), vec3(0, 0, 0));
        world->add_boundary_clipping_planes(outer_wall_west, AXIS_X);

        world->create_octree();
        world->clip_boundaries(step_into);
        
        return (World_Handle) world;
    }

    World_Handle core_do_octree_test(b8 step_into) {
        tmZone("do_octree_test", TM_SYSTEM_COLOR);
        
        World *world = (World *) core_create_world(100, 40, 100);
        
        {
            tmZone("create_random_boundaries", TM_SYSTEM_COLOR);

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
                world->add_boundary("Boundary"_s, position, size, rotation);
            }
        }
        
        world->create_octree();
        return world;
    }

    World_Handle core_do_large_volumes_test(b8 step_into) {
        tmZone("do_large_volumes_test", TM_SYSTEM_COLOR);
        
        World *world = (World *) core_create_world(100, 40, 100);
        
        {
            tmZone("create_random_boundaries", TM_SYSTEM_COLOR);

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

                auto *boundary = world->add_boundary("Boundary"_s, position, size, rotation);
                world->add_boundary_clipping_planes(boundary, (Axis) small_dimension);
            }
        }

        {
            tmZone("create_random_anchors", TM_SYSTEM_COLOR);
            create_random_anchors(world, 100);
        }

        world->create_octree();
        world->clip_boundaries(step_into);
        return world;
    }
    
    World_Handle core_do_cutout_test(b8 step_into) {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_create_world(50, 10, 50);

        Boundary *b0 = world->add_boundary("Boundary"_s, vec3(0, 0, -5), vec3(5, .5, .5), vec3(0, 0, 0));
        world->add_boundary_clipping_planes(b0, AXIS_Z);
        
        Boundary *b1 = world->add_boundary("Boundary"_s, vec3(0, 0, +5), vec3(5, .5, .5), vec3(0, 0, 0));
        world->add_boundary_clipping_planes(b1, AXIS_Z);

        Boundary *b2 = world->add_boundary("Boundary"_s, vec3(-5, 0, 0), vec3(.5, .5, 5), vec3(0, 0, 0));
        world->add_boundary_clipping_planes(b2, AXIS_X);
        
        Boundary *b3 = world->add_boundary("Boundary"_s, vec3(+5, 0, 0), vec3(.5, .5, 5), vec3(0, 0, 0));
        world->add_boundary_clipping_planes(b3, AXIS_X);

        world->add_anchor("Inside"_s, vec3(0, 0, 0));
        world->add_anchor("Outside"_s, vec3(0, 0, -10));

        world->create_octree();
        world->clip_boundaries(step_into);
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
            real theta    = i / static_cast<real>(steps * 2 * PI);
            vec3 position = vec3(static_cast<real>(sin(theta)) * radius, 0, static_cast<real>(cos(theta)) * radius);
            vec3 rotation = vec3(0, i / (real) steps, 0);
            vec3 size     = vec3(circumference / steps / 2 * (1. - space_per_step), .5, .5);

            Boundary *b = world->add_boundary("Boundary"_s, position, size, rotation);
            world->add_boundary_clipping_planes(b, AXIS_Z);
        }

        world->add_anchor("Inside"_s, vec3(0, 0, 0));
        world->add_anchor("Outside"_s, vec3(0, 0, -40));

        world->create_octree();
        world->clip_boundaries(step_into);
        return world;
    }

    World_Handle core_do_u_shape_test(b8 step_into) {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_create_world(50, 10, 50);

        Boundary *b0 = world->add_boundary("Boundary"_s, vec3(0, 0, -10), vec3(10, .5, .5), vec3(0));
        world->add_boundary_clipping_planes(b0, AXIS_Z);

        Boundary *b1 = world->add_boundary("Boundary"_s, vec3(10, 0, 0), vec3(.5, .5, 10), vec3(0));
        world->add_boundary_clipping_planes(b1, AXIS_X);
        
        Boundary *b2 = world->add_boundary("Boundary"_s, vec3(-10, 0, 0), vec3(.5, .5, 10), vec3(0));
        world->add_boundary_clipping_planes(b2, AXIS_X);
        
        world->add_anchor("Inside"_s, vec3(0, 0, 0));
        world->add_anchor("Outside"_s, vec3(0, 0, -20));
        
        world->create_octree();
        world->clip_boundaries(step_into);
        return world;
    }
    
    World_Handle core_do_center_block_test(b8 step_into) {
        tmFunction(TM_SYSTEM_COLOR);

        World *world = (World *) core_create_world(50, 10, 50);

        Boundary *boundary = world->add_boundary("Center Block"_s, vec3(0, 0, 0), vec3(5, 5, 5), vec3(0.125, 0, 0.125));
        world->add_boundary_clipping_planes(boundary, AXIS_X);
        world->add_boundary_clipping_planes(boundary, AXIS_Z);

        world->add_anchor("Outside"_s, vec3(0, 0, -10));
        
        world->create_octree();
        world->clip_boundaries(step_into);
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
        set_allocation_callbacks(Default_Allocator, "Heap");
        memory_tracking_enabled = true;
    }

    void core_disable_memory_tracking() {
        clear_allocation_callbacks(Default_Allocator);
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
