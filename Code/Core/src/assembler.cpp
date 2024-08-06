#include "assembler.h"
#include "world.h"
#include "floodfill.h"
#include "bvh.h"

#include "timing.h"
#include "hash_table.h"
#include "art.h"

struct Assembler {
    World *world;
    Flood_Fill *ff;
    Resizable_Array<Triangle> volume;

#if USE_HASH_TABLE_IN_ASSEMBLER
    Probed_Hash_Table<BVH_Entry *, b8> triangle_table;
#endif

#if USE_ART_IN_ASSEMBLER
    Adaptive_Radix_Tree<BVH_Entry *> triangle_art;
#endif
};

static
void assemble_triangle_against_cell(Assembler *assembler, vec3 cell_world_space_position, BVH_Entry *entry) {
#if USE_HASH_TABLE_IN_ASSEMBLER
    // Make sure this triangle isn't already in the volume.
    if(assembler->triangle_table.query(entry)) return;
#endif

#if USE_ART_IN_ASSEMBLER
    if(assembler->triangle_art.query(entry)) return;
#endif

    // We add a little offset to the position here so that we don't find the triangle that we are
    // actually casting from...
    vec3 direction = cell_world_space_position - entry->center;
    if(assembler->world->cast_ray_against_delimiters_and_root_planes(entry->center + direction * CORE_EPSILON, direction, 1.)) return;

    // Add the triangle to the volume
    assembler->volume.add(entry->triangle);
                
#if USE_HASH_TABLE_IN_ASSEMBLER
    assembler->triangle_table.add(entry, true);
#endif

#if USE_ART_IN_ASSEMBLER
    assembler->triangle_art.add(entry);
#endif
}

static
void assemble_triangle(Assembler *assembler, BVH_Entry *entry) {
    for(Cell *cell : assembler->ff->flooded_cells) {
        vec3 cell_world_space_position = get_cell_world_space_center(assembler->ff, cell);
        assemble_triangle_against_cell(assembler, cell_world_space_position, entry);
    }
}

Resizable_Array<Triangle> assemble(World *world, Flood_Fill *ff, Allocator *allocator) {
    tmFunction(TM_ASSEMBLING_COLOR);

    Assembler assembler;
    assembler.world = world;
    assembler.ff = ff;
    
    // Because this procedure is running simultaneously in several threads, we would need to
    // make the world's allocator thread-safe using a lock. That seems like a bad idea however,
    // so instead we allocate the volume on the thread-local temp allocator, and then copy it
    // once over to the world allocator when it is done (by using a lock).
    assembler.volume.allocator = allocator;

    // When assembling the triangles that make up a volume, we want to make sure that we
    // don't have duplicates in that volume. This could happen because a triangle might have
    // line-of-sight to many flood-filling cells, in which case it would be added multiple
    // times.
    // Therefore, we use either a hash table or an art, with the pointers to the triangles 
    // as keys to check for duplicates.

#if USE_HASH_TABLE_IN_ASSEMBLER
    auto hash = [](BVH_Entry *const &key) -> u64 { return murmur_64a((u64) key); };
    assembler.triangle_table.allocator = &temp;
    assembler.triangle_table.create(assembler.world->bvh->entries.count, hash, [](BVH_Entry *const &lhs, BVH_Entry *const &rhs) -> b8 { return lhs == rhs; });
#endif

#if USE_ART_IN_ASSEMBLER
    assembler.triangle_art.allocator = &temp;
    assembler.triangle_art.create();
#endif

    for(auto &root_entry : assembler.world->root_bvh_entries) {
        assemble_triangle(&assembler, &root_entry);
    }
        
    for(s64 i = 0; i < assembler.world->bvh.entries.count; ++i) {
        assemble_triangle(&assembler, &assembler.world->bvh.entries[i]);
    }

#if USE_HASH_TABLE_IN_ASSEMBLER && FOUNDATION_DEVELOPER
    printf("  Completed assembly with %" PRId64 " collisions, %f load factor for %" PRId64 " entries.\n", assembler.triangle_table.stats.collisions, assembler.triangle_table.stats.load_factor, assembler.triangle_table.count);
#endif

    return assembler.volume;
}
