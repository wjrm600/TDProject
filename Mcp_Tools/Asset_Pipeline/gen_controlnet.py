#!/usr/bin/env python3
"""
gen_controlnet.py — 스케치를 ControlNet(scribble) 제어 신호로 쓰고 denoise=1.0 으로 완전 렌더.

img2img 와 차이: img2img 는 소스 픽셀을 부분만 바꿔(denoise<1) 외곽선이 남지만,
ControlNet 은 빈 latent 에서 완전히 새로 그리되(denoise=1) 스케치 "선"을 구조로 강제 →
**구도는 스케치대로 잠그고 + 칼날은 솔리드하게 채움.**

제어 이미지 형식: scribble 은 보통 "검은 배경 + 흰 선" → ref_q_inv.png 사용 권장.
strength 1.0=강하게 구도 강제, 0.7=느슨.

사용 예:
  python gen_controlnet.py --ctrl RawAssets/ref_q_inv.png --name Alex_Q --count 6 \
      --ckpt illustriousXL_v01.safetensors --cn diffusion_pytorch_model.safetensors \
      --strength 0.9 --prompt "..." --neg "..."
"""

import argparse
import json
import os
import sys
import time
import uuid
import urllib.request
import urllib.error
import urllib.parse

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8")
    except Exception:
        pass

COMFY_HOST = os.environ.get("COMFY_HOST", "127.0.0.1:8188")
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RAW_DIR = os.path.join(SCRIPT_DIR, "RawAssets")


def _post(url, data):
    req = urllib.request.Request(url, data=json.dumps(data).encode("utf-8"),
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read())


def _get(url, timeout=30):
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return r.read()


def upload_image(path):
    boundary = "----comfy" + uuid.uuid4().hex
    fname = os.path.basename(path)
    with open(path, "rb") as f:
        filedata = f.read()
    parts = [
        ("--" + boundary).encode(),
        ('Content-Disposition: form-data; name="image"; filename="%s"' % fname).encode(),
        b"Content-Type: image/png", b"", filedata,
        ("--" + boundary).encode(),
        b'Content-Disposition: form-data; name="overwrite"', b"", b"true",
        ("--" + boundary + "--").encode(),
    ]
    body = b"\r\n".join(parts) + b"\r\n"
    req = urllib.request.Request("http://%s/upload/image" % COMFY_HOST, data=body,
                                 headers={"Content-Type": "multipart/form-data; boundary=" + boundary})
    with urllib.request.urlopen(req, timeout=60) as r:
        res = json.loads(r.read())
    name = res.get("name", fname)
    sub = res.get("subfolder", "")
    return (sub + "/" + name) if sub else name


def list_node_options(node, field):
    try:
        info = json.loads(_get("http://%s/object_info/%s" % (COMFY_HOST, node)))
        return info[node]["input"]["required"][field][0]
    except Exception:
        return []


def build_workflow(ckpt, cn_name, image_name, pos, neg, seed, steps, cfg,
                   sampler, scheduler, strength, size, prefix):
    return {
        "4":  {"class_type": "CheckpointLoaderSimple", "inputs": {"ckpt_name": ckpt}},
        "6":  {"class_type": "CLIPTextEncode", "inputs": {"text": pos, "clip": ["4", 1]}},
        "7":  {"class_type": "CLIPTextEncode", "inputs": {"text": neg, "clip": ["4", 1]}},
        "10": {"class_type": "LoadImage", "inputs": {"image": image_name}},
        "12": {"class_type": "ControlNetLoader", "inputs": {"control_net_name": cn_name}},
        "13": {"class_type": "ControlNetApplyAdvanced", "inputs": {
            "positive": ["6", 0], "negative": ["7", 0], "control_net": ["12", 0],
            "image": ["10", 0], "strength": strength, "start_percent": 0.0, "end_percent": 1.0}},
        "5":  {"class_type": "EmptyLatentImage", "inputs": {"width": size, "height": size, "batch_size": 1}},
        "3":  {"class_type": "KSampler", "inputs": {
            "seed": seed, "steps": steps, "cfg": cfg, "sampler_name": sampler,
            "scheduler": scheduler, "denoise": 1.0,
            "model": ["4", 0], "positive": ["13", 0], "negative": ["13", 1], "latent_image": ["5", 0]}},
        "8":  {"class_type": "VAEDecode", "inputs": {"samples": ["3", 0], "vae": ["4", 2]}},
        "9":  {"class_type": "SaveImage", "inputs": {"filename_prefix": prefix, "images": ["8", 0]}},
    }


