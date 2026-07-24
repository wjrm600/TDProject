# UI 텍스처 키트 — 콘텐츠(초상화·아이콘) 전용 파이프라인

AI 생성 텍스처로 UI 를 격상하는 파이프라인. **다크·프리미엄 v2 리디자인(2026-07-23 승인)** 이후 **적용 범위가 콘텐츠로 한정**된다.

## ⭐ 핵심 원칙 (v2) — AI = 콘텐츠, 크롬 = 토큰 솔리드

> **AI 생성 텍스처는 "콘텐츠"에만 쓴다. "크롬"은 절대 텍스처로 만들지 않는다.**

- **콘텐츠(AI OK)** = 캐릭터 초상화, 아이템 아이콘 — 고유하고 예측 불가한 시각 자산.
- **크롬(토큰 솔리드)** = 패널·프레임·버튼·구획선·코너 브래킷 — 전부 `AOSUIStyle` 토큰 색의 솔리드 드로잉(`SolidBrush`/`SolidButtonStyle`).
- **이유**: 크롬을 텍스처로 구우면 (1) 9-slice/라운드가 해상도마다 깨지고, (2) 색 토큰 규율이 무너지고, (3) 리테마 시 전량 재생성이 필요하다. 솔리드 크롬은 재컴파일만으로 다크로 리테마된다.
- 방향 상세 → [`Guides/02_Design/ART_DIRECTION.md`](../02_Design/ART_DIRECTION.md).

⚠️ **파일럿 상점 장식 텍스처(`T_UI_Shop_Panel`/`_Button`/`_Divider`/`_Tooltip`)는 제거 대상.** 상점 크롬은 토큰 솔리드로 교체된다(→ `Guides/02_Design/UI_Specs/Shop.md` 스타일 계약). `KitBrush`/`KitButtonStyle` 는 폐기하지 않되, **콘텐츠 이미지(초상화/아이콘) 전용**으로만 남긴다.

## 스타일 (콘텐츠 기준)

콘텐츠 자산(초상화/아이콘)은 다크·프리미엄 지반 위에서 읽혀야 한다:
- 아이콘 = 어두운 패널 위 대비 확보(밝은 실루엣/림), 투명 배경 위 중앙 정렬.
- 초상화 = 카드 슬롯 비율에 맞춘 크롭. 없으면 `AOSUIStyle::PlaceholderColor(Index)` 폴백.
- 마스터 프롬프트 토큰 = `ui_kit_manifest.json` 의 `style_prefix`(단일 진실). **크롬 컴포넌트(panel9/button3/bar) 항목은 매니페스트에서 신규 추가하지 않는다** — 콘텐츠(icon) 항목만 추가.

## 파이프라인 (콘텐츠 1종 추가 절차)

```
1. ui_kit_manifest.json 에 콘텐츠 항목 추가 (kind=icon/portrait, 크롬 종류 금지)
   { name, kind, size, final_size, prompt, control{...}, post[...], ue_name }
2. ComfyUI 기동 확인 (run_nvidia_gpu.bat → :8188)
3. python gen_ui_kit.py --only <name> --count 3
   → RawAssets/UI_Kit/<name>_<i>.png
4. 후보 판독 (Claude Read or 사용자) → 선정본을 <name>_final.png 로 복사
5. UE 임포트: import_ui_kit.py
   - 에디터 켜짐: MCP execute_script
   - 에디터 꺼짐: UnrealEditor-Cmd.exe <proj> -ExecutePythonScript=<py> -unattended -nosplash
     (⚠️ -run=pythonscript 커맨드릿은 임포트에서 Slate 크래시)
6. 위젯 배선: 콘텐츠 이미지 = AOSUIStyle::KitBrush (아래 계약). 크롬은 SolidBrush.
```

## 파일

| 파일 | 역할 |
|------|------|
| `Mcp_Tools/Asset_Pipeline/ui_kit_manifest.json` | 컴포넌트 명세 + 스타일 토큰 (단일 진실) — **콘텐츠 항목만 신규 추가** |
| `gen_ui_kit.py` | 배치 생성 CLI (ControlNet, `--no-cn` 폴백) |
| `draw_controls.py` | PIL 절차 도면 → 제어 이미지 (크롬 제어는 레거시, 콘텐츠엔 미사용/최소) |
| `ui_postprocess.py` | mask_from_rounded_rect / derive_states 등 |
| `import_ui_kit.py` | [UE 에디터] 배치 임포트 (TEXTUREGROUP_UI/NO_MIPMAPS/TC_EDITOR_ICON + save) |
| `Source/.../AOSUIStyle.h` | `KitBrush`(콘텐츠) / `SolidBrush`·`SolidButtonStyle`(크롬) |

