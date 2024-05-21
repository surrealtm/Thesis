using System.Collections;
using System.Collections.Generic;
using UnityEngine;

using static Core_Bindings;
using static Core_Helpers;

public class Tester : MonoBehaviour {
    public bool step_into = false;
    public Camera camera = null;
    public Debug_Draw_Options debug_draw_options = Debug_Draw_Options.Everything;
    private World_Handle world_handle;
    private bool rebuild_draw_data = false;
    
    public void Start() {
        core_begin_profiling();
        this.world_handle = core_do_house_test(this.step_into);
        core_stop_profiling();
        print_profiling(true);
        debug_draw_world(this.world_handle, this.debug_draw_options, true);
    }

    public void Update() {
        if(this.camera != null) make_texts_face_the_camera(this.camera);

        if(this.rebuild_draw_data) {
            debug_draw_world(this.world_handle, this.debug_draw_options, true);
            this.rebuild_draw_data = false;
        }
    }

    public void OnValidate() {
        // Unity crashes when doing debug_draw_world in here (maybe this is a different thread?), so instead
        // do this thing here (which obviously isn't thread safe, but eh who cares).
        this.rebuild_draw_data = true;
    }
}
