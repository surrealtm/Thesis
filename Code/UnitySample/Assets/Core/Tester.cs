using System.Collections;
using System.Collections.Generic;
using UnityEngine;

using static Core_Bindings;
using static Core_Helpers;

public class Tester : MonoBehaviour {
    public bool step_into = false;
    private World_Handle world_handle;

    public void Start() {
        core_begin_profiling();
        this.world_handle = core_do_house_test(this.step_into);
        core_stop_profiling();
        print_profiling(true);
    }
        
    public void Update() {                
        debug_draw_world(world_handle, Debug_Draw_Options.Everything);
    }
}
