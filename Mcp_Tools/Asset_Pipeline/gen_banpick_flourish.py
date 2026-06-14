#!/usr/bin/env python3
"""gen_banpick_flourish.py — 벤픽 타이머/제목 옆 날개형 플러리시 장식 생성.

1차: ComfyUI(:8188) 로 클린 라인아트 플러리시 생성 (white on black) → 휘도→알파 추출.
폴백: ComfyUI 미가동/실패 시 PIL 기하 플러리시(베지어 swoosh 3겹 페더 팬) — 플랫 라이트
테마에 어울리는 깨끗한 곡선. 어느 쪽이든 출력은 white-on-alpha (위젯에서 틴트).

  python gen_banpick_flourish.py            # ComfyUI 시도 → 실패 시 자동 폴백
  python gen_banpick_flourish.py --fallback # 기하 폴백 강제
→ RawAssets/T_BanPick_Flourish.png (512x256, 날개가 왼쪽으로 펼쳐짐 — 우측은 위젯에서 미러)
"""
import argparse
import json
import math
import os
import sys
import time
import uuid
import urllib.error
import urllib.parse
import urllib.request

from PIL import Image, ImageDraw

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8")
    except Exception:
        pass

HOST = os.environ.get("COMFY_HOST", "127.0.0.1:8188")
RAW = os.path.join(os.path.dirname(os.path.abspath(__file__)), "RawAssets")
OUT_W, OUT_H = 512, 256

POS = ("elegant ornamental flourish, swirling calligraphic wing decoration, "
       "clean vector line art, thin smooth curves, symmetric swoosh, "
       "pure white lines on pure black background, minimal flat design, ornament asset")
NEG = ("text, watermark, photo, 3d render, color, gradient background, noise, "
       "blurry, frame, border, face, person")


# ── ComfyUI helpers (gen_banpick_backdrop.py 패턴) ─────────────
def post(url, data):
    req = urllib.request.Request(url, data=json.dumps(data).encode("utf-8"),
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read())


def get(url, t=60):
    with urllib.request.urlopen(url, timeout=t) as r:
        return r.read()


def list_ckpts():
    try:
        info = json.loads(get(f"http://{HOST}/object_info/CheckpointLoaderSimple"))
        return info["CheckpointLoaderSimple"]["input"]["required"]["ckpt_name"][0]
    except Exception:
        return []


def comfy_generate(out_base="T_BanPick_Flourish"):
    """ComfyUI 로 1024x512 라인아트 생성 → PIL Image (RGB) 반환. 실패 시 None."""
    avail = list_ckpts()
    if not avail:
        print("[정보] ComfyUI 응답 없음 → 기하 폴백 사용")
        return None
    pick = ([c for c in avail if "dream" in c.lower()]
            or [c for c in avail if "xl" in c.lower()] or avail)
    ckpt = pick[0]
    low = ckpt.lower()
    if "lightning" in low or "light" in low or "turbo" in low:
        steps, cfg, sampler, sched = 8, 2.0, "dpmpp_sde", "karras"
    else:
        steps, cfg, sampler, sched = 28, 6.5, "dpmpp_2m", "karras"
    seed = int.from_bytes(os.urandom(4), "big")
    print(f"ComfyUI ckpt={ckpt} steps={steps} cfg={cfg} seed={seed}")

    graph = {
        "4": {"class_type": "CheckpointLoaderSimple", "inputs": {"ckpt_name": ckpt}},
        "6": {"class_type": "CLIPTextEncode", "inputs": {"text": POS, "clip": ["4", 1]}},
        "7": {"class_type": "CLIPTextEncode", "inputs": {"text": NEG, "clip": ["4", 1]}},
        "5": {"class_type": "EmptyLatentImage", "inputs": {"width": 1024, "height": 512, "batch_size": 1}},
        "3": {"class_type": "KSampler", "inputs": {
            "seed": seed, "steps": steps, "cfg": cfg, "sampler_name": sampler,
            "scheduler": sched, "denoise": 1.0,
            "model": ["4", 0], "positive": ["6", 0], "negative": ["7", 0], "latent_image": ["5", 0]}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["3", 0], "vae": ["4", 2]}},
        "9": {"class_type": "SaveImage", "inputs": {"filename_prefix": "tdproj_BanPickFlourish", "images": ["8", 0]}},
    }
    try:
        pid = post(f"http://{HOST}/prompt", {"prompt": graph, "client_id": uuid.uuid4().hex})["prompt_id"]
    except urllib.error.URLError as e:
        print(f"[정보] ComfyUI 연결 실패({e}) → 기하 폴백 사용")
        return None

    t0 = time.time()
    imgs = []
    while time.time() - t0 < 180:
        try:
            hist = json.loads(get(f"http://{HOST}/history/{pid}"))
        except urllib.error.URLError:
            hist = {}
        if pid in hist:
            for node in hist[pid].get("outputs", {}).values():
                imgs.extend(node.get("images", []))
            break
        time.sleep(1.0)
    if not imgs:
        print("[정보] 생성 타임아웃 → 기하 폴백 사용")
        return None

    q = urllib.parse.urlencode({"filename": imgs[0]["filename"],
                                "subfolder": imgs[0].get("subfolder", ""),
                                "type": imgs[0].get("type", "output")})
    data = get(f"http://{HOST}/view?{q}")
    raw_path = os.path.join(RAW, f"{out_base}_gen.png")
    with open(raw_path, "wb") as f:
        f.write(data)
    print("raw saved:", raw_path)
    import io
    return Image.open(io.BytesIO(data)).convert("RGB")


