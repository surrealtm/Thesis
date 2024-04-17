#include "bindings.h"
#include "../src/core.h"
#include "../src/random.h"

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

extern "C" {
    /* --------------------------------------------- General API --------------------------------------------- */

    World_Handle core_allocate_world() {
        World *world = (World *) Default_Allocator->allocate(sizeof(World));
        *world = World(); // Default-initialize the world.
        return (World_Handle) world;
    }

    void core_destroy_world(World_Handle world_handle) {
        World *world = (World *) world_handle;
        world->destroy();
        Default_Allocator->deallocate(world);
    }

    
    
    /* ----------------------------------------------- Testing ------------------------------------------------ */

    World_Handle core_do_house_test() {
        tmZone("do_house_test", TM_SYSTEM_COLOR);

        World *world = (World *) core_allocate_world();
        world->create(v3f(100, 10, 100));
        world->add_anchor("Kitchen"_s, v3f(-5, -3, -5));
        world->add_anchor("Living Room"_s, v3f(5, -3, -5));
        world->add_anchor("Hallway"_s, v3f(-5, -3, 8.5));
        world->add_anchor("Garden"_s, v3f(0, -3, -30));
        
        auto hallway_wall = world->add_boundary("HallwayWall"_s, v3f(-2, -3, +6), v3f(8, .25, .5), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(hallway_wall, 2);

        auto kitchen_wall0 = world->add_boundary("KitchenWall0"_s, v3f(0, -3, -7), v3f(.5, .25, 3), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(kitchen_wall0, 0);
        
        auto kitchen_wall1 = world->add_boundary("KitchenWall1"_s, v3f(-7, -3, 0), v3f(3, .25, .5), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(kitchen_wall1, 2);
        
        auto outer_wall_north = world->add_boundary("OuterWallNorth"_s, v3f(0, -3, -10), v3f(10, .25, .5), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(outer_wall_north, 2);

        auto outer_wall_south = world->add_boundary("OuterWallSouth"_s, v3f(0, -3, +10), v3f(10, .25, .5), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(outer_wall_south, 2);

        auto outer_wall_east = world->add_boundary("OuterWallEast"_s, v3f(+10, -3, 0), v3f(.5, .25, 10), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(outer_wall_east, 0);

        auto outer_wall_west = world->add_boundary("OuterWallWest"_s, v3f(-10, -3, 0), v3f(.5, .25, 10), v3f(0, 0, 0));
        world->add_boundary_clipping_planes(outer_wall_west, 0);

        world->create_octree();
        world->calculate_volumes();
        
        return (World_Handle) world;
    }

    World_Handle core_do_octree_test() {
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

    World_Handle core_do_large_volumes_test() {
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
                world->add_boundary_clipping_planes(boundary, small_dimension);
            }
        }

        {
            tmZone("create_random_anchors", TM_SYSTEM_COLOR);
            create_random_anchors(world, 100);
        }

        world->create_octree();
        world->calculate_volumes();

        return world;
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

    void core_print_profiling() {
        tmPrintToConsole(TIMING_OUTPUT_Summary | TIMING_OUTPUT_Timeline, TIMING_OUTPUT_Sort_By_Exclusive);
    }

    Timing_Data core_get_profiling_data() {
        Timing_Data data = tmData(TIMING_OUTPUT_Sort_By_Exclusive);
        return data;
    }

    void core_free_profiling_data(Timing_Data *data) {
        tmFreeData(data);
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
    tmSetColor(TM_VOLUME_COLOR, 215,  15, 219);
    return true;
}
#endif
