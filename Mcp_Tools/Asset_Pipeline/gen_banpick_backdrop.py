#!/usr/bin/env python3
"""gen_banpick_backdrop.py — ComfyUI(:8188)로 벤픽 드라마틱 배경(16:9) 생성 + PIL 후처리.

흐름: ComfyUI text2img(웅장한 다크 판타지 홀) → PIL 어둡게+비네팅(UI 가독성) → T_BanPick_Backdrop.png
인라인 실행(에이전트 무관). 임포트는 별도(MCP).
  python gen_banpick_backdrop.py
"""
import json, os, sys, time, uuid, math
import urllib.request, urllib.error, urllib.parse
from PIL import Image, ImageDraw, ImageFilter, ImageEnhance

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8")
    except Exception:
        pass

HOST = os.environ.get("COMFY_HOST", "127.0.0.1:8188")
RAW = os.path.join(os.path.dirname(os.path.abspath(__file__)), "RawAssets")
W, H = 1344, 768   # 16:9 (SDXL 친화 해상도)

POS = ("grand dark fantasy throne hall, colossal stone pillars and gothic arches, "
       "distant torch fire and glowing blue magic runes, hanging banners, "
       "cinematic volumetric god rays, deep shadows, epic moody atmosphere, "
       "desaturated muted colors, highly detailed digital painting, wide establishing shot")
NEG = ("text, watermark, signature, people, characters, person, face, hands, "
       "ui, hud, frame, border, bright, oversaturated, blurry, lowres, cartoon, anime")


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


def main():
    os.makedirs(RAW, exist_ok=True)
    avail = list_ckpts()
    print("available checkpoints:", avail)
    if not avail:
        print("[오류] ComfyUI 응답 없음 — 서버 확인"); sys.exit(1)

    # 체크포인트 선택: dreamshaper > xl > 첫번째
    pick = ([c for c in avail if "dream" in c.lower()]
            or [c for c in avail if "xl" in c.lower()] or avail)
    ckpt = pick[0]
    low = ckpt.lower()
    if "lightning" in low or "light" in low or "turbo" in low:
        steps, cfg, sampler, sched = 8, 2.0, "dpmpp_sde", "karras"
    else:
        steps, cfg, sampler, sched = 28, 6.5, "dpmpp_2m", "karras"
    print(f"using ckpt={ckpt}  steps={steps} cfg={cfg} {sampler}/{sched}")

    seed = int.from_bytes(os.urandom(4), "big")
    graph = {
        "4": {"class_type": "CheckpointLoaderSimple", "inputs": {"ckpt_name": ckpt}},
        "6": {"class_type": "CLIPTextEncode", "inputs": {"text": POS, "clip": ["4", 1]}},
        "7": {"class_type": "CLIPTextEncode", "inputs": {"text": NEG, "clip": ["4", 1]}},
        "5": {"class_type": "EmptyLatentImage", "inputs": {"width": W, "height": H, "batch_size": 1}},
        "3": {"class_type": "KSampler", "inputs": {
            "seed": seed, "steps": steps, "cfg": cfg, "sampler_name": sampler,
            "scheduler": sched, "denoise": 1.0,
            "model": ["4", 0], "positive": ["6", 0], "negative": ["7", 0], "latent_image": ["5", 0]}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["3", 0], "vae": ["4", 2]}},
        "9": {"class_type": "SaveImage", "inputs": {"filename_prefix": "tdproj_BanPickBg", "images": ["8", 0]}},
    }
    cid = uuid.uuid4().hex
    print(f"generating ({W}x{H}, seed={seed})...")
    try:
        pid = post(f"http://{HOST}/prompt", {"prompt": graph, "client_id": cid})["prompt_id"]
    except urllib.error.URLError as e:
        print(f"[오류] ComfyUI 연결 실패: {e}"); sys.exit(1)

    t0 = time.time(); imgs = []
    while time.time() - t0 < 300:
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
        print("[오류] 이미지 생성 실패/타임아웃"); sys.exit(1)

    q = urllib.parse.urlencode({"filename": imgs[0]["filename"],
                                "subfolder": imgs[0].get("subfolder", ""),
                                "type": imgs[0].get("type", "output")})
    data = get(f"http://{HOST}/view?{q}")
    raw_path = os.path.join(RAW, "T_BanPick_Backdrop_gen.png")
    with open(raw_path, "wb") as f:
        f.write(data)
    print("raw saved:", raw_path)

    # ── PIL 후처리: 어둡게 + 탈색 + 비네팅 (UI 텍스트 가독성)
    img = Image.open(raw_path).convert("RGB").resize((W, H))
    img = ImageEnhance.Brightness(img).enhance(0.60)
    img = ImageEnhance.Color(img).enhance(0.82)
    # 비네팅 마스크 (저해상도 생성 후 확대+블러 — 빠름)
    vw, vh = 336, 192
    vig = Image.new("L", (vw, vh), 0)
    vp = vig.load()
    cx, cy = vw / 2, vh / 2
    maxr = math.hypot(cx, cy)
    for y in range(vh):
        for x in range(vw):
            d = math.hypot(x - cx, y - cy) / maxr
            vp[x, y] = int(255 * max(0.0, 1.0 - d * d * 1.05))
    vig = vig.resize((W, H)).filter(ImageFilter.GaussianBlur(24))
    dark = Image.new("RGB", (W, H), (2, 3, 7))
    img = Image.composite(img, dark, vig)
    out = os.path.join(RAW, "T_BanPick_Backdrop.png")
    img.save(out)
    print("final saved:", out)


if __name__ == "__main__":
    main()
