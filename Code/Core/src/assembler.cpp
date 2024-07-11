#include "assembler.h"
#include "world.h"
#include "floodfill.h"
#include "bvh.h"

#include "timing.h"

static
void maybe_add_entry_to_volume(World *world, vec3 cell_position, BVH_Entry* entry, Resizable_Array<Triangle> *volume) {
    // @Incomplete: Check for duplicates here, which could happen if a single triangle is in between two
    // flooded cells for example. We might just use a hash table with the entry pointer as a hash key?
    vec3 direction = cell_position - entry->center;
    if(!world->cast_ray_against_delimiters_and_root_planes(entry->center + direction * CORE_EPSILON, direction, 1.)) { // We add a little offset to the position here so that we don't find the triangle that we are actually casting from...
        volume->add(entry->triangle);
    }
}

void assemble(Resizable_Array<Triangle> *volume, World *world, Flood_Fill *ff) {
    tmFunction(TM_ASSEMBLING_COLOR);

    for(Cell *cell : ff->flooded_cells) {
        vec3 world_space_position = get_cell_world_space_center(ff, cell);
        auto leafs = world->bvh->find_leafs_at_position(world->allocator, world_space_position); // @@Speed: Again, temp allocator

        for(auto *leaf : leafs) {
            s64 one_plus_last_entry_index = leaf->first_entry_index + leaf->entry_count;
            for(s64 i = leaf->first_entry_index; i < one_plus_last_entry_index; ++i) {
                maybe_add_entry_to_volume(world, world_space_position, &world->bvh->entries[i], volume);
            }
        }
    }
}
