#include "assembler.h"
#include "world.h"
#include "floodfill.h"
#include "bvh.h"

#include "timing.h"
#include "hash_table.h"

#define USE_HASH_TABLE true

void assemble(Resizable_Array<Triangle> *volume, World *world, Flood_Fill *ff) {
    tmFunction(TM_ASSEMBLING_COLOR);

#if USE_HASH_TABLE
    // When assembling the triangles that make up a volume, we want to make sure that we
    // don't have duplicates in that volume. This could happen because a triangle might have
    // line-of-sight to many flood-filling cells, in which case it would be added multiple
    // times.
    // Therefore, we use this hashtable with the pointers to the triangles as keys to check
    // for duplicates.
    // @Incomplete: Check table stats and make sure the hash procedure actually works.
    Probed_Hash_Table<BVH_Entry *, b8> triangle_table;
    triangle_table.allocator = world->allocator;

    auto hash = [](BVH_Entry *const &key) -> u64 { return murmur_64a((u64) key); };
    triangle_table.create(world->bvh->entries.count, hash, [](BVH_Entry *const &lhs, BVH_Entry *const &rhs) -> b8 { return lhs == rhs; });
#endif
    
    for(Cell *cell : ff->flooded_cells) {
        vec3 world_space_position = get_cell_world_space_center(ff, cell);
        auto leafs = world->bvh->find_leafs_at_position(world->allocator, world_space_position); // @@Speed: Again, temp allocator

        for(auto *leaf : leafs) {
            s64 one_plus_last_entry_index = leaf->first_entry_index + leaf->entry_count;
            for(s64 i = leaf->first_entry_index; i < one_plus_last_entry_index; ++i) {
                auto *entry = &world->bvh->entries[i];

#if USE_HASH_TABLE
                // Make sure this triangle isn't already in the volume.
                if(triangle_table.query(entry)) continue;
#endif

                // We add a little offset to the position here so that we don't find the triangle that we are actually casting from...
                vec3 direction = world_space_position - entry->center;
                if(world->cast_ray_against_delimiters_and_root_planes(entry->center + direction * CORE_EPSILON, direction, 1.)) continue;

                // Add the triangle to the volume
                volume->add(entry->triangle);
#if USE_HASH_TABLE
                triangle_table.add(entry, true);
#endif
            }
        }
    }

#if USE_HASH_TABLE
    triangle_table.destroy();
#endif
}
