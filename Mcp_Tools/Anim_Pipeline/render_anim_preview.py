# -*- coding: utf-8 -*-
"""render_anim_preview.py — 애니 프리뷰 오케스트레이터 (외부 Python).

헤드리스 Blender (창 없는 별도 프로세스 — 사용자 병행 작업 안전)로
콘택트 시트 + 키포즈 시트 + (옵션) MP4 프리뷰를 생성한다.

사용 예:
  python render_anim_preview.py --blend "BlenderAssets/Alex_Q.blend"
  python render_anim_preview.py --blend "BlenderAssets/Alex_Death.blend" ^
      --action Alex_Death --views front,side,tq --step 3 --keyposes 1,8,15,22 --mp4

산출물 (기본 Previews/<blend베이스명>/):
  f###_<view>.png   개별 프레임
  sheet_<view>.png  뷰별 콘택트 시트 (타이밍 판독용, 셀 320px)
  keyposes.png      키포즈 대형 시트 (포즈 품질 판독용, 셀 512px, front)
  preview.mp4       (--mp4 시) 사용자 확인용 영상
  meta.json         액션/범위/엔진/렌더러블 정보

※ 라이브 Blender MCP 세션에서 편집 중이면 디스크가 구버전 — 렌더 전
  bpy.ops.wm.save_mainfile() 필수 (루프: 편집 → 저장 → 본 스크립트 → 시트 Read).
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8")
    except Exception:
        pass

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BLENDER_EXE = os.environ.get(
    "BLENDER_EXE", r"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe")
BL_SCRIPT = os.path.join(SCRIPT_DIR, "bl_render_preview.py")
QA_SCRIPT = os.path.join(SCRIPT_DIR, "bl_anim_qa.py")


def run(args):
    if not args.blend and not args.fbx:
        raise SystemExit("[오류] --blend 또는 --fbx 필요")
    src = os.path.abspath(args.blend or args.fbx)
    if not os.path.exists(src):
        raise SystemExit("[오류] 소스 없음: %s" % src)
    if not os.path.exists(BLENDER_EXE):
        raise SystemExit("[오류] blender.exe 없음: %s (BLENDER_EXE 환경변수 설정)" % BLENDER_EXE)

    base = os.path.splitext(os.path.basename(src))[0]
    out_dir = os.path.abspath(args.out or os.path.join(SCRIPT_DIR, "Previews", base))
    os.makedirs(out_dir, exist_ok=True)

    views = [v.strip() for v in args.views.split(",") if v.strip()]
    cfg = {
        "out_dir": out_dir,
        "views": views,
        "step": args.step,
        "engine": args.engine,
        "res": args.res,
        "mp4": args.mp4,
        "mp4_view": args.mp4_view,
        "front_axis": args.front_axis,
    }
    if args.fbx:
        cfg["fbx"] = src
    if args.action:
        cfg["action"] = args.action
    if args.frames:
        cfg["frames"] = [int(f) for f in args.frames.split(",")]

    fd, cfg_path = tempfile.mkstemp(suffix=".json", prefix="animprev_")
    with os.fdopen(fd, "w", encoding="utf-8") as fp:
        json.dump(cfg, fp)

    cmd = ([BLENDER_EXE, "-b", src, "-P", BL_SCRIPT, "--", cfg_path]
           if args.blend else
           [BLENDER_EXE, "-b", "-P", BL_SCRIPT, "--", cfg_path])
    print("[렌더] %s" % " ".join('"%s"' % c if " " in c else c for c in cmd))
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              encoding="utf-8", errors="replace", timeout=1200)
    finally:
        os.unlink(cfg_path)
    done = "BL_RENDER_DONE" in (proc.stdout or "")
    if proc.returncode != 0 or not done:
        print(proc.stdout or "")
        print(proc.stderr or "")
        raise SystemExit("[오류] Blender 렌더 실패 (returncode=%d)" % proc.returncode)

    meta_path = os.path.join(out_dir, "meta.json")
    with open(meta_path, encoding="utf-8") as fp:
        meta = json.load(fp)

    sys.path.insert(0, SCRIPT_DIR)
    from make_contact_sheet import compose_view

    fps = meta["fps"]
    frame_start = meta["frame_range"][0]
    outputs = []
    for view in views:
        outputs.append(compose_view(out_dir, view, fps=fps,
                                    frame_start=frame_start))

    # 키포즈 대형 시트 (front 뷰): 명시 프레임 or 균등 5장
    key_frames = ([int(f) for f in args.keyposes.split(",")] if args.keyposes
                  else None)
    if key_frames is None:
        fr = meta["frames"]
        n = min(5, len(fr))
        key_frames = [fr[round(i * (len(fr) - 1) / max(n - 1, 1))] for i in range(n)]
    kp_view = "front" if "front" in views else views[0]
    kp = compose_view(out_dir, kp_view,
                      out_path=os.path.join(out_dir, "keyposes.png"),
                      fps=fps, cell_px=512, frames=key_frames,
                      frame_start=frame_start)
    outputs.append(kp)

    qa_report = None
    if args.qa:
        if not args.blend:
            print("[경고] --qa 는 blend 소스에서만 지원 — 생략")
        else:
            qa_cfg = {"out_dir": out_dir, "loop": args.loop,
                      "strict_slide": args.loop}
            if args.action:
                qa_cfg["action"] = args.action
            fd, qa_path = tempfile.mkstemp(suffix=".json", prefix="animqa_")
            with os.fdopen(fd, "w", encoding="utf-8") as fp:
                json.dump(qa_cfg, fp)
            try:
                subprocess.run([BLENDER_EXE, "-b", src, "-P", QA_SCRIPT,
                                "--", qa_path],
                               capture_output=True, text=True,
                               encoding="utf-8", errors="replace", timeout=600)
            finally:
                os.unlink(qa_path)
            qa_report = os.path.join(out_dir, "qa_report.txt")

    print("\n=== 완료 ===")
    print("액션: %s  범위: %s (%.2fs @ %dfps)  엔진: %s  렌더러블: %s"
          % (meta["action"], meta["frame_range"], meta["duration_s"],
             fps, meta["engine"], meta["renderable"]))
    for o in outputs:
        print("  시트: %s" % o)
    if meta.get("mp4"):
        print("  MP4 : %s" % meta["mp4"])
    if qa_report and os.path.exists(qa_report):
        print("  QA  : %s" % qa_report)
        with open(qa_report, encoding="utf-8") as fp:
            print(fp.read())
    print("  메타: %s" % meta_path)
    return outputs


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Blender 애니 콘택트 시트 렌더")
    ap.add_argument("--blend", help="blend 파일 경로")
    ap.add_argument("--fbx", help="FBX 직접 프리뷰 (소스 후보 비교용)")
    ap.add_argument("--action", help="액션 이름 (기본: 바인딩된 액션)")
    ap.add_argument("--views", default="front,side,tq")
    ap.add_argument("--step", type=int, default=3, help="프레임 간격 (기본 3)")
    ap.add_argument("--frames", help="명시 프레임 (예: 1,8,15) — step 무시")
    ap.add_argument("--keyposes", help="keyposes.png 대상 프레임 (기본: 균등 5장)")
    ap.add_argument("--engine", default="auto",
                    choices=["auto", "workbench", "eevee", "cycles"])
    ap.add_argument("--res", type=int, default=512)
    ap.add_argument("--mp4", action="store_true", help="preview.mp4 렌더")
    ap.add_argument("--mp4-view", default="tq")
    ap.add_argument("--qa", action="store_true", help="수치 QA 동시 실행 (blend 만)")
    ap.add_argument("--loop", action="store_true",
                    help="루프 모션 (첫↔끝 정합 + 슬라이드 strict)")
    ap.add_argument("--front-axis", default="-Y", choices=["-Y", "+Y", "-X", "+X"],
                    help="캐릭터 정면 월드 축 (기본 -Y)")
    ap.add_argument("--out", help="출력 폴더 (기본 Previews/<blend명>)")
    run(ap.parse_args())
