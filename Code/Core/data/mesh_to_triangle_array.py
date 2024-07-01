import bpy

f = open('C:/source/Thesis/Code/Core/data/triangle_data.txt', 'w+')

current_obj = bpy.context.active_object
verts_local = [v.co for v in current_obj.data.vertices.values()]
verts_world = [current_obj.matrix_world @ v_local for v_local in verts_local]

for i, face in enumerate(current_obj.data.polygons):
    verts_indices = face.vertices[:]
    for j in verts_indices:
        print('', -verts_world[j].x, ',', verts_world[j].z, ',', -verts_world[j].y, ',', file = f, end = '', sep = '')
    print('', file = f)

f.flush()   
f.close()