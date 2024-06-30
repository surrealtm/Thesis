#include "bvh.h"

//#define MAX_BVH_DEPTH            16
#define MAX_BVH_DEPTH             4
#define MIN_BVH_ENTRIES_TO_SPLIT  4

struct BVH_Node_Stack {
    BVH_Node *node;
    u32 depth;
};

static
void include_in_min_bounds(vec3 &bounds, const vec3 &point) {
    bounds.x = min(bounds.x, point.x);
    bounds.y = min(bounds.y, point.y);
    bounds.z = min(bounds.z, point.z);
}

static
void include_in_max_bounds(vec3 &bounds, const vec3 &point) {
    bounds.x = max(bounds.x, point.x);
    bounds.y = max(bounds.y, point.y);
    bounds.z = max(bounds.z, point.z);
}

static
void include_in_bounds(vec3 &min, vec3 &max, const Triangle &triangle) {
    include_in_min_bounds(min, triangle.p0);
    include_in_min_bounds(min, triangle.p1);
    include_in_min_bounds(min, triangle.p2);

    include_in_max_bounds(max, triangle.p0);
    include_in_max_bounds(max, triangle.p1);
    include_in_max_bounds(max, triangle.p2);
}

void BVH::create(Allocator *allocator) {
    this->allocator         = allocator;
    this->entries           = Resizable_Array<BVH_Entry>();
    this->entries.allocator = allocator;

    memset(this->root.children, 0, sizeof(this->root.children));
}

void BVH::add(Triangle triangle) {
    BVH_Entry *entry = this->entries.push();
    entry->triangle  = triangle;
    entry->center    = (entry->triangle.p0 + entry->triangle.p1 + entry->triangle.p2) / 3.;
}

void BVH::subdivide() {
    Resizable_Array<BVH_Node_Stack> stack;
    stack.allocator = this->allocator;
    stack.add({ &this->root, 1 });

    this->root.first_entry_index = 0;
    this->root.entry_count       = this->entries.count;

    while(stack.count) {
        BVH_Node_Stack head = stack.pop();

        //
        // Calculate the bounds for this node.
        //
        {
            head.node->leaf = true;
            head.node->min = vec3(MAX_F32, MAX_F32, MAX_F32);
            head.node->max = vec3(MIN_F32, MIN_F32, MIN_F32);

            s64 one_plus_last = head.node->first_entry_index + head.node->entry_count;
            for(s64 i = head.node->first_entry_index; i < one_plus_last; ++i) {
                BVH_Entry &entry = this->entries[i];
                include_in_bounds(head.node->min, head.node->max, entry.triangle);
            }
        }

        //
        // Only subdivide this node if we haven't reached the max node depth yet and this node actually contains
        // enough entries that splitting seems like a good idea.
        //
        if(head.depth == MAX_BVH_DEPTH || head.node->entry_count < MIN_BVH_ENTRIES_TO_SPLIT) continue;

        s64 split_axis; // x = 0, y = 1, z = 2
        real split_value;
        real percentage_along_split_axis;
        
        //
        // Calculate the splitting plane for this node.
        //
        {
            vec3 size = head.node->max - head.node->min;

            if(size.x > size.y && size.x > size.z) {
                split_axis = 0;
            } else if(size.y > size.z) {
                split_axis = 1;
            } else {
                split_axis = 2;
            }
            
            percentage_along_split_axis = 0.5;
            split_value = head.node->min.values[split_axis] + (head.node->max.values[split_axis] - head.node->min.values[split_axis]) * percentage_along_split_axis;
        }
        
        s64 first_right_child_index;

        //
        // Sort the entries included in this node so that the ones contained in the left
        // and right children are each continuous in the entries array.
        //
        {
            s64 left_idx = head.node->first_entry_index, right_idx = head.node->first_entry_index + head.node->entry_count - 1;
            while(left_idx <= right_idx) {
                BVH_Entry &entry = this->entries[left_idx];
                if(entry.center.values[split_axis] > split_value) {
                    // This entry is inside the right child, but is currently located in the
                    // left child's section of the entries array. We need to swap it with
                    // another entry to move it into the right subsection.
                    auto tmp = this->entries[right_idx];
                    this->entries[right_idx] = this->entries[left_idx];
                    this->entries[left_idx] = tmp;
                    --right_idx;
                } else {
                    // This entry is inside the left child, and it is already in the correct
                    // spot in the entries array.
                    ++left_idx;
                }
            }

            first_right_child_index = left_idx;
        }

        //
        // Actually subdivide the node.
        //
        {
            s64 left_count = first_right_child_index - head.node->first_entry_index;
            s64 right_count = head.node->first_entry_index + head.node->entry_count - first_right_child_index;

            if(left_count > 0) {
                head.node->children[0] = (BVH_Node *) this->allocator->allocate(sizeof(BVH_Node));
                head.node->children[0]->first_entry_index = head.node->first_entry_index;
                head.node->children[0]->entry_count = left_count;
            }

            if(right_count > 0) {
                head.node->children[1] = (BVH_Node *) this->allocator->allocate(sizeof(BVH_Node));
                head.node->children[1]->first_entry_index = first_right_child_index;
                head.node->children[1]->entry_count = right_count;
            }
        }
        
        //
        // Add the two new children to the stack.
        //
        head.node->leaf = false;
            
        for(s64 i = 0; i < 2; ++i) {
            stack.add({ head.node->children[i], head.depth + 1 });
        }
    }
    
    stack.clear();
}



#include "noise.h"

Resizable_Array<Triangle> build_sample_triangle_mesh(Allocator *allocator) {
    Simplex_Noise_2D noise;
    noise.amplitude = 30;
    
    Resizable_Array<Triangle> mesh;
    mesh.allocator = allocator;

    for(s64 x = 0; x < 50; ++x) {
        for(s64 z = 0; z < 50; ++z) {
            real y0 = noise.fractal_noise((f64) (x),     (f64) (z)).value;
            real y1 = noise.fractal_noise((f64) (x + 1), (f64) (z)).value;
            real y2 = noise.fractal_noise((f64) (x),     (f64) (z + 1)).value;
            real y3 = noise.fractal_noise((f64) (x + 1), (f64) (z + 1)).value;

            vec3 p0 = vec3((real) (x),     y0, (real) (z));
            vec3 p1 = vec3((real) (x + 1), y1, (real) (z));
            vec3 p2 = vec3((real) (x),     y2, (real) (z + 1));
            vec3 p3 = vec3((real) (x + 1), y3, (real) (z + 1));

            mesh.add({ p0, p3, p1 });
            mesh.add({ p0, p2, p3 });
        }
    }
    
    return mesh;
}
