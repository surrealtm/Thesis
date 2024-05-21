#define FOUNDATION_DEVELOPER

using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

using UnityEngine;

using f32 = System.Single;
using f64 = System.Double;
using u8  = System.Byte;
using s64 = System.Int64;



/* ----------------------------------------------- Opaque Types ----------------------------------------------- */

public struct World {}
public struct World_Handle { System.IntPtr __handle; public World_Handle(IntPtr ptr) { __handle = ptr; } }
public struct Debug_Draw_Data_Handle { System.IntPtr __handle; public Debug_Draw_Data_Handle(IntPtr ptr) { __handle = ptr; } }
public struct Timing_Data_Handle { System.IntPtr __handle; public Timing_Data_Handle(IntPtr ptr) { __handle = ptr; } }
public struct Memory_Information_Handle { System.IntPtr __handle; public Memory_Information_Handle(IntPtr ptr) { __handle = ptr; } }




/* ------------------------------------------ Debugging Information ------------------------------------------ */

public enum Debug_Draw_Options : uint {
    Nothing             = 0x0,
    Octree              = 0x1,
    Anchors             = 0x2,
    Boundaries          = 0x4,
    Clipping_Faces      = 0x8,
    Clipping_Wireframes = 0x10,
    Volume_Faces        = 0x20,
    Volume_Wireframes   = 0x40,
    Labels              = 0x1000,
    Normals             = 0x2000,
    Axis_Gizmo          = 0x4000,
    Root_Planes         = 0x8000,
    Everything          = 0xffffffff,
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



/* ------------------------------------------------- Bindings ------------------------------------------------- */

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



/* -------------------------------------------- Debugging Wrapper -------------------------------------------- */

public unsafe class Core_Helpers {
    private static Material material;
    private static Mesh sphere_mesh;
    private static Mesh cuboid_mesh;
    private static bool draw_data_setup = false;
    private static List<GameObject> dbg_draw_objects = new List<GameObject>();

    private static void setup_draw_helpers() {
        if(draw_data_setup) return;

        //
        // Set up the cuboid mesh.
        //
        {
            Vector3[] vertices = new Vector3[] {
                new Vector3(-1.0f, -1.0f, -1.0f), new Vector3(-1.0f, -1.0f, 1.0f),  new Vector3(-1.0f, 1.0f, 1.0f),   // Left Side
                new Vector3(-1.0f, -1.0f, -1.0f), new Vector3(-1.0f, 1.0f, 1.0f),   new Vector3(-1.0f, 1.0f, -1.0f),  // Left Side
                new Vector3(1.0f, 1.0f, -1.0f),   new Vector3(-1.0f, -1.0f, -1.0f), new Vector3(-1.0f, 1.0f, -1.0f),  // Back Side
                new Vector3(1.0f, 1.0f, -1.0f),   new Vector3(1.0f, -1.0f, -1.0f),  new Vector3(-1.0f, -1.0f, -1.0f), // Back Side
                new Vector3(1.0f, -1.0f, 1.0f),   new Vector3(-1.0f, -1.0f, -1.0f), new Vector3(1.0f, -1.0f, -1.0f),  // Bottom Side
                new Vector3(1.0f, -1.0f, 1.0f),   new Vector3(-1.0f, -1.0f, 1.0f),  new Vector3(-1.0f, -1.0f, -1.0f), // Bottom Side
                new Vector3(-1.0f, 1.0f, 1.0f),   new Vector3(-1.0f, -1.0f, 1.0f),  new Vector3(1.0f, -1.0f, 1.0f),   // Front Side
                new Vector3(1.0f, 1.0f, 1.0f),    new Vector3(-1.0f, 1.0f, 1.0f),   new Vector3(1.0f, -1.0f, 1.0f),   // Front Side
                new Vector3(1.0f, 1.0f, 1.0f),    new Vector3(1.0f, -1.0f, -1.0f),  new Vector3(1.0f, 1.0f, -1.0f),   // Right Side
                new Vector3(1.0f, -1.0f, -1.0f),  new Vector3(1.0f, 1.0f, 1.0f),    new Vector3(1.0f, -1.0f, 1.0f),   // Right Side
                new Vector3(1.0f, 1.0f, 1.0f),    new Vector3(1.0f, 1.0f, -1.0f),   new Vector3(-1.0f, 1.0f, -1.0f),  // Top Side
                new Vector3(1.0f, 1.0f, 1.0f),    new Vector3(-1.0f, 1.0f, -1.0f),  new Vector3(-1.0f, 1.0f, 1.0f),   // Top Side
            };

            Vector3[] normals = new Vector3[] {
                new Vector3(-1.0f, 0.0f, 0.0f), new Vector3(-1.0f, 0.0f, 0.0f), new Vector3(-1.0f, 0.0f, 0.0f), // Left Side
                new Vector3(-1.0f, 0.0f, 0.0f), new Vector3(-1.0f, 0.0f, 0.0f), new Vector3(-1.0f, 0.0f, 0.0f), // Left Side
                new Vector3(0.0f, 0.0f, -1.0f), new Vector3(0.0f, 0.0f, -1.0f), new Vector3(0.0f, 0.0f, -1.0f), // Back Side
                new Vector3(0.0f, 0.0f, -1.0f), new Vector3(0.0f, 0.0f, -1.0f), new Vector3(0.0f, 0.0f, -1.0f), // Back Side
                new Vector3(0.0f, -1.0f, 0.0f), new Vector3(0.0f, -1.0f, 0.0f), new Vector3(0.0f, -1.0f, 0.0f), // Bottom Side
                new Vector3(0.0f, -1.0f, 0.0f), new Vector3(0.0f, -1.0f, 0.0f), new Vector3(0.0f, -1.0f, 0.0f), // Bottom Side
                new Vector3(0.0f, 0.0f, 1.0f),  new Vector3(0.0f, 0.0f, 1.0f),  new Vector3(0.0f, 0.0f, 1.0f),  // Front Side
                new Vector3(0.0f, 0.0f, 1.0f),  new Vector3(0.0f, 0.0f, 1.0f),  new Vector3(0.0f, 0.0f, 1.0f),  // Front Side
                new Vector3(1.0f, 0.0f, 0.0f),  new Vector3(1.0f, 0.0f, 0.0f),  new Vector3(1.0f, 0.0f, 0.0f),  // Right Side
                new Vector3(1.0f, 0.0f, 0.0f),  new Vector3(1.0f, 0.0f, 0.0f),  new Vector3(1.0f, 0.0f, 0.0f),  // Right Side
                new Vector3(0.0f, 1.0f, 0.0f),  new Vector3(0.0f, 1.0f, 0.0f),  new Vector3(0.0f, 1.0f, 0.0f),  // Top Side
                new Vector3(0.0f, 1.0f, 0.0f),  new Vector3(0.0f, 1.0f, 0.0f),  new Vector3(0.0f, 1.0f, 0.0f),  // Top Side
            };

            int[] indices = new int[] {
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,
                12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
            };

            cuboid_mesh = new Mesh();
            cuboid_mesh.name = "CoreCuboid";
            cuboid_mesh.SetVertices(vertices);
            cuboid_mesh.SetNormals(normals);
            cuboid_mesh.SetTriangles(indices, 0);
        }

        //
        // Set up the sphere mesh.
        //
        {

        }

        //
        // Set up the material.
        //
        {
            material = new Material(Shader.Find("Standard"));
        }

        draw_data_setup = true;
    }

