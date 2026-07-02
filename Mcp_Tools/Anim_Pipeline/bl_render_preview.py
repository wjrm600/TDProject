# -*- coding: utf-8 -*-
"""bl_render_preview.py — Blender 내부 렌더 본체 (헤드리스/라이브 MCP 겸용).

헤드리스 (권장 — 창 없는 별도 프로세스라 병행 작업 안전):
  blender.exe -b <blend> -P bl_render_preview.py -- <cfg.json 경로>

라이브 Blender MCP 세션 (폴백):
  exec(open(r"...\\bl_render_preview.py").read()); main(cfg_dict)
  ※ 라이브에서는 cfg["cleanup"]=True 로 생성 오브젝트 자동 제거.

핵심:
- 아마추어(본)는 F12 렌더에 나오지 않음 → ensure_renderable() 이 본마다
  팔각뿔 프록시 메시를 생성(Copy Transforms 컨스트레인트로 본에 부착).
  좌(_l)=파랑 / 우(_r)=빨강 / 중앙=주황 (정면 뷰 좌우 판독용).
- 씬 프레임 범위를 액션 범위로 강제 (과거 6.4→8.3s 길이 버그 방어).
- 렌더 후 blend 를 저장하지 않음 (헤드리스는 메모리에서만 변경).

cfg 키:
  out_dir   : 출력 폴더 (필수)
  action    : 액션 이름 (기본: 아마추어에 바인딩된 액션)
  views     : ["front","side","tq"] (기본 3방향)
  frames    : 명시 프레임 리스트 (지정 시 step 무시)
  step      : 프레임 간격 (기본 3, 마지막 프레임 항상 포함)
  engine    : auto|workbench|eevee|cycles (기본 auto = workbench 우선)
  res       : 프레임 해상도 px (기본 512)
  mp4       : True 면 preview.mp4 렌더 (기본 False)
  mp4_view  : mp4 카메라 뷰 (기본 "tq")
  front_axis: 캐릭터 정면이 향하는 월드 축 "-Y"|"+Y"|"-X"|"+X" (기본 "-Y")
  cleanup   : 렌더 후 생성 오브젝트 제거 (라이브 세션용, 기본 False)
"""
import json
import math
import os
import sys

import bpy
import bmesh
from mathutils import Vector

PREFIX = "AnimPreview_"

COLOR_L = (0.15, 0.4, 1.0, 1.0)   # 왼쪽 본 = 파랑
COLOR_R = (1.0, 0.15, 0.15, 1.0)  # 오른쪽 본 = 빨강
COLOR_C = (1.0, 0.55, 0.1, 1.0)   # 중앙 본 = 주황

# 실루엣을 가리는 비변형 본 제외 (UE 마네킹: ik_foot_root, ik_hand_gun, root 등)
EXCLUDE_BONE_PREFIXES = ("ik_", "root")

ENGINE_IDS = {
    "workbench": "BLENDER_WORKBENCH",
    "eevee": "BLENDER_EEVEE",
    "cycles": "CYCLES",
}
AUTO_ORDER = ["workbench", "eevee", "cycles"]


def find_armature():
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    if not arms:
        raise RuntimeError("아마추어 없음")
    return arms[0]


def resolve_action(arm, action_name=None):
    """액션 확정 + 씬 프레임 범위 = 액션 범위 강제. (start, end) 반환."""
    ad = arm.animation_data
    if ad is None:
        raise RuntimeError("armature.animation_data 없음")
    if action_name:
        act = bpy.data.actions.get(action_name)
        if act is None:
            names = [a.name for a in bpy.data.actions]
            raise RuntimeError("액션 '%s' 없음. 존재: %s" % (action_name, names))
        ad.action = act
        # Blender 4.4+ slotted action: 액션 교체 시 슬롯 재바인딩 필요
        try:
            if act.slots and ad.action_slot is None:
                ad.action_slot = act.slots[0]
        except AttributeError:
            pass
    act = ad.action
    if act is None:
        raise RuntimeError("바인딩된 액션 없음 — cfg['action'] 지정 필요. 존재: %s"
                           % [a.name for a in bpy.data.actions])
    fs, fe = act.frame_range
    start, end = int(math.floor(fs)), int(math.ceil(fe))
    scene = bpy.context.scene
    scene.frame_start, scene.frame_end = start, end
    return act.name, start, end


COLOR_W = (1.0, 0.95, 0.3, 1.0)   # 무기 본 = 밝은 노랑 (실루엣 핵심)


