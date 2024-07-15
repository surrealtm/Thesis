using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;

public enum Test_Case {
    Scene,
    House,
    BVH,
    Large_Volumes,
    Cutout,
    Circle,
    U_Shape,
    Center_Block,
    Gallery,
    Roof,
    Jobs,
}

public class Tester : MonoBehaviour {
    [CustomEditor(typeof(Tester))]
    public class Inspector : Editor {
        public override void OnInspectorGUI() {
            DrawDefaultInspector();

            if(GUILayout.Button("Create From Scene")) {
                ((Tester) this.target).create_from_scene_flag = true;
            }

            if(GUILayout.Button("Re-Run Tests")) {
                ((Tester) this.target).rerun_test_flag = true;
            }

            if(GUILayout.Button("Clear Draw Data")) {
                ((Tester) this.target).clear_draw_data_flag = true;                
            }

            if(GUILayout.Button("Destroy World")) {
                ((Tester) this.target).destroy_world_flag = true;
            }
        }
    }

    public bool step_into = false;
    public Camera camera = null;
    public Debug_Draw_Options debug_draw_options = Debug_Draw_Options.Everything;
    public Test_Case test_case = Test_Case.House;

    private World_Handle world_handle;
    private bool create_from_scene_flag = false;
    private bool rerun_test_flag        = false;
    private bool clear_draw_data_flag   = false;
    private bool destroy_world_flag     = false;
    
    public void run_test() {
        this.destroy_world();

        Core_Bindings.core_begin_profiling();

        switch(this.test_case) {
        case Test_Case.Scene:
            this.world_handle = this.create_world_from_scene();
            break;

        case Test_Case.House:
            this.world_handle = Core_Bindings.core_do_house_test(this.step_into);
            break;

        case Test_Case.BVH:
            this.world_handle = Core_Bindings.core_do_bvh_test(this.step_into);
            break;

        case Test_Case.Large_Volumes:
            this.world_handle = Core_Bindings.core_do_large_volumes_test(this.step_into);
            break;

        case Test_Case.Cutout:
            this.world_handle = Core_Bindings.core_do_cutout_test(this.step_into);
            break;

        case Test_Case.Circle:
            this.world_handle = Core_Bindings.core_do_circle_test(this.step_into);
            break;

        case Test_Case.U_Shape:
            this.world_handle = Core_Bindings.core_do_u_shape_test(this.step_into);
            break;

        case Test_Case.Center_Block:
            this.world_handle = Core_Bindings.core_do_center_block_test(this.step_into);
            break;

        case Test_Case.Gallery:
            this.world_handle = Core_Bindings.core_do_gallery_block_test(this.step_into);
            break;

        case Test_Case.Roof:
            this.world_handle = Core_Bindings.core_do_roof_block_test(this.step_into);
            break;

        case Test_Case.Jobs:
            this.world_handle = Core_Bindings.core_do_jobs_test(this.step_into);
            break;
        }

        Core_Bindings.core_stop_profiling();
        Core_Helpers.print_profiling(this.world_handle, true);
        Core_Helpers.debug_draw_world(this.world_handle, this.debug_draw_options, true);
    }

    public World_Handle create_world_from_scene() {
        this.destroy_world();
        this.world_handle = Core_Helpers.create_world_from_scene();
        return this.world_handle;
    }

    public void destroy_world() {
        if(this.world_handle.valid()) {
            Core_Helpers.clear_debug_draw();
            Core_Bindings.core_destroy_world(this.world_handle);
            this.world_handle.invalidate();
        }
    }

    public void Start() {
        //this.run_test();
    }

    public void Update() {
        if(this.camera != null) Core_Helpers.make_texts_face_the_camera(this.camera);

        if(this.create_from_scene_flag) {
            this.create_world_from_scene();
            this.create_from_scene_flag = false;
        }

        if(this.rerun_test_flag) {
            this.run_test();
            this.rerun_test_flag = false;
        }

        if(this.clear_draw_data_flag) {
            Core_Helpers.clear_debug_draw();
            this.clear_draw_data_flag = false;
        }

        if(this.destroy_world_flag) {
            this.destroy_world();
            this.destroy_world_flag = false;
        }
    }

    public void OnValidate() {
        // Unity crashes when doing debug_draw_world in here (maybe this is a different thread?), so instead
        // do this thing here (which obviously isn't thread safe, but eh who cares).
        this.rerun_test_flag = true;
    }
}
