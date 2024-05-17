#define FOUNDATION_DEVELOPER

using System;
using System.Runtime.InteropServices;

using UnityEngine;

// nocheckin: Docs
using World_Handle              = System.IntPtr;
using Debug_Draw_Data_Handle    = System.IntPtr;
using Timing_Data_Handle        = System.IntPtr;
using Memory_Information_Handle = System.IntPtr;

using f32 = System.Single;
using f64 = System.Double;
using u8  = System.Byte;
using s64 = System.Int64;

public enum Debug_Draw_Options : uint {
    DEBUG_DRAW_Nothing             = 0x0,
    DEBUG_DRAW_Octree              = 0x1,
    DEBUG_DRAW_Anchors             = 0x2,
    DEBUG_DRAW_Boundaries          = 0x4,
    DEBUG_DRAW_Clipping_Faces      = 0x8,
    DEBUG_DRAW_Clipping_Wireframes = 0x10,
    DEBUG_DRAW_Volume_Faces        = 0x20,
    DEBUG_DRAW_Volume_Wireframes   = 0x40,
    DEBUG_DRAW_Labels              = 0x1000,
    DEBUG_DRAW_Normals             = 0x2000,
    DEBUG_DRAW_Axis_Gizmo          = 0x4000,
    DEBUG_DRAW_Root_Planes         = 0x8000,
    DEBUG_DRAW_Everything          = 0xffffffff,
}

public unsafe struct v3f {
    public f32 x, y, z;
}

public unsafe struct quatf {
    public f32 x, y, z, w;
}

public unsafe struct _string {
    public s64 count;
    public IntPtr data; // char *

    public String cs() { return System.Runtime.InteropServices.Marshal.PtrToStringAnsi(this.data); }
}

public unsafe struct Debug_Draw_Line {
    public v3f p0, p1;
    public f32 thickness;
    public u8 r, g, b;
}

public unsafe struct Debug_Draw_Triangle {
    public v3f p0, p1, p2;
    public f32 thickness;
    public u8 r, g, b;
}

public unsafe struct Debug_Draw_Text {
    public v3f position;
    public _string text;
    public u8 r, g, b;
}

public unsafe struct Debug_Draw_Cuboid {
    public v3f position;
    public v3f size;
    public quatf rotation;
    public u8 r, g, b;
}

public unsafe struct Debug_Draw_Sphere {
    public v3f position;
    public f32 radius;
    public u8 r, g, b;
}

public unsafe struct Debug_Draw_Data {
    public Debug_Draw_Line* lines;
    public s64 line_count;

    public Debug_Draw_Triangle* triangles;
    public s64 triangle_count;

    public Debug_Draw_Text* texts;
    public s64 text_count;

    public Debug_Draw_Cuboid* cuboids;
    public s64 cuboid_count;

    public Debug_Draw_Sphere* spheres;
    public s64 sphere_count;
}

public unsafe struct Timing_Timeline_Entry {
    public _string name;
    public s64 start_in_nanoseconds, end_in_nanoseconds;
    public s64 depth;
    public u8 r, g, b;
}

public unsafe struct Timing_Summary_Entry {
    public _string name;
    public s64 inclusive_time_in_nanoseconds;
    public s64 exclusive_time_in_nanoseconds;
    public s64 count;
}

public unsafe struct Timing_Data {
    public Timing_Timeline_Entry** timelines;
    public s64* timelines_entry_count;
    public s64 timelines_count;

    public Timing_Summary_Entry* summary;
    public s64 summary_count;
    
    public s64 total_time_in_nanoseconds;
    public s64 total_overhead_time_in_nanoseconds;
    public s64 total_overhead_space_in_bytes;
}

public unsafe struct Memory_Allocator_Information {
    public _string name;
    public s64 allocation_count;
    public s64 deallocation_count;
    public s64 working_set_size;
    public s64 peak_working_set_size;
}

public unsafe struct Memory_Information {
    public s64 os_working_set_size;
    public Memory_Allocator_Information* allocators;
    public s64 allocator_count;
}

public unsafe struct World {}; // Opaque type

public class Core_Bindings {
    /* --------------------------------------------- General API --------------------------------------------- */
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern World_Handle core_create_world(double x, double y, double z);
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern void core_destroy_world(World_Handle world);


    
#if FOUNDATION_DEVELOPER
    /* ----------------------------------------------- Testing ----------------------------------------------- */
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern void core_do_world_step(World_Handle world_handle, bool step_mode);

    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern World_Handle core_do_house_test(bool step_into);
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern World_Handle core_do_octree_test(bool step_into);
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern World_Handle core_do_large_volumes_test(bool step_into);
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern World_Handle core_do_cutout_test(bool step_into);
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern World_Handle core_do_circle_test(bool step_into);
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern World_Handle core_do_u_shape_test(bool step_into);
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern World_Handle core_do_center_block_test(bool step_into);
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern World_Handle core_do_jobs_test(bool step_into);

    

    /* ---------------------------------------------- Debug Draw --------------------------------------------- */

    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern Debug_Draw_Data core_debug_draw_world(World_Handle world_handle, Debug_Draw_Options options);
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern void core_free_debug_draw_data(Debug_Draw_Data_Handle data);
    
    

    /* ---------------------------------------------- Profiling ---------------------------------------------- */
    
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern void core_begin_profiling();
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern void core_stop_profiling();
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern void core_print_profiling(bool include_timeline);    
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern Timing_Data core_get_profiling_data();
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern void core_free_profiling_data(Timing_Data_Handle data);

    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern Memory_Information core_get_memory_information(World_Handle world_handle);
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern void core_free_memory_information(Memory_Information_Handle info);

    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern void core_enable_memory_tracking();
    [DllImport("../Core/x64/ReleaseDll/Core.dll")]
    public static extern void core_disable_memory_tracking();
#endif
};

public unsafe class Core_Helpers {
    public static void debug_draw_world(World_Handle world_handle) {
        
    }

    public static f64 to_seconds(s64 nanoseconds) {
        return (f64) nanoseconds / 1000000000.0f;
    }
    
    public static void print_profiling(bool include_timeline) {
        Timing_Data timing_data = Core_Bindings.core_get_profiling_data();
        
        Debug.Log("------------------------------ Summary: ------------------------------");
        
        for(s64 i = 0; i < timing_data.summary_count; ++i) {
            string name = timing_data.summary[i].name.cs();
            Debug.Log(name +  " | Exclusive: " + to_seconds(timing_data.summary[i].exclusive_time_in_nanoseconds) + "s | " + timing_data.summary[i].count + "x");
        }

        Debug.Log("------------------------------ Summary: ------------------------------");

        Core_Bindings.core_free_profiling_data((IntPtr) (&timing_data));
    }
};
