# Asset Pipeline — AI 이미지 생성 → 언리얼 자동 임포트

클로드 데스크탑 프롬프트 → **로컬 ComfyUI(무료, GTX 3080)** 생성 → 배경 제거 → 언리얼 임포트.

```
gen_icon.py (외부 Python)                     import_ui_assets.py (에디터 내부 Python)
프롬프트 → ComfyUI :8188 → PNG → rembg   ──►   /Game/AOS/UI/Assets 로 임포트 + UI 텍스처 세팅
   → RawAssets/<name>_0..N.png                  (unreal-engine MCP execute_script 로 호출)
```

> **두 단계는 실행 컨텍스트가 다릅니다.** 생성은 일반 Python(HTTP), 임포트는 에디터 내부
> Python(`unreal` 모듈). gen_icon.py 가 PNG 만 만들고, 임포트는 MCP 로 별도 호출합니다.

---

## 1회 셋업 (Windows + RTX 3080, 무료)

### A. ComfyUI 설치 (서버 :8188)
1. **ComfyUI 포터블(NVIDIA)** 다운로드: GitHub `comfyanonymous/ComfyUI` → Releases →
   `ComfyUI_windows_portable_nvidia.7z` (Python·PyTorch·CUDA 번들).
2. 압축 해제 → `run_nvidia_gpu.bat` 실행 → 브라우저 `127.0.0.1:8188` 떠야 정상.
   - 포트가 다르면 `set COMFY_HOST=127.0.0.1:<포트>` 로 gen_icon.py 에 알려주세요.

### B. SDXL 체크포인트 (게임 아이콘용, 무료)
- **권장: DreamShaper XL (Lightning DPM++ SDE)** — Civitai 무료, 판타지/게임아트 강함,
  4~8 step 고속(3080 에서 장당 ~2~4초). ~6.5GB.
- 받은 `.safetensors` 를 `ComfyUI_windows_portable/ComfyUI/models/checkpoints/` 에 복사.
- 파일명을 `--ckpt` 로 넘기세요 (gen_icon.py 가 설치된 목록을 검증/안내함).
- 대안: Juggernaut XL(사실적), SDXL base 1.0(바닐라 → `--steps 30 --cfg 7 --scheduler normal`).

### C. 배경 제거(rembg) — 투명 알파 PNG
```
pip install -r requirements.txt
```
(첫 실행 시 u2net 모델 ~170MB 자동 다운로드)

---

## 사용

### 1) 생성 (외부 Python)
```
python gen_icon.py --name Alex_Q --count 4 ^
    --prompt "decisive golden sword strike, energy burst"
```
→ `RawAssets/Alex_Q_0.png ... Alex_Q_3.png` (투명 배경 후보 4장)

빠른 이터레이션은 Lightning 기본값(8 step), 일반 SDXL 이면 `--steps 30 --cfg 7 --scheduler normal`.

### 2) 임포트 (언리얼 MCP — 에디터 실행 중)
마음에 드는 1장을 골라 `unreal-engine` MCP `execute_script` 로:
```
import_ui_assets.py  <RawAssets/Alex_Q_2.png 절대경로>  /Game/AOS/UI/Assets
```
→ `/Game/AOS/UI/Assets/Alex_Q_2` 텍스처 생성 (UI 그룹/NoMipmap/EditorIcon 압축 자동).

### 3) 바인딩
`agent-art-visual` / `agent-prog-ui` 에 에셋 경로를 넘겨 위젯·머티리얼에 연결.

---

## 옵션 (gen_icon.py)
| 인자 | 기본값 | 설명 |
|------|--------|------|
| `--prompt` | (필수) | 아이콘 핵심 묘사. 공통 스타일은 자동 결합 |
| `--name` | (필수) | 출력 베이스명 (예: `Item_Sword`) |
| `--count` | 4 | 후보 장수 |
| `--ckpt` | dreamshaperXL_lightningDPMSDE.safetensors | 체크포인트 파일명 |
| `--style` / `--neg` | 게임 아이콘 프리셋 | 통일감용 공통 프롬프트 |
| `--steps`/`--cfg`/`--sampler`/`--scheduler` | 8 / 2.0 / dpmpp_sde / karras | Lightning 기준 |
| `--size` | 1024 | 정사각 해상도 |
| `--seed` | -1(랜덤) | 고정 시 동일 구도 재현 |
| `--no-bg-removal` | off | 배경 제거 끄기 |