## 배선 계약 (C++)

```cpp
// ── 콘텐츠(초상화/아이콘): 텍스처 있으면 이미지, 없으면 폴백색 ──
PortraitImage->SetBrush(AOSUIStyle::KitBrush(
    TEXT("/Game/AOS/UI/Assets/T_Portrait_Kwang.T_Portrait_Kwang"),
    0.f /* 콘텐츠는 9-slice 아님 → SliceMargin=0 */, AOSUIStyle::PlaceholderColor(0)));

// ── 크롬(패널/버튼/구획선): 토큰 솔리드. 텍스처 배선 금지 ──
PanelBorder->SetBrush(AOSUIStyle::SolidBrush(AOSUIStyle::PanelBase));
Button->SetStyle(AOSUIStyle::SolidButtonStyle(AOSUIStyle::RedCTA)); // 예: 라운드 준비 CTA
```

- **크롬에 `KitBrush`/`KitButtonStyle`(텍스처 경로) 사용 금지** — `SolidBrush`/`SolidButtonStyle`(토큰 색)만.
- **텍스트는 텍스처에 굽지 않는다** — 전부 TextBlock(선명도/tabular 수치).
- 콘텐츠 텍스처 미존재에도 **PIE 항상 동작**(폴백 색/`PlaceholderColor`) — 기존 BanPick 폴백 패턴 유지.

## 원칙 / 함정 (실측)

1. **크롬은 텍스처화하지 않는다** — v2 최상위 원칙. 파일럿에서 만든 크롬 텍스처는 리테마 부채가 됐다(제거 대상).
2. **9-slice 알파에 rembg 금지** — 직선/라운드 엣지를 파먹어 slice 가 깨짐(레거시 크롬 생성 시 실측). 콘텐츠 아이콘은 절차 마스크(`mask_from_rounded_rect`)로 결정적 유도.
3. **validate_nine_slice** = (레거시 크롬용) 중앙 스트레치 표준편차 검사. 콘텐츠 전용으로 전환하며 사용 빈도 감소.
4. ControlNet 모델 = `diffusion_pytorch_model.safetensors`(SDXL scribble), strength 0.85 — 크롬 제어용이었음. 콘텐츠(초상화/아이콘)는 프롬프트 위주 + 최소 제어.
5. ComfyUI 위치: `C:\Users\wjrm7\Downloads\ComfyUI_windows_portable_nvidia\ComfyUI_windows_portable\run_nvidia_gpu.bat`

## 적용 현황 / 백로그 (다크·프리미엄 + WBP 하이브리드 기준)

**크롬 마이그레이션(텍스처 → 토큰 솔리드)**
- 🔄 **상점 크롬 전환**(진행): 파일럿 `T_UI_Shop_*`(Panel/Button×3/Divider/Tooltip) **제거** → `AOSShopWidget` 을 `SolidBrush`/`SolidButtonStyle` + `PanelBorder`(옛 `RootBg`)로 재배선. 계약 → `Guides/02_Design/UI_Specs/Shop.md`.
- 🔄 **준비/배치 크롬**(진행): `AOSCharacterSelectWidget` 을 다크 토큰 솔리드 + `BindWidgetOptional` WBP 하이브리드로. 계약 → `Guides/02_Design/UI_Specs/CharacterSelect.md`.
- ⬜ 벤픽·미니맵·라운드 결과·메인 메뉴 크롬 순차 다크 솔리드 전환.

**콘텐츠 텍스처(AI 유지·확대)**
- ⬜ 캐릭터 초상화(로스터 14 실캐릭터 + 플레이스홀더) — 카드/벤픽/슬롯 콘텐츠.
- ⬜ 아이템 아이콘(`DT_Items` 카탈로그) — 상점 View2 아이템 버튼 콘텐츠.

> 요약: **크롬은 코드(토큰 솔리드)로 내려가고, AI 텍스처 파이프라인은 콘텐츠(초상화/아이콘)로 좁혀 유지**한다.