def queue_prompt(graph, client_id):
    return _post("http://%s/prompt" % COMFY_HOST, {"prompt": graph, "client_id": client_id})["prompt_id"]


def wait_images(prompt_id, timeout=300):
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            hist = json.loads(_get("http://%s/history/%s" % (COMFY_HOST, prompt_id)))
        except urllib.error.URLError:
            hist = {}
        if prompt_id in hist:
            imgs = []
            for node in hist[prompt_id].get("outputs", {}).values():
                imgs.extend(node.get("images", []))
            return imgs
        time.sleep(1.0)
    raise TimeoutError("ComfyUI 생성 타임아웃 (300s)")


def download_image(img):
    q = urllib.parse.urlencode({"filename": img["filename"], "subfolder": img.get("subfolder", ""),
                                "type": img.get("type", "output")})
    return _get("http://%s/view?%s" % (COMFY_HOST, q))


def remove_bg(png_bytes):
    try:
        from rembg import remove
        return remove(png_bytes)
    except Exception as e:
        print("  [경고] rembg 미설치/실패 — 배경 제거 건너뜀 (%s)" % e)
        return png_bytes


def main():
    ap = argparse.ArgumentParser(description="ComfyUI ControlNet(scribble) 렌더")
    ap.add_argument("--ctrl", required=True, help="제어(스케치) 이미지 경로 — scribble 은 흰선/검은배경 권장")
    ap.add_argument("--prompt", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--count", type=int, default=6)
    ap.add_argument("--ckpt", default="illustriousXL_v01.safetensors")
    ap.add_argument("--cn", default="diffusion_pytorch_model.safetensors", help="ControlNet 모델 파일명")
    ap.add_argument("--neg", default="worst quality, low quality, lineart, outline, white background, person, text")
    ap.add_argument("--strength", type=float, default=0.9, help="1.0=강하게 구도 강제, 0.7=느슨")
    ap.add_argument("--cfg", type=float, default=6.0)
    ap.add_argument("--steps", type=int, default=30)
    ap.add_argument("--sampler", default="euler_ancestral")
    ap.add_argument("--scheduler", default="normal")
    ap.add_argument("--size", type=int, default=1024)
    ap.add_argument("--seed", type=int, default=-1)
    ap.add_argument("--no-bg-removal", action="store_true")
    args = ap.parse_args()

    ctrl = args.ctrl if os.path.isabs(args.ctrl) else os.path.join(SCRIPT_DIR, args.ctrl)
    if not os.path.exists(ctrl):
        print("[오류] 제어 이미지 없음: %s" % ctrl)
        sys.exit(1)

    os.makedirs(RAW_DIR, exist_ok=True)

    cns = list_node_options("ControlNetLoader", "control_net_name")
    if cns and args.cn not in cns:
        print("[오류] ControlNet '%s' 없음. 설치된 목록:" % args.cn)
        for c in cns:
            print("    -", c)
        sys.exit(1)

    try:
        ctrl_name = upload_image(ctrl)
        print("제어 이미지 업로드: %s" % ctrl_name)
    except Exception as e:
        print("[오류] 제어 이미지 업로드 실패: %s" % e)
        sys.exit(1)

    client_id = uuid.uuid4().hex
    saved = []
    for i in range(args.count):
        seed = (args.seed + i) if args.seed >= 0 else int.from_bytes(os.urandom(4), "big")
        graph = build_workflow(args.ckpt, args.cn, ctrl_name, args.prompt, args.neg, seed,
                               args.steps, args.cfg, args.sampler, args.scheduler,
                               args.strength, args.size, "tdproj_%s" % args.name)
        try:
            pid = queue_prompt(graph, client_id)
        except urllib.error.URLError as e:
            print("[오류] ComfyUI 연결 실패 (%s): %s" % (COMFY_HOST, e))
            sys.exit(1)
        print("[%d/%d] seed=%d strength=%.2f 생성 중..." % (i + 1, args.count, seed, args.strength))
        imgs = wait_images(pid)
        if not imgs:
            print("  (이미지 없음)")
            continue
        data = download_image(imgs[0])
        if not args.no_bg_removal:
            data = remove_bg(data)
        out = os.path.join(RAW_DIR, "%s_%d.png" % (args.name, i))
        with open(out, "wb") as f:
            f.write(data)
        saved.append(out)
        print("  저장: %s" % out)

    if not saved:
        print("\n생성된 파일 없음.")
        sys.exit(1)
    print("\n완료. 후보:")
    for s in saved:
        print("  %s" % s)


if __name__ == "__main__":
    main()
