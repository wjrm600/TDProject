# -*- coding: utf-8 -*-
"""bl_render_uefbx.py — UE 에서 export 한 애님 FBX(메시+스켈레톤+애니 포함)를
그대로 Blender 에서 렌더해 콘택트 시트용 프레임을 뽑는다. (리타깃 결과 자가검증용.)

UE export 옵션 export_preview_mesh=True 로 뽑으면 메시가 함께 들어 있어
리타깃 없이 바로 살(skin) 변형을 볼 수 있다. 카메라는 메시 바운딩박스로 자동 프레이밍.

호출:
  blender -b -P bl_render_uefbx.py -- <anim.fbx> <out_dir> [frames_csv] [view]
  view: front(기본) | side | tq(3/4)
"""
import bpy, sys, os, math
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
fbx = argv[0]
out_dir = argv[1]
frames_csv = argv[2] if len(argv) > 2 else ""
view = argv[3] if len(argv) > 3 else "front"

# clean scene
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
vl = bpy.context.view_layer

bpy.ops.import_scene.fbx(filepath=fbx)
arm = next((o for o in bpy.data.objects if o.type == "ARMATURE"), None)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]

f0, f1 = sc.frame_start, sc.frame_end
# if the imported action set a different range, honor armature action
if arm and arm.animation_data and arm.animation_data.action:
    fr = arm.animation_data.action.frame_range
    f0, f1 = int(fr[0]), int(fr[1])
sc.frame_start, sc.frame_end = f0, f1

if frames_csv:
    frames = [int(x) for x in frames_csv.split(",")]
else:
    n = 12
    step = max(1, (f1 - f0) // (n - 1))
    frames = list(range(f0, f1 + 1, step))[:n]

# --- compute mesh world bbox across a mid frame ---
sc.frame_set(frames[len(frames)//2]); vl.update()
mn = Vector((1e9, 1e9, 1e9)); mx = Vector((-1e9, -1e9, -1e9))
for m in meshes:
    for corner in m.bound_box:
        wc = m.matrix_world @ Vector(corner)
        for i in range(3):
            mn[i] = min(mn[i], wc[i]); mx[i] = max(mx[i], wc[i])
center = (mn + mx) * 0.5
size = mx - mn
height = max(size.z, 0.1)
reach = max(size.x, size.y, size.z)

# --- camera ---
cam_d = bpy.data.cameras.new("C"); cam = bpy.data.objects.new("C", cam_d)
sc.collection.objects.link(cam); sc.camera = cam
cam.data.lens = 50
dist = reach * 2.4 + height * 0.6
if view == "side":
    cam.location = center + Vector((dist, 0, height * 0.05))
    cam.rotation_euler = (math.radians(90), 0, math.radians(90))
elif view == "top":
    cam.location = center + Vector((0, 0, dist))
    cam.rotation_euler = (0, 0, 0)  # looking straight down -Z; +Y up in frame
elif view == "tq":
    cam.location = center + Vector((dist * 0.7, -dist * 0.7, height * 0.25))
    cam.rotation_euler = (math.radians(78), 0, math.radians(45))
else:  # front (character usually faces -Y in UE export)
    cam.location = center + Vector((0, -dist, height * 0.05))
    cam.rotation_euler = (math.radians(90), 0, 0)

# --- workbench single-color (readable for dark meshes) ---
sc.render.engine = "BLENDER_WORKBENCH"
sh = sc.display.shading
sh.light = "STUDIO"
sh.color_type = "SINGLE"
sh.single_color = (0.78, 0.79, 0.83)
sc.render.resolution_x = 440
sc.render.resolution_y = 620
sc.render.film_transparent = False

os.makedirs(out_dir, exist_ok=True)
for f in frames:
    sc.frame_set(f); vl.update()
    sc.render.filepath = os.path.join(out_dir, "skin_f%03d.png" % f)
    bpy.ops.render.render(write_still=True)

print("UEFBX_FRAMES_DONE", ",".join(str(f) for f in frames))
print("BBOX height=%.3f reach=%.3f center=(%.2f,%.2f,%.2f)" % (height, reach, center.x, center.y, center.z))
