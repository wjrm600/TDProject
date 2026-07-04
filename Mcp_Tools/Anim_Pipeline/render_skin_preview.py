# -*- coding: utf-8 -*-
"""render_skin_preview.py — 애니 blend 를 **실제 char1 스킨메시**로 렌더 (자가 시각검증).

bone-proxy 렌더(render_anim_preview.py)는 비틀림(roll)·스킨 관통을 못 본다.
이 스크립트는 UE 에서 export 한 char1 스킨메시를 붙여 실제 살 변형을 렌더한다.

준비물: char1_skin.fbx (UE char1_accurig SkeletalMesh export, 이 폴더에 동봉).
  재생성 = UE 파이썬: unreal.Exporter.run_asset_export_task(SkeletalMeshExporterFBX,
  object=/Game/AOS/Meshes/Characters/Alex/AccuRig/char1_accurig).

사용:
  python render_skin_preview.py --blend ..\..\BlenderAssets\Alex_E.blend
  python render_skin_preview.py --blend <blend> --frames 1,15,20,37,46,60

산출: Previews/<blend>_skin/skin_sheet.png (+ 개별 skin_fNNN.png)
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from make_contact_sheet import compose  # noqa: E402

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8")
    except Exception:
        pass

BLENDER = os.environ.get(
    "BLENDER_EXE", r"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe")


def main():
    ap = argparse.ArgumentParser(description="애니 blend → 실제 스킨메시 콘택트 시트")
    ap.add_argument("--blend", required=True, help="애니된 'root' armature 를 가진 blend")
    ap.add_argument("--skin", default=os.path.join(HERE, "char1_skin.fbx"),
                    help="char1 스킨메시 FBX (기본: 동봉본)")
    ap.add_argument("--frames", default="", help="렌더 프레임 CSV (생략=균등 12장)")
    ap.add_argument("--out", default="", help="출력 폴더 (기본 Previews/<blend>_skin)")
    args = ap.parse_args()

    blend = os.path.abspath(args.blend)
    skin = os.path.abspath(args.skin)
    if not os.path.exists(blend):
        raise SystemExit("[오류] blend 없음: %s" % blend)
    if not os.path.exists(skin):
        raise SystemExit("[오류] 스킨 FBX 없음: %s" % skin)
    base = os.path.splitext(os.path.basename(blend))[0]
    out_dir = os.path.abspath(args.out or os.path.join(HERE, "Previews", base + "_skin"))
    os.makedirs(out_dir, exist_ok=True)

    cmd = [BLENDER, "-b", blend, "-P", os.path.join(HERE, "bl_skin_preview.py"),
           "--", skin, out_dir, args.frames]
    print("[렌더]", " ".join('"%s"' % c if " " in c else c for c in cmd))
    res = subprocess.run(cmd, capture_output=True, text=True)
    tail = "\n".join(res.stdout.splitlines()[-6:])
    print(tail)
    if "SKIN_FRAMES_DONE" not in res.stdout:
        print(res.stderr[-2000:])
        raise SystemExit("[오류] 스킨 렌더 실패")

    pngs = sorted(f for f in os.listdir(out_dir)
                  if f.startswith("skin_f") and f.endswith(".png"))
    items = [(i, os.path.join(out_dir, p)) for i, p in enumerate(pngs)]
    labels = ["F" + p[6:9].lstrip("0").rjust(1) for p in pngs]
    sheet = compose(items, os.path.join(out_dir, "skin_sheet.png"),
                    cell_px=440, labels=labels)
    print("=== 스킨 미리보기 완료 ===")
    print("  시트:", sheet)


if __name__ == "__main__":
    main()
