#include "assembler.h"
#include "world.h"
#include "floodfill.h"
#include "bvh.h"

#include "timing.h"
#include "hash_table.h"
#include "art.h"

Resizable_Array<Triangle> assemble(World *world, Flood_Fill *ff, Allocator *allocator) {
    tmFunction(TM_ASSEMBLING_COLOR);

    // Because this procedure is running simultaneously in several threads, we would need to
    // make the world's allocator thread-safe using a lock. That seems like a bad idea however,
    // so instead we allocate the volume on the thread-local temp allocator, and then copy it
    // once over to the world allocator when it is done (by using a lock).
    Resizable_Array<Triangle> volume;
    volume.allocator = allocator;

    // When assembling the triangles that make up a volume, we want to make sure that we
    // don't have duplicates in that volume. This could happen because a triangle might have
    // line-of-sight to many flood-filling cells, in which case it would be added multiple
    // times.
    // Therefore, we use either a hash table or an art, with the pointers to the triangles 
    // as keys to check for duplicates.

#if USE_HASH_TABLE_IN_ASSEMBLER
    auto hash = [](BVH_Entry *const &key) -> u64 { return murmur_64a((u64) key); };

    Probed_Hash_Table<BVH_Entry *, b8> triangle_table;
    triangle_table.allocator = &temp;
    triangle_table.create(world->bvh->entries.count, hash, [](BVH_Entry *const &lhs, BVH_Entry *const &rhs) -> b8 { return lhs == rhs; });
#endif

#if USE_ART_IN_ASSEMBLER
    Adaptive_Radix_Tree<BVH_Entry *> triangle_art;
    triangle_art.allocator = &temp;
    triangle_art.create();
#endif
    
    for(Cell *cell : ff->flooded_cells) {
        vec3 world_space_position = get_cell_world_space_center(ff, cell);
        auto leafs = world->bvh->find_leafs_at_position(&temp, world_space_position);
        
        for(auto *leaf : leafs) {
            s64 one_plus_last_entry_index = leaf->first_entry_index + leaf->entry_count;
            for(s64 i = leaf->first_entry_index; i < one_plus_last_entry_index; ++i) {
                auto *entry = &world->bvh->entries[i];

#if USE_HASH_TABLE_IN_ASSEMBLER
                // Make sure this triangle isn't already in the volume.
                if(triangle_table.query(entry)) continue;
#endif

#if USE_ART_IN_ASSEMBLER
                if(triangle_art.query(entry)) continue;
#endif

                // We add a little offset to the position here so that we don't find the triangle that we are
                // actually casting from...
                vec3 direction = world_space_position - entry->center;
                if(world->cast_ray_against_delimiters_and_root_planes(entry->center + direction * CORE_EPSILON, direction, 1.)) continue;

                // Add the triangle to the volume
                volume.add(entry->triangle);
                
#if USE_HASH_TABLE_IN_ASSEMBLER
                triangle_table.add(entry, true);
#endif

#if USE_ART_IN_ASSEMBLER
                triangle_art.add(entry);
#endif
            }
        }
    }

#if USE_HASH_TABLE_IN_ASSEMBLER && FOUNDATION_DEVELOPER
    printf("  Completed assembly with %" PRId64 " collisions, %f load factor for %" PRId64 " entries.\n", triangle_table.stats.collisions, triangle_table.stats.load_factor, triangle_table.count);
#endif

    return volume;
}