def _bone_color(name):
    low = name.lower()
    if "weapon" in low or "sword" in low:
        return COLOR_W
    if low.endswith("_l") or "_l_" in low:
        return COLOR_L
    if low.endswith("_r") or "_r_" in low:
        return COLOR_R
    return COLOR_C


def _included_bones(arm, exclude_prefixes):
    out = []
    for pb in arm.pose.bones:
        low = pb.name.lower()
        if any(low.startswith(p) for p in exclude_prefixes):
            continue
        if pb.bone.length < 1e-6:
            continue
        out.append(pb)
    return out


def _make_bone_proxy_mesh(length, width_cap, name):
    """+Y 로 length 만큼 뻗는 팔각뿔(본 모양) 메시. 굵기는 전역 캡으로 제한."""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    w = max(min(length * 0.15, width_cap), width_cap * 0.25)
    j = min(length * 0.15, w)
    v = [bm.verts.new(p) for p in (
        (0, 0, 0),
        (w, j, w), (-w, j, w), (-w, j, -w), (w, j, -w),
        (0, length, 0))]
    for f in ((0, 1, 2), (0, 2, 3), (0, 3, 4), (0, 4, 1),
              (5, 2, 1), (5, 3, 2), (5, 4, 3), (5, 1, 4)):
        bm.faces.new([v[i] for i in f])
    bm.to_mesh(mesh)
    bm.free()
    return mesh


def ensure_renderable(arm, bones, fig_height_world, created, ext_map):
    """스킨 메시가 없으면 본 프록시 메시 생성 (굵기 = 캐릭터 높이의 2%).

    주의: 프록시 지오메트리는 본 로컬 단위인데 fig_height 는 월드 단위 —
    아마추어 오브젝트 스케일(UE FBX 는 보통 0.01)로 나눠 로컬로 환산해야 함.
    ext_map(출력): 연장된 무기 본 {이름: 블레이드 길이(로컬)} — bbox 재계산용.
    """
    for o in bpy.data.objects:
        if o.type == "MESH" and any(
                m.type == "ARMATURE" and m.object == arm for m in o.modifiers):
            return "skinned_mesh"
    scene = bpy.context.scene
    sc = arm.matrix_world.to_scale()
    avg_scale = max((sc.x + sc.y + sc.z) / 3.0, 1e-9)
    fig_height_local = fig_height_world / avg_scale
    width_cap = max(fig_height_local * 0.02, 1e-4)
    # 무기 본은 보통 부착 소켓(짧음) — 블레이드 길이로 연장해 궤적 시각화
    blade_len = fig_height_local * 0.75
    n = 0
    for pb in bones:
        length = pb.bone.length
        if _bone_color(pb.name) == COLOR_W and length < blade_len:
            length = blade_len
            ext_map[pb.name] = blade_len
        mesh = _make_bone_proxy_mesh(length, width_cap,
                                     PREFIX + "M_" + pb.name)
        if pb.name.lower() == "head":
            _add_head_sphere(mesh, pb.bone.length)
        obj = bpy.data.objects.new(PREFIX + pb.name, mesh)
        obj.color = _bone_color(pb.name)
        scene.collection.objects.link(obj)
        con = obj.constraints.new("COPY_TRANSFORMS")
        con.target = arm
        con.subtarget = pb.name
        created.append(obj)
        n += 1
    return "bone_proxy(%d)" % n


def _add_head_sphere(mesh, length):
    """head 본 중간에 구체를 붙여 인체 실루엣의 머리 식별을 돕는다."""
    bm = bmesh.new()
    bm.from_mesh(mesh)
    ret = bmesh.ops.create_icosphere(bm, subdivisions=1, radius=length * 0.5)
    for v in ret["verts"]:
        v.co.y += length * 0.6
    bm.to_mesh(mesh)
    bm.free()


def add_ground_plane(center, size, ground_z, created):
    """접지 판독용 지면 (얇은 박스 — 수평 카메라에서도 엣지 라인이 보임)."""
    scene = bpy.context.scene
    mesh = bpy.data.meshes.new(PREFIX + "GroundMesh")
    bm = bmesh.new()
    r = max(size.x, size.y, size.z) * 1.2
    t = size.z * 0.01 + 1e-5
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x = center.x + v.co.x * 2 * r
        v.co.y = center.y + v.co.y * 2 * r
        v.co.z = ground_z + (v.co.z - 0.5) * 2 * t  # 윗면 = ground_z
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(PREFIX + "Ground", mesh)
    obj.color = (0.13, 0.13, 0.16, 1.0)
    scene.collection.objects.link(obj)
    created.append(obj)