    public static void draw_cuboid(Vector3 position, Quaternion rotation, Vector3 size, Color color) {
        GameObject _object = new GameObject();
        _object.name       = "DbgCuboid";
        _object.transform.position   = position;
        _object.transform.rotation   = rotation;
        _object.transform.localScale = size;

        MeshFilter mesh_filter = _object.AddComponent<MeshFilter>();
        mesh_filter.mesh = cuboid_mesh;
        
        MeshRenderer mesh_renderer = _object.AddComponent<MeshRenderer>();
        mesh_renderer.sharedMaterial = material;

        dbg_draw_objects.Add(_object);
    }

    public static void debug_draw_world(World_Handle world_handle, Debug_Draw_Options options) {
        setup_draw_helpers();

        Debug_Draw_Data draw_data = Core_Bindings.core_debug_draw_world(world_handle, options);

        for(s64 i = 0; i < draw_data.cuboid_count; ++i) {
            draw_cuboid(new Vector3(draw_data.cuboids[i].position.x, draw_data.cuboids[i].position.y, draw_data.cuboids[i].position.z), new Quaternion(draw_data.cuboids[i].rotation.x, draw_data.cuboids[i].rotation.y, draw_data.cuboids[i].rotation.z, draw_data.cuboids[i].rotation.w), new Vector3(draw_data.cuboids[i].size.x, draw_data.cuboids[i].size.y, draw_data.cuboids[i].size.z), new Color(draw_data.cuboids[i].r, draw_data.cuboids[i].g, draw_data.cuboids[i].b, 255));
        }
        
        Core_Bindings.core_free_debug_draw_data(new Debug_Draw_Data_Handle((IntPtr) (&draw_data)));
    }

    public static void clear_debug_draw() {
        foreach(GameObject obj in dbg_draw_objects) {
            GameObject.Destroy(obj);
        }

        dbg_draw_objects.Clear();
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

        Core_Bindings.core_free_profiling_data(new Timing_Data_Handle((IntPtr) (&timing_data)));
    }
};
