using System.Collections;
using System.Collections.Generic;
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
        }
    }

    public Debug_Draw_Options debug_draw_options = Debug_Draw_Options.Delimiter_Wireframes;
    public double cell_world_space_size = 2.0;
    private World_Handle world_handle;

    void destroy_world() {
        if(this.world_handle.valid()) Core_Bindings.core_destroy_world(world_handle);
        Core_Helpers.clear_debug_draw();
        this.world_handle.invalidate();
        Debug.Log("Destroyed world.");
    }

    void create_world() {
        this.destroy_world();

        Core_Bindings.core_begin_profiling();
        this.world_handle = Core_Helpers.create_world_from_scene(cell_world_space_size);
        Core_Bindings.core_stop_profiling();
        Core_Helpers.print_profiling(this.world_handle, false);
        
        Core_Helpers.debug_draw_world(this.world_handle, this.debug_draw_options, true);
    }

    void Start() {
        this.world_handle.invalidate();
        this.create_world();
    }

    void Destroy() {
        this.destroy_world();
    }
}
