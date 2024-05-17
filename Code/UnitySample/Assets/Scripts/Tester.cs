using System.Collections;
using System.Collections.Generic;
using UnityEngine;

using static Core_Bindings;
using static Core_Helpers;

public class Tester : MonoBehaviour {
    public bool step_into = false;
    
    public void Start() {
        core_begin_profiling();
        core_do_house_test(this.step_into);
        core_stop_profiling();
        print_profiling(true);
    }
}