# ── PIL 기하 폴백: 베지어 swoosh 3겹 페더 팬 ─────────────
def quad_bezier(p0, p1, p2, t):
    mt = 1.0 - t
    x = mt * mt * p0[0] + 2 * mt * t * p1[0] + t * t * p2[0]
    y = mt * mt * p0[1] + 2 * mt * t * p1[1] + t * t * p2[1]
    return x, y


def geometric_flourish():
    """4x 슈퍼샘플 캔버스에 테이퍼 swoosh 3개 → 다운스케일 AA. white-on-black RGB 반환."""
    S = 4
    W, H = OUT_W * S, OUT_H * S
    img = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(img)

    # 오른쪽(제목 쪽)에서 왼쪽으로 펼쳐지는 페더 팬 — 위/중/아래 3곡선
    anchor = (W * 0.96, H * 0.62)
    curves = [
        # (끝점, 컨트롤, 시작폭px@1x, 끝폭px@1x)
        ((W * 0.04, H * 0.18), (W * 0.42, H * 0.02), 5.0, 0.8),
        ((W * 0.02, H * 0.50), (W * 0.40, H * 0.34), 6.0, 1.0),
        ((W * 0.10, H * 0.82), (W * 0.46, H * 0.74), 4.5, 0.7),
    ]
    for (end, ctrl, w0, w1) in curves:
        n = 1600
        for i in range(n + 1):
            t = i / n
            x, y = quad_bezier(anchor, ctrl, end, t)
            r = (w0 * (1.0 - t) + w1 * t) * S / 2.0
            d.ellipse([x - r, y - r, x + r, y + r], fill=255)

    # 앵커 포인트 장식: 작은 다이아몬드
    cx, cy = anchor
    ds = 9.0 * S / 2.0
    d.polygon([(cx, cy - ds), (cx + ds, cy), (cx, cy + ds), (cx - ds, cy)], fill=255)

    img = img.resize((OUT_W, OUT_H), Image.LANCZOS)
    return Image.merge("RGB", (img, img, img))


def main():
    global POS
    ap = argparse.ArgumentParser()
    ap.add_argument("--fallback", action="store_true", help="기하 폴백 강제")
    ap.add_argument("--prompt", default=None, help="포지티브 프롬프트 교체 (기본: 플러리시)")
    ap.add_argument("--out", default="T_BanPick_Flourish", help="출력 베이스명 (raw=<out>_gen.png)")
    args = ap.parse_args()
    if args.prompt:
        POS = args.prompt

    os.makedirs(RAW, exist_ok=True)

    rgb = None
    if not args.fallback:
        rgb = comfy_generate(args.out)
    if rgb is None:
        print("기하 플러리시 생성 (PIL)")
        rgb = geometric_flourish()
    else:
        rgb = rgb.resize((OUT_W, OUT_H), Image.LANCZOS)

    # 휘도 → 알파 (black bg 제거), RGB 는 순백 (위젯 틴트용)
    lum = rgb.convert("L")
    white = Image.new("L", (OUT_W, OUT_H), 255)
    out_img = Image.merge("RGBA", (white, white, white, lum))

    out = os.path.join(RAW, f"{args.out}.png")
    out_img.save(out)
    print("final saved:", out)


if __name__ == "__main__":
    main()
