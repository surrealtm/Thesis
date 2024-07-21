using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

[Serializable]
public struct Delimiter_Plane {
    public Axis_Index axis;
    public bool centered;
    public bool extend_u;
    public bool extend_v;
}

public class Delimiter : MonoBehaviour {
    [SerializeField]
    public Delimiter_Plane[] planes;
    public byte level    = 0;
}
