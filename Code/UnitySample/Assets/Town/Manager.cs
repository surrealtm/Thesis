using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using UnityEngine;
using UnityEditor;

public class Manager : MonoBehaviour {
    [CustomEditor(typeof(Manager))]
    public class Inspector : Editor {
        public override void OnInspectorGUI() {
            DrawDefaultInspector();

            if(GUILayout.Button("Create")) {
                ((Manager) this.target).create_world();
            }

            if(GUILayout.Button("Destroy")) {
                ((Manager) this.target).destroy_world();                
            }

            if(GUILayout.Button("Serialize")) {
                ((Manager) this.target).serialize_world();                
            }
        }
    }

    [SerializeField] GameObject query_object;

    public Debug_Draw_Options debug_draw_options = Debug_Draw_Options.Delimiter_Wireframes;
    public double cell_world_space_size = 2.0;
    private World_Handle world_handle;
    private Anchor current_residing_anchor = null;

    void destroy_world() {
        if(this.world_handle.valid()) Core_Bindings.core_destroy_world(world_handle);
        Core_Helpers.clear_debug_draw();
        this.world_handle.invalidate();
        UnityEngine.Debug.Log("Destroyed world.");
    }

    void create_world() {
        this.destroy_world();

        Stopwatch sw = new Stopwatch();
        sw.Start();
        this.world_handle = Core_Helpers.create_world_from_scene_and_print_profiling(this.cell_world_space_size);
        sw.Stop();
        UnityEngine.Debug.Log("Created world (" + sw.Elapsed.Seconds + "s).");

        Core_Helpers.debug_draw_world(this.world_handle, this.debug_draw_options, true);
    }

    void serialize_world() {
        Core_Helpers.serialize_world_setup_code("C:/source/Thesis/Code/Core/src/serialized_setup.cpp", this.cell_world_space_size);
    }

    Anchor query_world() {
        Stopwatch sw = new Stopwatch();
        sw.Start();
        Anchor anchor = Core_Helpers.query_world(this.world_handle, this.query_object.transform.position);
        sw.Stop();
        UnityEngine.Debug.Log("Queried world (" + sw.Elapsed.Ticks / (float) TimeSpan.TicksPerMillisecond + "ms).");
        return anchor;    
    }
    
    void Start() {
        this.world_handle.invalidate();
        this.create_world();
    }

    void Update() {
        this.current_residing_anchor = null;

        if(this.query_object != null) {
            this.current_residing_anchor = this.query_world();
        }
    }

    void OnGUI() {
        Rect rect = new Rect(10, 10, 256, 32);
        if(this.current_residing_anchor != null) {
            GUI.Label(rect, "Current Anchor: " + this.current_residing_anchor.name);
        } else {
            GUI.Label(rect, "Outside any anchor");
        }
    }

    void Destroy() {
        this.destroy_world();
    }
}
