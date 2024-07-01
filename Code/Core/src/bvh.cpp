#include "bvh.h"

#define MAX_BVH_DEPTH            10
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

void BVH_Stats::print_to_stdout() {
    printf("================== BVH ==================\n");
    printf("  Max Leaf Depth:    %" PRId64 "\n", this->max_leaf_depth);
    printf("  Min Leaf Depth:    %" PRId64 "\n", this->min_leaf_depth);
    printf("  Min Leaf Entries:  %" PRId64 "\n", this->min_entries_in_leaf);
    printf("  Max Leaf Entries:  %" PRId64 "\n", this->max_entries_in_leaf);
    printf("  Total Node Count:  %" PRId64 "\n", this->total_node_count);
    printf("  Total Entry Count: %" PRId64 "\n", this->total_entry_count);
    printf("  AVG Fill Rate:     %f\n", this->total_entry_count / (f32) this->total_node_count);
    printf("================== BVH ==================\n");

}

void BVH_Node::update_stats(BVH_Stats *stats, s64 depth) {
    ++stats->total_node_count;

    if(this->leaf) {
        stats->max_leaf_depth      = max(stats->max_leaf_depth, depth);
        stats->min_leaf_depth      = min(stats->min_leaf_depth, depth);
        stats->max_entries_in_leaf = max(stats->max_entries_in_leaf, this->entry_count);
        stats->min_entries_in_leaf = min(stats->min_entries_in_leaf, this->entry_count);
    } else {
        if(this->children[0]) this->children[0]->update_stats(stats, depth + 1);
        if(this->children[1]) this->children[1]->update_stats(stats, depth + 1);
    }
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
        // Actually subdivide the node and add the children to the queue.
        //
        {
            head.node->leaf = false;
    
            s64 left_count = first_right_child_index - head.node->first_entry_index;
            s64 right_count = head.node->first_entry_index + head.node->entry_count - first_right_child_index;

            if(left_count > 0) {
                head.node->children[0] = (BVH_Node *) this->allocator->allocate(sizeof(BVH_Node));
                head.node->children[0]->first_entry_index = head.node->first_entry_index;
                head.node->children[0]->entry_count = left_count;
                stack.add({ head.node->children[0], head.depth + 1 });
            }

            if(right_count > 0) {
                head.node->children[1] = (BVH_Node *) this->allocator->allocate(sizeof(BVH_Node));
                head.node->children[1]->first_entry_index = first_right_child_index;
                head.node->children[1]->entry_count = right_count;
                stack.add({ head.node->children[1], head.depth + 1 });
            }
        }
    }
    
    stack.clear();
}

BVH_Stats BVH::stats() {
    BVH_Stats stats;
    stats.max_leaf_depth      = 0;
    stats.min_leaf_depth      = MAX_S64;
    stats.max_entries_in_leaf = 0;
    stats.min_entries_in_leaf = MAX_S64;
    stats.total_node_count    = 0;
    stats.total_entry_count   = this->entries.count;
    
    this->root.update_stats(&stats, 0);
    
    return stats;
}



#include "string_type.h"
#include "os_specific.h"
#include "timing.h"

static
real read_real(string *line) {
    s64 end = search_string(*line, ',');
    real result = (real) string_to_double(substring_view(*line, 0, end), null);
    
    if(end + 1 < line->count) {
        *line = substring_view(*line, end + 1, line->count);
    } else {
        *line = ""_s;
    }

    return result;
}

static
vec3 read_vec3(string *line) {
    vec3 result;
    result.x = read_real(line);
    result.y = read_real(line);
    result.z = read_real(line);
    return result;
}

Resizable_Array<Triangle> build_sample_triangle_mesh(Allocator *allocator) {
    tmFunction(TM_SYSTEM_COLOR);

    Resizable_Array<Triangle> result;
    result.allocator = allocator;

    string file_path = "Core/data/triangle_data.txt"_s;
    string file_content = os_read_file(Default_Allocator, file_path);
    if(!file_content.count) {
        string working_directory = os_get_working_directory();
        printf("The triangle data file '%.*s' does not exist!\n", (u32) file_path.count, file_path.data);
        printf("Working directory: '%.*s'\n", (u32) working_directory.count, working_directory.data);
        deallocate_string(Default_Allocator, &working_directory);
        return result;
    }
    
    string original_file_content = file_content;
    defer { os_free_file_content(Default_Allocator, &original_file_content); };

    while(file_content.count) {
        string line = read_next_line(&file_content);

        Triangle *triangle = result.push();
        triangle->p0 = read_vec3(&line);
        triangle->p1 = read_vec3(&line);
        triangle->p2 = read_vec3(&line);
    }

    return result;
}
