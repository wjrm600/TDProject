# -*- coding: utf-8 -*-
"""gen_ui_kit.py — 매니페스트 기반 UI 텍스처 컴포넌트 키트 배치 생성.

흐름 (컴포넌트당):
  draw_controls 절차 도면 → ComfyUI ControlNet(scribble, denoise 1.0) 생성
  → ui_postprocess (대칭/지오메트리 마스크/3상태 파생/9-slice 검증)
  → RawAssets/UI_Kit/<name>_<i>.png  (+ <name>_<i>_Hover/_Pressed)

사용 예:
  python gen_ui_kit.py --manifest ui_kit_manifest.json [--only Shop_Panel]
      [--count 3] [--seed 42] [--no-cn]

기존 검증 코드 재사용: gen_controlnet.py 의 HTTP 클라이언트/그래프 함수 import.
CN 없이(--no-cn) vanilla 생성도 가능 (지오메트리 정밀도만 절충).
"""
import argparse
import io
import json
import os
import sys

from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)
import draw_controls  # noqa: E402
import gen_controlnet as gc  # noqa: E402  (HTTP 클라이언트 재사용)
import gen_icon  # noqa: E402  (vanilla 그래프 재사용)
import ui_postprocess as post  # noqa: E402

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8")
    except Exception:
        pass

OUT_DIR = os.path.join(SCRIPT_DIR, "RawAssets", "UI_Kit")
CTRL_DIR = os.path.join(OUT_DIR, "controls")


def apply_post(img, steps, comp, results):
    """매니페스트 post 단계 적용. 반환 = {접미사: Image} (기본 "": 마스터)."""
    out = {"": img}
    for step in steps:
        name, _, arg = step.partition(":")
        if name == "symmetrize_x":
            out = {k: post.symmetrize_x(v) for k, v in out.items()}
        elif name == "mask_rounded":
            mask = post.mask_from_rounded_rect(out[""].size, float(arg))
            out = {k: post.apply_mask(v, mask) for k, v in out.items()}
        elif name == "mask_full":
            out = {k: v.convert("RGBA") for k, v in out.items()}
        elif name == "validate9":
            r = post.validate_nine_slice(out[""], float(arg))
            results["validate9"] = r
        elif name == "states":
            states = post.derive_states(out[""])
            out = {"": states["Normal"], "_Hover": states["Hover"],
                   "_Pressed": states["Pressed"]}
    return out


def gen_component(comp, manifest, count, seed, use_cn, cn_name):
    w, h = comp["size"]
    fw, fh = comp.get("final_size", comp["size"])
    pos = manifest["style_prefix"] + ", " + comp["prompt"]
    neg = manifest["neg"]
    smp = manifest["sampler"]
    client_id = __import__("uuid").uuid4().hex

    ctrl_name = None
    if use_cn and comp.get("control"):
        c = comp["control"]
        ctrl_img = draw_controls.draw(c["draw"], w, h, **c.get("args", {}))
        os.makedirs(CTRL_DIR, exist_ok=True)
        ctrl_path = os.path.join(CTRL_DIR, comp["name"] + "_ctrl.png")
        ctrl_img.save(ctrl_path)
        ctrl_name = gc.upload_image(ctrl_path)

    saved = []
    for i in range(count):
        s = (seed + i) if seed >= 0 else int.from_bytes(os.urandom(4), "big")
        if ctrl_name:
            graph = gc.build_workflow(
                manifest["ckpt"], cn_name, ctrl_name, pos, neg, s,
                smp["steps"], smp["cfg"], smp["sampler_name"], smp["scheduler"],
                comp["control"].get("strength", 0.85), w, "uikit_" + comp["name"])
            # gen_controlnet 그래프는 정사각 latent — 비정사각 크기로 교정
            graph["5"]["inputs"]["width"] = w
            graph["5"]["inputs"]["height"] = h
        else:
            graph = gen_icon.build_workflow(
                manifest["ckpt"], pos, neg, s, smp["steps"], smp["cfg"],
                smp["sampler_name"], smp["scheduler"], w, h,
                "uikit_" + comp["name"])
        pid = gc.queue_prompt(graph, client_id)
        print("[%s %d/%d] seed=%d 생성 중..." % (comp["name"], i + 1, count, s))
        imgs = gc.wait_images(pid)
        if not imgs:
            print("  (이미지 없음)")
            continue
        img = Image.open(io.BytesIO(gc.download_image(imgs[0])))

        results = {}
        variants = apply_post(img, comp.get("post", []), comp, results)
        for suffix, v in variants.items():
            v = v.resize((fw, fh), Image.LANCZOS)
            p = os.path.join(OUT_DIR, "%s_%d%s.png" % (comp["name"], i, suffix))
            v.save(p)
            saved.append(p)
        tag = ""
        if "validate9" in results:
            r = results["validate9"]
            tag = "  [9-slice %s std=%.1f]" % ("PASS" if r["pass"] else "FAIL",
                                               r["center_std"])
        print("  저장: %s_%d (%d변형)%s" % (comp["name"], i, len(variants), tag))
    return saved


def main():
    ap = argparse.ArgumentParser(description="UI 텍스처 키트 배치 생성")
    ap.add_argument("--manifest", default=os.path.join(SCRIPT_DIR, "ui_kit_manifest.json"))
    ap.add_argument("--only", help="특정 컴포넌트만 (이름)")
    ap.add_argument("--count", type=int, default=3)
    ap.add_argument("--seed", type=int, default=-1)
    ap.add_argument("--no-cn", action="store_true", help="ControlNet 없이 vanilla 생성")
    ap.add_argument("--cn", default="diffusion_pytorch_model.safetensors")
    args = ap.parse_args()

    with open(args.manifest, encoding="utf-8-sig") as fp:
        manifest = json.load(fp)
    os.makedirs(OUT_DIR, exist_ok=True)

    use_cn = not args.no_cn
    if use_cn:
        cns = gc.list_node_options("ControlNetLoader", "control_net_name")
        if args.cn not in cns:
            print("[경고] ControlNet '%s' 없음 (%s) — vanilla 로 진행" % (args.cn, cns))
            use_cn = False

    all_saved = []
    for comp in manifest["components"]:
        if args.only and comp["name"] != args.only:
            continue
        all_saved += gen_component(comp, manifest, args.count, args.seed,
                                   use_cn, args.cn)
    print("\n완료: %d개 파일 → %s" % (len(all_saved), OUT_DIR))


if __name__ == "__main__":
    main()
