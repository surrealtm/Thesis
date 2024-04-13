#include "core.h"
#include "timing.h"
#include "os_specific.h"

/* -------------------------------------------------- Octree -------------------------------------------------- */

void Octree::create(Allocator *allocator, v3f center, v3f dimensions) {
    this->center = center;
    this->dimensions = 0;
    memset(this->children, 0, sizeof(this->children));

    this->anchors.allocator = allocator;
    this->boundaries.allocator = allocator;
}

Octree *Octree::get_octree_for_aabb(AABB const &aabb, Allocator *allocator) {
    tmFunction();
    return this;
}



/* -------------------------------------------------- World -------------------------------------------------- */

void World::create(v3f size) {
    this->arena.create(16 * ONE_MEGABYTE);
    this->pool.create(&this->arena);
    this->allocator = this->pool.allocator();
    
    this->size = size;

    this->anchors.allocator    = &this->allocator;
    this->boundaries.allocator = &this->allocator;
    this->root.create(&this->allocator, v3f(0), this->size);
}

void World::destroy() {
    this->arena.destroy();
}

void World::add_anchor(string name, v3f position) {
    Anchor *anchor   = this->anchors.push();
    anchor->name     = copy_string(&this->allocator, name);
    anchor->position = position;
}

void World::add_boundary(string name, AABB aabb) {
    Boundary *boundary = this->boundaries.push();
    boundary->name     = copy_string(&this->allocator, name);
    boundary->aabb     = aabb;
}

void World::create_octree() {
    tmFunction();
    
    //
    // Insert all boundaris.
    //
    for(auto *boundary : this->boundaries) {
        Octree *octree = this->root.get_octree_for_aabb(boundary->aabb, &this->allocator);
        octree->boundaries.add(boundary);
    }

    //
    // Insert all anchors.
    //
    for(auto *anchor : this->anchors) {
        AABB anchor_aabb = { anchor->position, anchor->position };
        Octree *octree = this->root.get_octree_for_aabb(anchor_aabb, &this->allocator);
        octree->anchors.add(anchor);
    }
}

