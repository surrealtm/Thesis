using System.Collections;
using System.Collections.Generic;
using UnityEngine;

using static Core_Bindings;
using static Core_Helpers;

public class Tester : MonoBehaviour {
    public bool step_into = false;
    public Camera camera = null;
    private World_Handle world_handle;

    public void Start() {
        core_begin_profiling();
        this.world_handle = core_do_house_test(this.step_into);
        core_stop_profiling();
        print_profiling(true);
        debug_draw_world(this.world_handle, Debug_Draw_Options.Everything);
    }

    public void Update() {
        if(this.camera != null) make_texts_face_the_camera(this.camera);
    }
}
