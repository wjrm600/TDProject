#!/usr/bin/env python3
"""
gen_img2img.py — 소스 이미지를 ComfyUI img2img 로 변형 (구도 유지 + 프롬프트로 수정).

흐름: 소스 PNG 업로드(/upload/image) → LoadImage → VAEEncode → KSampler(denoise<1) → 저장.
  --denoise 가 클수록 원본에서 더 많이 바뀜 (0.4=살짝, 0.6=중간, 0.75=많이/거의 새로).

사용 예:
  python gen_img2img.py --src RawAssets/Alex_Q_2.png --name Alex_Q --count 6 \
      --ckpt illustriousXL_v01.safetensors --denoise 0.7 \
      --prompt "..." --neg "..."
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

# Windows 콘솔(cp949)에서도 UTF-8 출력
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
    """소스 이미지를 ComfyUI /upload/image 로 업로드 → LoadImage 가 참조할 이름 반환."""
    boundary = "----comfy" + uuid.uuid4().hex
    fname = os.path.basename(path)
    with open(path, "rb") as f:
        filedata = f.read()
    parts = [
        ("--" + boundary).encode(),
        ('Content-Disposition: form-data; name="image"; filename="%s"' % fname).encode(),
        b"Content-Type: image/png",
        b"",
        filedata,
        ("--" + boundary).encode(),
        b'Content-Disposition: form-data; name="overwrite"',
        b"",
        b"true",
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


def list_checkpoints():
    try:
        info = json.loads(_get("http://%s/object_info/CheckpointLoaderSimple" % COMFY_HOST))
        return info["CheckpointLoaderSimple"]["input"]["required"]["ckpt_name"][0]
    except Exception:
        return []


def build_workflow(ckpt, image_name, pos, neg, seed, steps, cfg, sampler, scheduler, denoise, prefix):
    """img2img API 그래프 — EmptyLatentImage 대신 LoadImage→VAEEncode 로 소스 latent 사용."""
    return {
        "4":  {"class_type": "CheckpointLoaderSimple", "inputs": {"ckpt_name": ckpt}},
        "10": {"class_type": "LoadImage", "inputs": {"image": image_name}},
        "11": {"class_type": "VAEEncode", "inputs": {"pixels": ["10", 0], "vae": ["4", 2]}},
        "6":  {"class_type": "CLIPTextEncode", "inputs": {"text": pos, "clip": ["4", 1]}},
        "7":  {"class_type": "CLIPTextEncode", "inputs": {"text": neg, "clip": ["4", 1]}},
        "3":  {"class_type": "KSampler", "inputs": {
            "seed": seed, "steps": steps, "cfg": cfg,
            "sampler_name": sampler, "scheduler": scheduler, "denoise": denoise,
            "model": ["4", 0], "positive": ["6", 0], "negative": ["7", 0],
            "latent_image": ["11", 0]}},
        "8":  {"class_type": "VAEDecode", "inputs": {"samples": ["3", 0], "vae": ["4", 2]}},
        "9":  {"class_type": "SaveImage", "inputs": {"filename_prefix": prefix, "images": ["8", 0]}},
    }


def queue_prompt(graph, client_id):
    return _post("http://%s/prompt" % COMFY_HOST,
                 {"prompt": graph, "client_id": client_id})["prompt_id"]


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
    q = urllib.parse.urlencode({"filename": img["filename"],
                                "subfolder": img.get("subfolder", ""),
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
    ap = argparse.ArgumentParser(description="ComfyUI img2img — 소스 이미지 변형")
    ap.add_argument("--src", required=True, help="소스 이미지 경로 (RawAssets/Alex_Q_2.png 등)")
    ap.add_argument("--prompt", required=True)
    ap.add_argument("--name", required=True, help="출력 베이스명")
    ap.add_argument("--count", type=int, default=6)
    ap.add_argument("--ckpt", default="illustriousXL_v01.safetensors")
    ap.add_argument("--neg", default="worst quality, low quality, jpeg artifacts, person, human, text, watermark")
    ap.add_argument("--denoise", type=float, default=0.7, help="0.4=살짝 0.6=중간 0.75=많이")
    ap.add_argument("--cfg", type=float, default=6.0)
    ap.add_argument("--steps", type=int, default=30)
    ap.add_argument("--sampler", default="euler_ancestral")
    ap.add_argument("--scheduler", default="normal")
    ap.add_argument("--seed", type=int, default=-1)
    ap.add_argument("--no-bg-removal", action="store_true")
    args = ap.parse_args()

    src = args.src if os.path.isabs(args.src) else os.path.join(SCRIPT_DIR, args.src)
    if not os.path.exists(src):
        print("[오류] 소스 이미지 없음: %s" % src)
        sys.exit(1)

    os.makedirs(RAW_DIR, exist_ok=True)

    available = list_checkpoints()
    if available and args.ckpt not in available:
        print("[오류] 체크포인트 '%s' 없음. 설치된 목록:" % args.ckpt)
        for c in available:
            print("    -", c)
        sys.exit(1)
    if not available:
        print("[경고] ComfyUI(%s) 응답 없음 — 서버 미실행이면 곧 오류." % COMFY_HOST)

    try:
        image_name = upload_image(src)
        print("소스 업로드 완료: %s" % image_name)
    except Exception as e:
        print("[오류] 소스 업로드 실패: %s" % e)
        sys.exit(1)

    client_id = uuid.uuid4().hex
    saved = []
    for i in range(args.count):
        seed = (args.seed + i) if args.seed >= 0 else int.from_bytes(os.urandom(4), "big")
        graph = build_workflow(args.ckpt, image_name, args.prompt, args.neg, seed,
                               args.steps, args.cfg, args.sampler, args.scheduler,
                               args.denoise, "tdproj_%s" % args.name)
        try:
            pid = queue_prompt(graph, client_id)
        except urllib.error.URLError as e:
            print("[오류] ComfyUI 연결 실패 (%s): %s" % (COMFY_HOST, e))
            sys.exit(1)
        print("[%d/%d] seed=%d denoise=%.2f 생성 중..." % (i + 1, args.count, seed, args.denoise))
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
