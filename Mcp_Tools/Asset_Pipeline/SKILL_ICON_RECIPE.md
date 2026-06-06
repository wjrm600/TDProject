# Alex 스킬 아이콘 — 확정 레시피 (세트 통일감 유지용)

스케치 → ControlNet(scribble) → 완성 아이콘. img2img/text2img로는 "구도 잠금 + 솔리드 렌더"를
동시에 못 해서 ControlNet으로 확정 (2026-06-05 검증 완료).

## ✅ 확정 현황
| 스킬 | 상태 | 파일 | 시드 |
|------|------|------|------|
| Q (DecisiveStrike — 대검 내려치기 + 위쪽 흰 트레일) | ✅ 확정 | `RawAssets/Alex_Q_FINAL.png` | 1754108806 |
| W (Courage — 방패/방어 오라) | ⬜ 스케치 대기 | | |
| E (Judgment — 회전 검풍) | ⬜ 스케치 대기 | | |
| R (DemacianJustice — 하늘의 거대검 처형) | ⬜ 스케치 대기 | | |

## 🔒 고정 파이프라인 (모든 스킬 동일)
- 스크립트: `gen_controlnet.py`
- 체크포인트: `animagineXL40_v4Opt.safetensors`
- ControlNet: `diffusion_pytorch_model.safetensors` (scribble SDXL, xinsir)
- strength **0.9**, cfg **6**, steps **30**, sampler **euler_ancestral**, scheduler **normal**, size **1024**

## 🔒 고정 스타일 프리픽스 (프롬프트 공통 앞부분)
```
masterpiece, best quality, amazing quality, fully rendered, solid metal blade,
detailed shading, no humans, weapon focus, <스킬별 내용>, dramatic lighting,
dynamic, dark background
```

## 🔒 고정 네거티브
```
worst quality, low quality, lineart, outline, monochrome, sketch, white background,
multiple swords, person, human, hands, text, watermark, blurry
```

## 스케치 전처리 (각 스킬 공통 3단계)
1. 손스케치 `ref_<skill>.png` (검은 선 / 흰 배경) 를 `RawAssets/` 에 저장
2. 정사각 패딩 → `ref_<skill>_sq.png`, 색 반전 → `ref_<skill>_inv.png`
   (pillow: pad to max-side square 1024, then ImageOps.invert)
3. `python gen_controlnet.py --ctrl RawAssets/ref_<skill>_inv.png --name Alex_<skill> ...`

## 메모
- ControlNet 제어 이미지는 **반전본(흰 선/검은 배경)** = scribble 표준 형식.
- 트레일을 또렷이 하려면 프롬프트에 `(bright glowing white energy slash trail arcing above the sword:1.3~1.4)`.
- 좋아한 시드 고정(`--seed N`) 후 `--count`로 주변 변형 탐색(seed, seed+1...) 가능.
- 배경 투명 필요 시 `pip install rembg` (현재 미설치 — 스킬 아이콘은 검은 배경이라 불필요).