def compute_bbox(arm, bones, start, end, ext_map=None):
    """전 구간(≤12샘플) 본 head/tail 월드 좌표의 바운딩박스.

    ext_map: {본이름: 연장길이(로컬)} — 무기 블레이드 프록시 팁을 bbox 에 포함
    (없으면 블레이드가 카메라 프레임 밖으로 잘림).
    """
    scene = bpy.context.scene
    step = max(1, (end - start) // 12)
    lo = Vector((1e18, 1e18, 1e18))
    hi = Vector((-1e18, -1e18, -1e18))
    for f in list(range(start, end + 1, step)) + [end]:
        scene.frame_set(f)
        mw = arm.matrix_world
        for pb in bones:
            pts = [mw @ pb.head, mw @ pb.tail]
            if ext_map and pb.name in ext_map:
                d = pb.tail - pb.head
                if d.length > 1e-9:
                    pts.append(mw @ (pb.head + d.normalized() * ext_map[pb.name]))
            for p in pts:
                lo = Vector(map(min, lo, p))
                hi = Vector(map(max, hi, p))
    return lo, hi


def _camera_for_view(view, center, size, front_axis, created):
    """직교 카메라 생성. front_axis = 캐릭터 정면이 향하는 월드 축."""
    cam_data = bpy.data.cameras.new(PREFIX + "Cam_" + view)
    cam_data.type = "ORTHO"
    cam = bpy.data.objects.new(PREFIX + "Cam_" + view, cam_data)
    bpy.context.scene.collection.objects.link(cam)
    created.append(cam)

    dist = max(size.x, size.y, size.z) * 3.0 + 1.0
    cam_data.clip_end = dist * 4.0
    # front_axis 기준 정면 카메라의 방위각 (카메라가 서는 방향)
    azim = {"-Y": -90.0, "+Y": 90.0, "-X": 180.0, "+X": 0.0}[front_axis]
    elev = 0.0
    if view == "side":
        azim += 90.0
    elif view == "tq":
        azim += 45.0
        elev = 18.0  # 지면·깊이 판독용 고도각
    a, e = math.radians(azim), math.radians(elev)
    cam.location = (center.x + dist * math.cos(e) * math.cos(a),
                    center.y + dist * math.cos(e) * math.sin(a),
                    center.z + dist * math.sin(e))
    # 카메라 -Z 가 center 를 향하도록: yaw = azim+90°, pitch = 90°-elev
    cam.rotation_euler = (math.pi / 2.0 - e, 0.0, math.radians(azim + 90.0))

    if view == "side":
        extent = max(size.y, size.z)
    elif view == "front":
        extent = max(size.x, size.z)
    else:
        extent = max(size.x, size.y, size.z)
    cam_data.ortho_scale = extent * 1.2
    return cam


def setup_lights(created):
    if any(o.type == "LIGHT" for o in bpy.data.objects):
        return
    scene = bpy.context.scene
    for name, rot, energy in (
            ("Key", (0.9, 0.2, 0.6), 3.0),
            ("Fill", (1.1, -0.3, -1.2), 1.2),
            ("Rim", (-0.8, 0.1, 2.6), 2.0)):
        data = bpy.data.lights.new(PREFIX + name, type="SUN")
        data.energy = energy
        obj = bpy.data.objects.new(PREFIX + name, data)
        obj.rotation_euler = rot
        scene.collection.objects.link(obj)
        created.append(obj)


def pick_engine(engine):
    scene = bpy.context.scene
    order = AUTO_ORDER if engine in (None, "auto") else [engine]
    for key in order:
        try:
            scene.render.engine = ENGINE_IDS[key]
            if key == "cycles":
                scene.cycles.samples = 16
                scene.cycles.device = "CPU"
            return key
        except Exception:
            continue
    raise RuntimeError("사용 가능한 렌더 엔진 없음: %s" % order)


def main(cfg):
    out_dir = cfg["out_dir"]
    os.makedirs(out_dir, exist_ok=True)

    if cfg.get("fbx"):  # FBX 직접 프리뷰 (빈 씬에 임포트 — 소스 후보 비교용)
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=cfg["fbx"],
                                 automatic_bone_orientation=False)

    scene = bpy.context.scene
    created = []

    arm = find_armature()
    action_name, start, end = resolve_action(arm, cfg.get("action"))

    exclude = tuple(cfg.get("exclude_bones", EXCLUDE_BONE_PREFIXES))
    bones = _included_bones(arm, exclude)
    lo, hi = compute_bbox(arm, bones, start, end)
    size = hi - lo

    ext_map = {}
    renderable = ensure_renderable(arm, bones, size.z, created, ext_map)
    if ext_map:  # 블레이드 프록시 팁까지 프레임에 들어오도록 bbox 재계산
        lo, hi = compute_bbox(arm, bones, start, end, ext_map)
        size = hi - lo
    center = (lo + hi) / 2.0

    # 지면 = 아마추어 원점 높이 (UE 관례: root 가 지면). bbox 최저점을 쓰면
    # 슬램 등에서 무기가 지면을 뚫는 프레임 때문에 발이 떠 보임.
    ground_z = arm.matrix_world.translation.z
    add_ground_plane(center, size, ground_z, created)
    setup_lights(created)
    engine = pick_engine(cfg.get("engine", "auto"))

    # Workbench: 오브젝트 색으로 본 좌우 구분 + 어두운 배경으로 대비 확보
    scene.display.shading.color_type = "OBJECT"
    scene.display.shading.light = "STUDIO"
    try:
        scene.display.shading.background_type = "VIEWPORT"
        scene.display.shading.background_color = (0.32, 0.33, 0.38)
    except Exception:
        pass

    res = int(cfg.get("res", 512))
    scene.render.resolution_x = res
    scene.render.resolution_y = res
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False

    frames = cfg.get("frames")
    if not frames:
        step = int(cfg.get("step", 3))
        frames = list(range(start, end + 1, step))
        if frames[-1] != end:
            frames.append(end)
    frames = [f for f in frames if start <= f <= end]

    views = cfg.get("views", ["front", "side", "tq"])
    front_axis = cfg.get("front_axis", "-Y")
    cams = {v: _camera_for_view(v, center, size, front_axis, created) for v in views}

    rendered = []
    for view in views:
        scene.camera = cams[view]
        for f in frames:
            scene.frame_set(f)
            path = os.path.join(out_dir, "f%03d_%s.png" % (f, view))
            scene.render.filepath = path
            bpy.ops.render.render(write_still=True)
            rendered.append(path)

    mp4_path = None
    if cfg.get("mp4"):
        mp4_view = cfg.get("mp4_view", "tq")
        scene.camera = cams.get(mp4_view) or list(cams.values())[0]
        img = scene.render.image_settings
        try:
            img.media_type = "VIDEO"  # Blender 5.x
        except AttributeError:
            img.file_format = "FFMPEG"  # Blender 4.x
        try:
            img.file_format = "FFMPEG"
        except Exception:
            pass
        scene.render.ffmpeg.format = "MPEG4"
        scene.render.ffmpeg.codec = "H264"
        scene.render.ffmpeg.constant_rate_factor = "MEDIUM"
        mp4_path = os.path.join(out_dir, "preview.mp4")
        scene.render.filepath = mp4_path
        bpy.ops.render.render(animation=True)

    meta = {
        "blend": bpy.data.filepath,
        "action": action_name,
        "frame_range": [start, end],
        "fps": scene.render.fps,
        "duration_s": round((end - start) / scene.render.fps, 3),
        "frames": frames,
        "views": views,
        "front_axis": front_axis,
        "engine": engine,
        "renderable": renderable,
        "res": res,
        "mp4": mp4_path,
        "bbox_min": list(lo), "bbox_max": list(hi),
        "color_legend": {"blue": "left(_l)", "red": "right(_r)", "orange": "center"},
    }
    with open(os.path.join(out_dir, "meta.json"), "w", encoding="utf-8") as fp:
        json.dump(meta, fp, ensure_ascii=False, indent=2)

    if cfg.get("cleanup"):
        for o in created:
            bpy.data.objects.remove(o, do_unlink=True)

    print("BL_RENDER_DONE frames=%d views=%d engine=%s renderable=%s"
          % (len(frames), len(views), engine, renderable))
    return meta


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if not argv:
        raise SystemExit("사용법: blender -b <blend> -P bl_render_preview.py -- <cfg.json>")
    with open(argv[0], encoding="utf-8-sig") as fp:  # PowerShell BOM 허용
        main(json.load(fp))
