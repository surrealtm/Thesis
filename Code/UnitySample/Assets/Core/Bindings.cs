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

[Flags]
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
    public u8 r, g, b, a;
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
    [DllImport("Core.dll")]
    public static extern World_Handle core_create_world(double x, double y, double z);
    [DllImport("Core.dll")]
    public static extern void core_destroy_world(World_Handle world);



#if FOUNDATION_DEVELOPER
    /* ----------------------------------------------- Testing ----------------------------------------------- */
    [DllImport("Core.dll")]
    public static extern void core_do_world_step(World_Handle world_handle, bool step_mode);

    [DllImport("Core.dll")]
    public static extern World_Handle core_do_house_test(bool step_into);
    [DllImport("Core.dll")]
    public static extern World_Handle core_do_octree_test(bool step_into);
    [DllImport("Core.dll")]
    public static extern World_Handle core_do_large_volumes_test(bool step_into);
    [DllImport("Core.dll")]
    public static extern World_Handle core_do_cutout_test(bool step_into);
    [DllImport("Core.dll")]
    public static extern World_Handle core_do_circle_test(bool step_into);
    [DllImport("Core.dll")]
    public static extern World_Handle core_do_u_shape_test(bool step_into);
    [DllImport("Core.dll")]
    public static extern World_Handle core_do_center_block_test(bool step_into);
    [DllImport("Core.dll")]
    public static extern World_Handle core_do_jobs_test(bool step_into);



    /* ---------------------------------------------- Debug Draw --------------------------------------------- */

    [DllImport("Core.dll")]
    public static extern Debug_Draw_Data core_debug_draw_world(World_Handle world_handle, Debug_Draw_Options options);
    [DllImport("Core.dll")]
    public static extern void core_free_debug_draw_data(Debug_Draw_Data_Handle data);



    /* ---------------------------------------------- Profiling ---------------------------------------------- */

    [DllImport("Core.dll")]
    public static extern void core_begin_profiling();
    [DllImport("Core.dll")]
    public static extern void core_stop_profiling();
    [DllImport("Core.dll")]
    public static extern void core_print_profiling(bool include_timeline);
    [DllImport("Core.dll")]
    public static extern Timing_Data core_get_profiling_data();
    [DllImport("Core.dll")]
    public static extern void core_free_profiling_data(Timing_Data_Handle data);

    [DllImport("Core.dll")]
    public static extern Memory_Information core_get_memory_information(World_Handle world_handle);
    [DllImport("Core.dll")]
    public static extern void core_free_memory_information(Memory_Information_Handle info);

    [DllImport("Core.dll")]
    public static extern void core_enable_memory_tracking();
    [DllImport("Core.dll")]
    public static extern void core_disable_memory_tracking();
#endif
};



/* -------------------------------------------- Debugging Wrapper -------------------------------------------- */

public unsafe class Core_Helpers {
    private static bool draw_data_setup = false;
    private static List<GameObject> dbg_draw_objects = new List<GameObject>();

    private static Color color(u8 r, u8 g, u8 b) {
        return new Color(r / 255.0f, g / 255.0f, b / 255.0f, 1);
    }

    private static Color color(u8 r, u8 g, u8 b, u8 a) {
        return new Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }

    private static Vector3 vector3(v3f v) {
        return new Vector3(v.x, v.y, v.z);
    }

    private static Quaternion quat(quatf q) {
        return new Quaternion(q.x, q.y, q.z, q.w);
    }
    
    public static void draw_primitive(PrimitiveType primitive_type, Vector3 position, Quaternion rotation, Vector3 size, Color color) {
        GameObject _object = GameObject.CreatePrimitive(primitive_type);
        _object.name       = "Dbg" + primitive_type;
        _object.transform.position   = position;
        _object.transform.rotation   = rotation;
        _object.transform.localScale = size * 2;
        
        MeshRenderer mesh_renderer = _object.GetComponent<MeshRenderer>();
        mesh_renderer.material.color = color;

        dbg_draw_objects.Add(_object);
    }

    public static void draw_text(Vector3 position, string text, Color color) {
        GameObject _object = new GameObject();
        _object.name       = "DbgText";
        _object.transform.position   = position;
        _object.transform.localScale = new Vector3(-0.03f, 0.03f, 0.03f); // Make this very small and the font size really large for higher quality text. The X scale is negative to flip the text, making it readable again after the LookAt when facing the camera.

        TextMesh text_mesh = _object.AddComponent<TextMesh>();
        text_mesh.text     = text;
        text_mesh.fontSize = 100;
        text_mesh.anchor   = TextAnchor.MiddleCenter;
        text_mesh.font     = (Font)Resources.GetBuiltinResource(typeof(Font), "Arial.ttf");
        text_mesh.GetComponent<Renderer>().material.color = color;

        dbg_draw_objects.Add(_object);
    }

