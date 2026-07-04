# -*- coding: utf-8 -*-
"""bl_skin_preview.py — Blender 내부 실행. 애니된 'root' armature 를 실제 char1
스킨메시로 렌더한다 (본 프록시가 아닌 진짜 살 변형 확인용).

원리: UE 에서 export 한 char1 스킨메시(FBX)는 char1_accurig_Skeleton(마네킹 호환,
본 이름 pelvis/thigh_l/... 이 'root' 와 일치, rest orientation dot~1.0)에 물려 있다.
그래서 리타겟 없이 'root' 애니의 회전을 **본 이름으로 그대로 복사**해 스킨 armature 를
구동하면 정확히 변형된다. (스케일: root=0.01 / UE=1.0 → pelvis translation 만 환산.)

호출:
  blender -b <anim.blend> -P bl_skin_preview.py -- <skin_fbx> <out_dir> [frames_csv]
  frames_csv 생략 시 범위 균등 12장.
"""
import bpy, sys, os, math

argv = sys.argv[sys.argv.index("--") + 1:]
skin_fbx = argv[0]
out_dir = argv[1]
frames_csv = argv[2] if len(argv) > 2 else ""

sc = bpy.context.scene
vl = bpy.context.view_layer
root = bpy.data.objects.get("root") or next(o for o in bpy.data.objects if o.type == "ARMATURE")

f0, f1 = sc.frame_start, sc.frame_end
if frames_csv:
    frames = [int(x) for x in frames_csv.split(",")]
else:
    step = max(1, (f1 - f0) // 11)
    frames = list(range(f0, f1 + 1, step))

# --- import skin mesh + its (native-scale) armature ---
before = set(o.name for o in bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=skin_fbx)
new = [o for o in bpy.data.objects if o.name not in before]
UE = next(o for o in new if o.type == "ARMATURE")
mesh = next(o for o in new if o.type == "MESH")

common = [b.name for b in UE.pose.bones if b.name in root.pose.bones]
sf = root.scale.x / UE.scale.x  # root-local -> UE-local translation factor

# --- copy 'root' animation onto UE armature by bone name (rest matches) ---
for f in range(f0, f1 + 1):
    sc.frame_set(f)
    vl.update()
    for bn in common:
        d = UE.pose.bones[bn]
        d.rotation_mode = "QUATERNION"
        d.rotation_quaternion = root.pose.bones[bn].rotation_quaternion.copy()
    if "pelvis" in common:
        UE.pose.bones["pelvis"].location = root.pose.bones["pelvis"].location * sf
    for bn in common:
        UE.pose.bones[bn].keyframe_insert("rotation_quaternion")
    if "pelvis" in common:
        UE.pose.bones["pelvis"].keyframe_insert("location")

# --- camera (3/4 front; character faces -Y) + workbench ---
cam_d = bpy.data.cameras.new("SkinCam")
cam = bpy.data.objects.new("SkinCam", cam_d)
sc.collection.objects.link(cam)
cam.location = (0.5, -3.7, 0.95)
cam.rotation_euler = (math.radians(90), 0, 0)
cam.data.lens = 45
sc.camera = cam
sc.render.engine = "BLENDER_WORKBENCH"
sc.render.resolution_x = 440
sc.render.resolution_y = 620
sc.render.film_transparent = False

os.makedirs(out_dir, exist_ok=True)
for f in frames:
    sc.frame_set(f)
    vl.update()
    sc.render.filepath = os.path.join(out_dir, "skin_f%03d.png" % f)
    bpy.ops.render.render(write_still=True)

print("SKIN_FRAMES_DONE", ",".join(str(f) for f in frames))
