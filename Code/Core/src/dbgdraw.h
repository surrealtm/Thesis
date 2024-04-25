#pragma once

#include "foundation.h"
#include "strings.h"
#include "math/v3.h"
#include "math/qt.h"

struct World;

enum Debug_Draw_Options {
    DEBUG_DRAW_Nothing             = 0x0,
    DEBUG_DRAW_Octree              = 0x1,
    DEBUG_DRAW_Anchors             = 0x2,
    DEBUG_DRAW_Boundaries          = 0x4,
    DEBUG_DRAW_Clipping_Faces      = 0x8,
    DEBUG_DRAW_Clipping_Wireframes = 0x10,
    DEBUG_DRAW_Volume_Faces        = 0x20,
    DEBUG_DRAW_Volume_Wireframes   = 0x40,
    DEBUG_DRAW_Labels              = 0x1000, // Draw hud texts for all anchors and boundaries.
    DEBUG_DRAW_Normals             = 0x2000, // Draw normals for clipping planes and volumes.
    DEBUG_DRAW_Everything          = 0xffffffff,
};

struct Debug_Draw_Line {
    v3f p0, p1;
    f32 thickness;
    u8 r, g, b;
};

struct Debug_Draw_Triangle {
    v3f p0, p1, p2;
    u8 r, g, b, a;
};

struct Debug_Draw_Text {
    v3f position;
    string text;
    u8 r, g, b;
};

struct Debug_Draw_Cuboid {
    v3f position;
    v3f size;
    qtf rotation;
    u8 r, g, b;
};

struct Debug_Draw_Sphere {
    v3f position;
    f32 radius;
    u8 r, g, b;
};

struct Debug_Draw_Data {
    Debug_Draw_Line *lines;
    s64 line_count;

    Debug_Draw_Triangle *triangles;
    s64 triangle_count;
    
    Debug_Draw_Text *texts;
    s64 text_count;

    Debug_Draw_Cuboid *cuboids;
    s64 cuboid_count;

    Debug_Draw_Sphere *spheres;
    s64 sphere_count;
};

Debug_Draw_Data debug_draw_world(World *world, Debug_Draw_Options options);
void free_debug_draw_data(Debug_Draw_Data *data);