    public static void debug_draw_world(World_Handle world_handle, Debug_Draw_Options options, bool clear) {
        if(clear) clear_debug_draw();

        Debug_Draw_Data draw_data = Core_Bindings.core_debug_draw_world(world_handle, options);

        if(draw_data.triangle_count > 0) {
            List<Vector3> triangle_vertices = new List<Vector3>();
            List<Color> triangle_colors = new List<Color>();
            List<int> triangle_indices = new List<int>();

            for(int i = 0; i < draw_data.triangle_count; ++i) {
                Color _color = color(draw_data.triangles[i].r, draw_data.triangles[i].g, draw_data.triangles[i].b, draw_data.triangles[i].a);
                triangle_vertices.Add(vector3(draw_data.triangles[i].p0));
                triangle_vertices.Add(vector3(draw_data.triangles[i].p1));
                triangle_vertices.Add(vector3(draw_data.triangles[i].p2));
                triangle_colors.Add(_color);
                triangle_colors.Add(_color);
                triangle_colors.Add(_color);
                triangle_indices.Add(i * 3 + 0);
                triangle_indices.Add(i * 3 + 1);
                triangle_indices.Add(i * 3 + 2);
            }

            Mesh triangle_mesh = new Mesh();
            triangle_mesh.name = "DbgTriangleMesh";
            triangle_mesh.SetVertices(triangle_vertices);
            triangle_mesh.SetColors(triangle_colors);
            triangle_mesh.SetTriangles(triangle_indices, 0);

            GameObject _object = new GameObject();
            _object.name = "DbgTriangles";

            MeshFilter mesh_filter = _object.AddComponent<MeshFilter>();
            mesh_filter.mesh = triangle_mesh;

            MeshRenderer mesh_renderer = _object.AddComponent<MeshRenderer>();
            mesh_renderer.material = Resources.Load<Material>("FlatMaterial");

            dbg_draw_objects.Add(_object);
        }

        if(draw_data.line_count > 0) {
            List<Vector3> line_vertices = new List<Vector3>();
            List<Color> line_colors = new List<Color>();
            List<int> line_indices = new List<int>();

            for(int i = 0; i < draw_data.line_count; ++i) {
                Color _color = color(draw_data.lines[i].r, draw_data.lines[i].g, draw_data.lines[i].b);
                line_vertices.Add(vector3(draw_data.lines[i].p0));
                line_vertices.Add(vector3(draw_data.lines[i].p1));
                line_colors.Add(_color);
                line_colors.Add(_color);
                line_indices.Add(i * 2 + 0);
                line_indices.Add(i * 2 + 1);                
            }
            
            Mesh line_mesh = new Mesh();
            line_mesh.name = "DbgLineMesh";
            line_mesh.SetVertices(line_vertices);
            line_mesh.SetColors(line_colors);
            line_mesh.SetIndices(line_indices, MeshTopology.Lines, 0);

            GameObject _object = new GameObject();
            _object.name = "DbgLines";

            MeshFilter mesh_filter = _object.AddComponent<MeshFilter>();
            mesh_filter.mesh = line_mesh;

            MeshRenderer mesh_renderer = _object.AddComponent<MeshRenderer>();
            mesh_renderer.material = Resources.Load<Material>("WireframeMaterial");

            dbg_draw_objects.Add(_object);            
        }
        
        for(s64 i = 0; i < draw_data.text_count; ++i) {
            draw_text(vector3(draw_data.texts[i].position), draw_data.texts[i].text.cs(), color(draw_data.texts[i].r, draw_data.texts[i].g, draw_data.texts[i].b));
        }

        for(s64 i = 0; i < draw_data.cuboid_count; ++i) {
            draw_primitive(PrimitiveType.Cube, vector3(draw_data.cuboids[i].position), quat(draw_data.cuboids[i].rotation), vector3(draw_data.cuboids[i].size), color(draw_data.cuboids[i].r, draw_data.cuboids[i].g, draw_data.cuboids[i].b));
        }

        for(s64 i = 0; i < draw_data.sphere_count; ++i) {
            draw_primitive(PrimitiveType.Sphere, vector3(draw_data.spheres[i].position), new Quaternion(0, 0, 0, 1), new Vector3(draw_data.spheres[i].radius, draw_data.spheres[i].radius, draw_data.spheres[i].radius), color(draw_data.spheres[i].r, draw_data.spheres[i].g, draw_data.spheres[i].b));
        }
        
        Core_Bindings.core_free_debug_draw_data(new Debug_Draw_Data_Handle((IntPtr) (&draw_data)));
    }

    public static void make_texts_face_the_camera(Camera camera) {
        foreach(GameObject _object in dbg_draw_objects) {
            if(_object.GetComponent<TextMesh>() != null) {
                _object.transform.LookAt(camera.transform.position);
            }
        }
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
