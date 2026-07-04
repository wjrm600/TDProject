#!/usr/bin/env python3
"""
gen_icon.py — ComfyUI(:8188) HTTP API 로 UI 아이콘/이미지를 생성하는 외부 스크립트.

[흐름]
  프롬프트 → ComfyUI 큐 → PNG 다운로드 → (rembg) 배경 제거(투명 알파) → RawAssets/ 저장.

[중요] 이 스크립트는 *일반 Python* 에서 실행됩니다 (ComfyUI HTTP API 호출 + rembg).
       언리얼 임포트는 별개 — 에디터 내부 Python 의 import_ui_assets.py 를
       unreal-engine MCP 의 execute_script 로 호출하세요 (그쪽엔 unreal 모듈이 있음).
       즉 생성(외부) ↔ 임포트(에디터 내부) 는 실행 컨텍스트가 다릅니다.

[사용 예]
  python gen_icon.py --name Alex_Q --count 4 \
      --prompt "decisive golden sword strike, energy burst"

[환경변수]
  COMFY_HOST  (기본 127.0.0.1:8188)  — ComfyUI 서버 주소. 포트 다르면 여기서 조정.
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

# Windows 콘솔(cp949 등)에서도 UTF-8 로 출력 — em dash/화살표/이모지 출력 시 크래시 방지.
for _stream in (sys.stdout, sys.stderr):
    try:
        _stream.reconfigure(encoding="utf-8")
    except Exception:
        pass

COMFY_HOST = os.environ.get("COMFY_HOST", "127.0.0.1:8188")
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RAW_DIR = os.path.join(SCRIPT_DIR, "RawAssets")

# 게임 통일감용 공통 스타일 프리픽스 (모든 아이콘에 결합). --style 로 교체 가능.
DEFAULT_STYLE = (
    "MOBA game skill icon, dark fantasy, centered composition, single subject, "
    "bold readable silhouette, dramatic rim light, high contrast, "
    "clean plain dark background, digital painting, game UI asset"
)
DEFAULT_NEG = (
    "text, letters, watermark, signature, ui border, frame, grid, multiple objects, "
    "blurry, lowres, jpeg artifacts, photo, busy cluttered background"
)


def build_workflow(ckpt, pos, neg, seed, steps, cfg, sampler, scheduler, w, h, prefix,
                   lora="", lora_strength=0.8):
    """ComfyUI API 포맷 그래프 (vanilla SDXL — 커스텀 노드 불필요).
    lora 지정 시 LoraLoader 노드를 삽입해 model/clip 을 거쳐가게 한다.
    ⚠️ LoRA 는 체크포인트와 같은 아키텍처여야 함 (SD1.5 LoRA ↔ SD1.5 ckpt, SDXL ↔ SDXL)."""
    model_src, clip_src = ["4", 0], ["4", 1]
    graph = {
        "4": {"class_type": "CheckpointLoaderSimple", "inputs": {"ckpt_name": ckpt}},
    }
    if lora:
        graph["10"] = {"class_type": "LoraLoader", "inputs": {
            "lora_name": lora, "strength_model": lora_strength, "strength_clip": lora_strength,
            "model": ["4", 0], "clip": ["4", 1]}}
        model_src, clip_src = ["10", 0], ["10", 1]
    graph.update({
        "6": {"class_type": "CLIPTextEncode", "inputs": {"text": pos, "clip": clip_src}},
        "7": {"class_type": "CLIPTextEncode", "inputs": {"text": neg, "clip": clip_src}},
        "5": {"class_type": "EmptyLatentImage",
              "inputs": {"width": w, "height": h, "batch_size": 1}},
        "3": {"class_type": "KSampler", "inputs": {
            "seed": seed, "steps": steps, "cfg": cfg,
            "sampler_name": sampler, "scheduler": scheduler, "denoise": 1.0,
            "model": model_src, "positive": ["6", 0], "negative": ["7", 0],
            "latent_image": ["5", 0]}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["3", 0], "vae": ["4", 2]}},
        "9": {"class_type": "SaveImage", "inputs": {"filename_prefix": prefix, "images": ["8", 0]}},
    })
    return graph


def _post(url, data):
    req = urllib.request.Request(
        url, data=json.dumps(data).encode("utf-8"),
        headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read())


def _get(url, timeout=30):
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return r.read()


def list_checkpoints():
    """설치된 체크포인트 파일명 목록 (검증/안내용). 실패 시 빈 리스트."""
    try:
        info = json.loads(_get(f"http://{COMFY_HOST}/object_info/CheckpointLoaderSimple"))
        return info["CheckpointLoaderSimple"]["input"]["required"]["ckpt_name"][0]
    except Exception:
        return []


def queue_prompt(graph, client_id):
    return _post(f"http://{COMFY_HOST}/prompt",
                 {"prompt": graph, "client_id": client_id})["prompt_id"]


def wait_images(prompt_id, timeout=300):
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            hist = json.loads(_get(f"http://{COMFY_HOST}/history/{prompt_id}"))
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
    q = urllib.parse.urlencode({
        "filename": img["filename"],
        "subfolder": img.get("subfolder", ""),
        "type": img.get("type", "output")})
    return _get(f"http://{COMFY_HOST}/view?{q}")


def remove_bg(png_bytes):
    """rembg 로 배경 제거(투명 알파). 미설치 시 원본 그대로 + 경고."""
    try:
        from rembg import remove
        return remove(png_bytes)
    except Exception as e:
        print(f"  [경고] rembg 미설치/실패 — 배경 제거 건너뜀 ({e})")
        return png_bytes


def main():
    ap = argparse.ArgumentParser(description="ComfyUI 로 UI 아이콘 생성 → RawAssets/ 저장")
    ap.add_argument("--prompt", required=True, help="아이콘 핵심 묘사 (스타일은 자동 결합)")
    ap.add_argument("--name", required=True, help="출력 파일 베이스명 (예: Alex_Q)")
    ap.add_argument("--count", type=int, default=4, help="후보 장수 (기본 4)")
    ap.add_argument("--ckpt", default="dreamshaperXL_lightningDPMSDE.safetensors",
                    help="ComfyUI/models/checkpoints/ 내 파일명")
    ap.add_argument("--style", default=DEFAULT_STYLE)
    ap.add_argument("--neg", default=DEFAULT_NEG)
    # 아래 기본값은 SDXL-Lightning(DPM++ SDE) 기준. 일반 SDXL 이면 --steps 30 --cfg 7 --scheduler normal
    ap.add_argument("--steps", type=int, default=8)
    ap.add_argument("--cfg", type=float, default=2.0)
    ap.add_argument("--sampler", default="dpmpp_sde")
    ap.add_argument("--scheduler", default="karras")
    ap.add_argument("--size", type=int, default=1024)
    ap.add_argument("--width", type=int, default=0, help="가로 해상도 (0이면 --size 사용). 전신 캐릭터는 세로 비율 권장")
    ap.add_argument("--height", type=int, default=0, help="세로 해상도 (0이면 --size 사용)")
    ap.add_argument("--seed", type=int, default=-1, help="-1 = 매 장 랜덤")
    ap.add_argument("--no-bg-removal", action="store_true", help="배경 제거 끄기")
    ap.add_argument("--lora", default="", help="loras/ 내 LoRA 파일명 (지정 시 적용). ckpt 와 같은 아키텍처여야 함")
    ap.add_argument("--lora-strength", type=float, default=0.8, help="LoRA 강도 (model+clip 공통)")
    args = ap.parse_args()

    os.makedirs(RAW_DIR, exist_ok=True)

    # 연결 + 체크포인트 검증 (친절한 안내)
    available = list_checkpoints()
    if available and args.ckpt not in available:
        print(f"[오류] 체크포인트 '{args.ckpt}' 를 찾을 수 없습니다.")
        print("  ComfyUI/models/checkpoints/ 에 설치된 파일:")
        for c in available:
            print(f"    - {c}")
        print("  → --ckpt <위 파일명> 으로 지정하세요.")
        sys.exit(1)
    if not available:
        print(f"[경고] ComfyUI({COMFY_HOST}) 응답 없음 — 서버 미실행이면 곧 연결 오류가 납니다.")

    client_id = uuid.uuid4().hex
    pos = f"{args.prompt}, {args.style}"
    saved = []

    for i in range(args.count):
        # --seed 고정 시: 첫 장은 그 시드, 이후는 seed+1, seed+2... (좋아한 구도 주변 탐색)
        seed = (args.seed + i) if args.seed >= 0 else int.from_bytes(os.urandom(4), "big")
        graph = build_workflow(args.ckpt, pos, args.neg, seed, args.steps, args.cfg,
                               args.sampler, args.scheduler,
                               (args.width or args.size), (args.height or args.size),
                               f"tdproj_{args.name}", args.lora, args.lora_strength)
        try:
            pid = queue_prompt(graph, client_id)
        except urllib.error.URLError as e:
            print(f"[오류] ComfyUI 연결 실패 ({COMFY_HOST}). "
                  f"ComfyUI 서버(run_nvidia_gpu.bat)가 실행 중인지 확인.\n  {e}")
            sys.exit(1)
        print(f"[{i + 1}/{args.count}] seed={seed} 생성 중...")
        imgs = wait_images(pid)
        if not imgs:
            print("  (이미지 없음 — 체크포인트명/워크플로 확인)")
            continue
        data = download_image(imgs[0])
        if not args.no_bg_removal:
            data = remove_bg(data)
        out = os.path.join(RAW_DIR, f"{args.name}_{i}.png")
        with open(out, "wb") as f:
            f.write(data)
        saved.append(out)
        print(f"  저장: {out}")

    if not saved:
        print("\n생성된 파일이 없습니다.")
        sys.exit(1)

    print("\n완료. 후보 파일:")
    for s in saved:
        print(f"  {s}")
    print("\n[다음] 마음에 드는 1장을 고른 뒤, 언리얼 MCP execute_script 로 임포트:")
    print(f"  import_ui_assets.py <고른 png 절대경로> /Game/AOS/UI/Assets")


if __name__ == "__main__":
    main()
