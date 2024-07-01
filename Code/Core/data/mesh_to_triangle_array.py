import bpy

f = open('C:/source/Thesis/Code/Core/data/triangle_data.h', 'w+')

current_obj = bpy.context.active_object
verts_local = [v.co for v in current_obj.data.vertices.values()]
verts_world = [current_obj.matrix_world @ v_local for v_local in verts_local]

print('#include "foundation.h"', file = f)
print('#include "typedefs.h"', file = f)
print('', file = f)
print('const Triangle triangle_vertices[] = {', file = f)

for i, face in enumerate(current_obj.data.polygons):
    verts_indices = face.vertices[:]
    print('    { ', file = f, end = '')
    for j in verts_indices:
        print('vec3(', verts_world[j].x, ', ', verts_world[j].y, ', ', verts_world[j].z, '), ', file = f, end = '', sep = '')
        
    print('},', file = f)

print('};', file = f)

f.flush()   
f.close()